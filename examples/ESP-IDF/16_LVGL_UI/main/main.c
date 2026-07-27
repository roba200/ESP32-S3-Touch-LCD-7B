#include <stdio.h>        // snprintf, used to format live channel values
#include <stdbool.h>      // dash_channel_def_t's has_warn_low/high, dash_warning_active

#include "rgb_lcd_port.h" // Header for Waveshare RGB LCD driver
#include "gt911.h"        // Header for touch screen operations (GT911)
#include "lvgl_port.h"    // Header for LVGL port initialization and locking
#include "ui.h"           // Header for user interface initialization
#include "maxxecu_can.h"  // Header for the MaxxECU CAN log stream

static const char *TAG = "main"; // Tag used for ESP log output

// How often the UI polls the latest CAN values and repaints. Deliberately
// decoupled from the CAN frame rate (see dash_latest[] below) - a widget
// update this doesn't need to run at 50 Hz to look smooth.
#define DASH_UI_REFRESH_PERIOD_MS 50

// ---- Unit system ----
// Only pressure channels actually differ between the two systems here (this
// dash's spec sheet uses PSI; MaxxECU broadcasts kPa/bar on the wire) -
// temperature/voltage/etc channels are the same number in both. No Settings
// UI exists yet to flip this at runtime; when one is added, point it at
// this global and everything below (tiles, gauges, warnings-adjacent
// formatting) picks it up automatically since conversion happens once, at
// display time, from the CAN-native value in dash_latest[].
typedef enum {
    DASH_UNITS_METRIC,
    DASH_UNITS_IMPERIAL,
} dash_unit_system_t;

static dash_unit_system_t dash_unit_system = DASH_UNITS_IMPERIAL;

// ---- Channel metadata ----
// One entry per maxxecu_channel_id_t. range_min/max and warn_low/high are
// always in the CAN-native unit (same unit as the value maxxecu_can.c hands
// to dash_on_channel()); imperial_scale converts that native value for
// imperial display (1.0 = no difference between the two systems).
typedef struct {
    const char *short_name;       // compact, log-friendly (e.g. "MAP")
    const char *name;             // spaced-out for tile/gauge labels (e.g. "M A P")
    const char *symbol_metric;
    const char *symbol_imperial;
    const char *fmt;              // printf-style format for the (already-converted) display value
    float range_min;
    float range_max;
    float imperial_scale;
    bool has_warn_low;
    float warn_low;
    bool has_warn_high;
    float warn_high;
} dash_channel_def_t;

// Pressure conversions: 1 kPa = 0.145038 psi, 1 bar = 14.5038 psi. Range/
// warning numbers below were derived by converting this project's PSI spec
// (43/35 psi MAP, 160/120 psi oil & fuel pressure, 100/30 psi coolant
// pressure) through those factors, so the same physical threshold is used
// regardless of which unit system a channel is displayed in.
static const dash_channel_def_t dash_channels[MAXXECU_CH_COUNT] = {
    [MAXXECU_CH_RPM] = {
        .short_name = "RPM", .name = "R P M",
        .symbol_metric = "rpm", .symbol_imperial = "rpm", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 9000.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 8000.0f,
    },
    [MAXXECU_CH_TPS] = {
        .short_name = "TPS", .name = "T P S",
        .symbol_metric = "%", .symbol_imperial = "%", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 100.0f, .imperial_scale = 1.0f,
    },
    [MAXXECU_CH_MAP] = {
        .short_name = "MAP", .name = "M A P",
        .symbol_metric = "kPa", .symbol_imperial = "psi", .fmt = "%.1f",
        .range_min = 0.0f, .range_max = 296.5f, .imperial_scale = 0.145038f,
        .has_warn_high = true, .warn_high = 241.3f,
    },
    [MAXXECU_CH_LAMBDA] = {
        .short_name = "Lambda", .name = "L A M B D A",
        .symbol_metric = "LA", .symbol_imperial = "LA", .fmt = "%.3f",
        .range_min = 0.7f, .range_max = 1.3f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 1.05f,
    },
    [MAXXECU_CH_LAMBDA_A] = {
        .short_name = "LambdaA", .name = "L A M B D A  A",
        .symbol_metric = "LA", .symbol_imperial = "LA", .fmt = "%.3f",
        .range_min = 0.7f, .range_max = 1.3f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 1.05f,
    },
    [MAXXECU_CH_IGNITION_TIMING] = {
        .short_name = "IgnTiming", .name = "I G N  T I M I N G",
        .symbol_metric = "deg", .symbol_imperial = "deg", .fmt = "%.1f",
        .range_min = -10.0f, .range_max = 50.0f, .imperial_scale = 1.0f,
    },
    [MAXXECU_CH_INJ_PW] = {
        .short_name = "InjPW", .name = "I N J  P W",
        .symbol_metric = "ms", .symbol_imperial = "ms", .fmt = "%.2f",
        .range_min = 0.0f, .range_max = 25.0f, .imperial_scale = 1.0f,
    },
    [MAXXECU_CH_INJ_DUTY] = {
        .short_name = "InjDuty", .name = "I N J  D U T Y",
        .symbol_metric = "%", .symbol_imperial = "%", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 120.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 90.0f,
    },
    [MAXXECU_CH_SPEED] = {
        .short_name = "Speed", .name = "S P E E D",
        .symbol_metric = "km/h", .symbol_imperial = "km/h", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 300.0f, .imperial_scale = 1.0f,
    },
    [MAXXECU_CH_ETHANOL] = {
        .short_name = "Ethanol", .name = "E T H A N O L",
        .symbol_metric = "%", .symbol_imperial = "%", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 100.0f, .imperial_scale = 1.0f,
    },
    [MAXXECU_CH_EGT1] = {
        .short_name = "EGT1", .name = "E G T 1",
        .symbol_metric = "\xC2\xB0" "C", .symbol_imperial = "\xC2\xB0" "C", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 1000.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 900.0f,
    },
    [MAXXECU_CH_EGT2] = {
        .short_name = "EGT2", .name = "E G T 2",
        .symbol_metric = "\xC2\xB0" "C", .symbol_imperial = "\xC2\xB0" "C", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 1000.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 900.0f,
    },
    [MAXXECU_CH_EGT3] = {
        .short_name = "EGT3", .name = "E G T 3",
        .symbol_metric = "\xC2\xB0" "C", .symbol_imperial = "\xC2\xB0" "C", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 1000.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 900.0f,
    },
    [MAXXECU_CH_EGT4] = {
        .short_name = "EGT4", .name = "E G T 4",
        .symbol_metric = "\xC2\xB0" "C", .symbol_imperial = "\xC2\xB0" "C", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 1000.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 900.0f,
    },
    [MAXXECU_CH_BATTERY] = {
        .short_name = "Battery", .name = "B A T T E R Y",
        .symbol_metric = "V", .symbol_imperial = "V", .fmt = "%.2f",
        .range_min = 8.0f, .range_max = 16.0f, .imperial_scale = 1.0f,
        .has_warn_low = true, .warn_low = 11.5f, .has_warn_high = true, .warn_high = 15.0f,
    },
    [MAXXECU_CH_IAT] = {
        .short_name = "IAT", .name = "I A T",
        .symbol_metric = "\xC2\xB0" "C", .symbol_imperial = "\xC2\xB0" "C", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 100.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 60.0f,
    },
    [MAXXECU_CH_COOLANT] = {
        .short_name = "Coolant", .name = "C O O L A N T",
        .symbol_metric = "\xC2\xB0" "C", .symbol_imperial = "\xC2\xB0" "C", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 130.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 105.0f,
    },
    [MAXXECU_CH_GEAR] = {
        // Not in dash_cyclable_channels[] - ui_GearShiftValue is a dedicated
        // widget with its own R/N/number text, handled in dash_ui_timer_cb().
        .short_name = "Gear", .name = "G E A R",
        .symbol_metric = "", .symbol_imperial = "", .fmt = "%.0f",
        .range_min = -1.0f, .range_max = 6.0f, .imperial_scale = 1.0f,
    },
    [MAXXECU_CH_OIL_PRESSURE] = {
        .short_name = "OilPress", .name = "O I L  P R E S S",
        .symbol_metric = "bar", .symbol_imperial = "psi", .fmt = "%.1f",
        .range_min = 0.0f, .range_max = 11.03f, .imperial_scale = 14.5038f,
        .has_warn_low = true, .warn_low = 8.27f,
    },
    [MAXXECU_CH_OIL_TEMP] = {
        .short_name = "OilTemp", .name = "O I L  T E M P",
        .symbol_metric = "\xC2\xB0" "C", .symbol_imperial = "\xC2\xB0" "C", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 150.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 120.0f,
    },
    [MAXXECU_CH_FUEL_PRESSURE] = {
        .short_name = "FuelPress", .name = "F U E L  P R E S S",
        .symbol_metric = "bar", .symbol_imperial = "psi", .fmt = "%.1f",
        .range_min = 0.0f, .range_max = 11.03f, .imperial_scale = 14.5038f,
        .has_warn_low = true, .warn_low = 8.27f,
    },
    [MAXXECU_CH_COOLANT_PRESSURE] = {
        .short_name = "CoolantPress", .name = "C O O L A N T  P R E S S",
        .symbol_metric = "bar", .symbol_imperial = "psi", .fmt = "%.1f",
        .range_min = 0.0f, .range_max = 6.89f, .imperial_scale = 14.5038f,
        .has_warn_low = true, .warn_low = 2.07f,
    },
    [MAXXECU_CH_TRANS_TEMP] = {
        .short_name = "TransTemp", .name = "T R A N S  T E M P",
        .symbol_metric = "\xC2\xB0" "C", .symbol_imperial = "\xC2\xB0" "C", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 150.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 120.0f,
    },
    [MAXXECU_CH_DIFF_TEMP] = {
        .short_name = "DiffTemp", .name = "D I F F  T E M P",
        .symbol_metric = "\xC2\xB0" "C", .symbol_imperial = "\xC2\xB0" "C", .fmt = "%.0f",
        .range_min = 0.0f, .range_max = 150.0f, .imperial_scale = 1.0f,
        .has_warn_high = true, .warn_high = 120.0f,
    },
};

// Channels selectable on MainScreen's digital tiles / AnalogClusterScreen's
// bottom gauges. RPM/Speed/Lambda/Gear are deliberately excluded - they
// already have dedicated always-on widgets (ui_RPMSlider, ui_SpeedValue,
// ui_LambdaValue, ui_GearShiftValue plus the big top gauges).
static const maxxecu_channel_id_t dash_cyclable_channels[] = {
    MAXXECU_CH_MAP,        MAXXECU_CH_TPS,           MAXXECU_CH_LAMBDA_A,      MAXXECU_CH_IGNITION_TIMING,
    MAXXECU_CH_INJ_PW,     MAXXECU_CH_INJ_DUTY,      MAXXECU_CH_ETHANOL,       MAXXECU_CH_EGT1,
    MAXXECU_CH_EGT2,       MAXXECU_CH_EGT3,          MAXXECU_CH_EGT4,         MAXXECU_CH_BATTERY,
    MAXXECU_CH_IAT,        MAXXECU_CH_COOLANT,       MAXXECU_CH_OIL_PRESSURE, MAXXECU_CH_OIL_TEMP,
    MAXXECU_CH_FUEL_PRESSURE, MAXXECU_CH_COOLANT_PRESSURE, MAXXECU_CH_TRANS_TEMP, MAXXECU_CH_DIFF_TEMP,
};
#define DASH_CYCLABLE_COUNT (sizeof(dash_cyclable_channels) / sizeof(dash_cyclable_channels[0]))

static uint8_t dash_cyclable_index_of(maxxecu_channel_id_t channel)
{
    for (uint8_t i = 0; i < DASH_CYCLABLE_COUNT; i++)
    {
        if (dash_cyclable_channels[i] == channel)
        {
            return i;
        }
    }
    return 0; // only reachable if a caller passes a channel not in the list
}

// Latest decoded values, written by the CAN RX task's callback and read by
// dash_ui_timer_cb() on the LVGL task. A plain volatile array is fine here:
// single writer, single reader, word-sized elements - no tearing on this
// CPU, and no lock needed. Deliberately NOT touching LVGL from the CAN
// task's callback: lv_slider_set_value()/lv_label_set_text() would need
// lvgl_port_lock(-1), which blocks indefinitely whenever the LVGL task is
// mid-render (an 800x480 RGB panel flush can take a few ms). With several
// channels at 50 Hz, that stalled the CAN task's twai_receive() loop often
// enough to overflow the driver's RX queue and silently drop frames -
// visible as gaps in the raw per-frame log, not just a slow-to-update
// screen. The callback now just stores a value and returns immediately.
static volatile float dash_latest[MAXXECU_CH_COUNT];

// Set true/false (and logged on each transition, not spammed every tick)
// once a channel crosses its warn_low/warn_high. Not wired to any UI action
// yet - WarningScreen integration is a deliberate follow-up, not this pass.
static volatile bool dash_warning_active[MAXXECU_CH_COUNT];

// Set the first time any channel is successfully decoded off the bus - used
// to hold SplashScreen up until a real CAN link exists (see
// start_splash_slider_anim() / dash_ui_timer_cb()) instead of dismissing it
// after a fixed timer regardless of whether the ECU/simulator is connected.
static volatile bool dash_can_link_up = false;

static void dash_on_channel(maxxecu_channel_id_t channel, float value)
{
    dash_latest[channel] = value;
    dash_can_link_up = true;
}

// Converts a native-unit value to whichever system dash_unit_system is
// currently set to and formats it with the channel's fmt string.
static void dash_format_value(maxxecu_channel_id_t channel, float native_value, char *buf, size_t buf_size)
{
    const dash_channel_def_t *def = &dash_channels[channel];
    float display = (dash_unit_system == DASH_UNITS_IMPERIAL) ? native_value * def->imperial_scale : native_value;
    snprintf(buf, buf_size, def->fmt, display);
}

static const char *dash_channel_symbol(maxxecu_channel_id_t channel)
{
    const dash_channel_def_t *def = &dash_channels[channel];
    return (dash_unit_system == DASH_UNITS_IMPERIAL) ? def->symbol_imperial : def->symbol_metric;
}

// Checked once per UI tick from dash_ui_timer_cb(). Native-unit comparison
// throughout, so it's correct regardless of dash_unit_system.
static void dash_warnings_refresh(void)
{
    for (int ch = 0; ch < MAXXECU_CH_COUNT; ch++)
    {
        const dash_channel_def_t *def = &dash_channels[ch];
        float value = dash_latest[ch];
        bool active = (def->has_warn_low && value < def->warn_low) || (def->has_warn_high && value > def->warn_high);

        if (active != dash_warning_active[ch])
        {
            dash_warning_active[ch] = active;
            ESP_LOGW(TAG, "%s: %s (value=%.3f)", def->short_name, active ? "WARNING" : "cleared", value);
        }
    }
}

// ---- MainScreen's 4 tappable "digital channel" tiles ----
//
// Each ui_DigitalChannelN is a ui_ParaTile component (Name / Value / Symbol
// labels - see ui_comp_paratile.c) with no fixed meaning of its own.
// Tapping one advances it to the next channel in dash_cyclable_channels[];
// the value shown keeps refreshing every tick regardless of which channel
// is currently selected.
typedef struct {
    lv_obj_t *panel;       // the tile's clickable Panel2 child - see below
    lv_obj_t *name_label;
    lv_obj_t *value_label;
    lv_obj_t *symbol_label;
    uint8_t cyclable_index;
} dash_digital_channel_tile_t;

static dash_digital_channel_tile_t dash_tiles[4];

// Re-formats the currently selected channel's live value. Cheap enough to
// call every UI tick regardless of whether the channel changed.
static void dash_digital_channel_refresh_value(dash_digital_channel_tile_t *tile)
{
    maxxecu_channel_id_t ch = dash_cyclable_channels[tile->cyclable_index];
    char buf[16];

    dash_format_value(ch, dash_latest[ch], buf, sizeof(buf));
    lv_label_set_text(tile->value_label, buf);
}

// Switches the tile to a new channel: sets Name/Symbol (only needed once
// per selection, unlike the value) and refreshes the value immediately.
static void dash_digital_channel_select(dash_digital_channel_tile_t *tile, maxxecu_channel_id_t channel)
{
    tile->cyclable_index = dash_cyclable_index_of(channel);
    lv_label_set_text(tile->name_label, dash_channels[channel].name);
    lv_label_set_text(tile->symbol_label, dash_channel_symbol(channel));
    dash_digital_channel_refresh_value(tile);
}

static void dash_digital_channel_click_cb(lv_event_t *e)
{
    dash_digital_channel_tile_t *tile = lv_event_get_user_data(e);
    uint8_t next = (tile->cyclable_index + 1) % DASH_CYCLABLE_COUNT;
    dash_digital_channel_select(tile, dash_cyclable_channels[next]);
}

// Wires one ui_DigitalChannelN tile to a starting channel and makes it
// tappable. Must run under lvgl_port_lock (called from app_main's locked
// ui_init() block).
static void dash_digital_channel_tile_init(dash_digital_channel_tile_t *tile, lv_obj_t *root,
                                            maxxecu_channel_id_t start_channel)
{
    // ui_ParaTile_create()'s root object is only a 5px border around Panel2,
    // which covers virtually the whole visible tile and is what actually
    // receives the tap - hook the click event there, not on the root.
    tile->panel = ui_comp_get_child(root, UI_COMP_PARATILE_PANEL2);
    tile->name_label = ui_comp_get_child(root, UI_COMP_PARATILE_PANEL2_CONTAINER6_NAME);
    tile->value_label = ui_comp_get_child(root, UI_COMP_PARATILE_PANEL2_CONTAINER6_CONTAINER7_VALUE);
    tile->symbol_label = ui_comp_get_child(root, UI_COMP_PARATILE_PANEL2_CONTAINER6_CONTAINER7_SYMBOL);

    dash_digital_channel_select(tile, start_channel);
    lv_obj_add_event_cb(tile->panel, dash_digital_channel_click_cb, LV_EVENT_CLICKED, tile);
}

static void dash_digital_channels_init(void)
{
    dash_digital_channel_tile_init(&dash_tiles[0], ui_DigitalChannel1, MAXXECU_CH_MAP);
    dash_digital_channel_tile_init(&dash_tiles[1], ui_DigitalChannel2, MAXXECU_CH_BATTERY);
    dash_digital_channel_tile_init(&dash_tiles[2], ui_DigitalChannel3, MAXXECU_CH_IAT);
    dash_digital_channel_tile_init(&dash_tiles[3], ui_DigitalChannel4, MAXXECU_CH_COOLANT);
}

// ---- Screen navigation: each screen's dedicated SettingsBtn -> Settings,
// plus Settings' own 3 buttons back out to each screen (SquareLine
// generated all of these buttons with no click behavior yet).
typedef struct {
    lv_obj_t **screen;
    void (*init)(void);
} dash_nav_target_t;

static void dash_nav_cb(lv_event_t *e)
{
    const dash_nav_target_t *target = lv_event_get_user_data(e);
    _ui_screen_change(target->screen, LV_SCR_LOAD_ANIM_NONE, 500, 0, target->init);
}

static const dash_nav_target_t dash_nav_main = {&ui_MainScreen, ui_MainScreen_screen_init};
static const dash_nav_target_t dash_nav_analog_cluster = {&ui_AnalogClusterScreen, ui_AnalogClusterScreen_screen_init};
static const dash_nav_target_t dash_nav_all_channels = {&ui_AllChannelsScreen, ui_AllChannelsScreen_screen_init};
static const dash_nav_target_t dash_nav_settings = {&ui_SettingsScreen, ui_SettingsScreen_screen_init};

// Wires each screen's dedicated SettingsBtn -> Settings, and Settings' own
// 3 buttons back out to those screens.
static void dash_navigation_init(void)
{
    lv_obj_add_event_cb(ui_SettingsBtn1, dash_nav_cb, LV_EVENT_CLICKED, (void *)&dash_nav_settings);
    lv_obj_add_event_cb(ui_SettingsBtn2, dash_nav_cb, LV_EVENT_CLICKED, (void *)&dash_nav_settings);
    lv_obj_add_event_cb(ui_SettingsBtn3, dash_nav_cb, LV_EVENT_CLICKED, (void *)&dash_nav_settings);
    lv_obj_add_event_cb(ui_SettingsBtn4, dash_nav_cb, LV_EVENT_CLICKED, (void *)&dash_nav_settings);

    lv_obj_add_event_cb(ui_RaceMainBtn, dash_nav_cb, LV_EVENT_CLICKED, (void *)&dash_nav_main);
    lv_obj_add_event_cb(ui_AnalogClusterBtn, dash_nav_cb, LV_EVENT_CLICKED, (void *)&dash_nav_analog_cluster);
    lv_obj_add_event_cb(ui_FullDataBtn, dash_nav_cb, LV_EVENT_CLICKED, (void *)&dash_nav_all_channels);
}

// ---- AnalogClusterScreen ----
//
// Needle images (big_gauge_indicator.png / small_indicator.png) are rotated
// with lv_img_set_angle() (0.1 deg units, clockwise from however the PNG is
// drawn, pivoting on the image center by default). SquareLine's placeholder
// angles are just wherever the needle was last dragged in the editor, not a
// calibrated 0%/100% - these sweep constants are a first-pass guess (a
// typical ~240 degree automotive gauge arc centered on "straight up") and
// will likely need tuning against the real artwork on hardware.
#define ANALOG_NEEDLE_MIN_DEG (-1200)
#define ANALOG_NEEDLE_MAX_DEG 1200

// Shared by the needle gauges below and AllChannelsScreen's progress bars -
// how far value sits between range_min and range_max, clamped to [0,1].
static float dash_fraction_in_range(float value, float range_min, float range_max)
{
    if (value < range_min)
    {
        value = range_min;
    }
    else if (value > range_max)
    {
        value = range_max;
    }
    return (range_max > range_min) ? (value - range_min) / (range_max - range_min) : 0.0f;
}

static int32_t dash_needle_angle(float value, float range_min, float range_max)
{
    float frac = dash_fraction_in_range(value, range_min, range_max);
    return ANALOG_NEEDLE_MIN_DEG + (int32_t)(frac * (ANALOG_NEEDLE_MAX_DEG - ANALOG_NEEDLE_MIN_DEG));
}

// The two big top gauges always show Speed/RPM, plus the pair of RPM bars
// above them. All 4 refreshed every UI tick from dash_ui_timer_cb(). Needle
// sweep uses native-unit range_min/max directly - the unit-system
// conversion factor cancels out of the (value-min)/(max-min) fraction, so
// it only matters for displayed text, not needle physics.
static void dash_analog_top_gauges_refresh(void)
{
    const dash_channel_def_t *speed_def = &dash_channels[MAXXECU_CH_SPEED];
    lv_img_set_angle(ui_SpeedGaugeIndicator,
                      dash_needle_angle(dash_latest[MAXXECU_CH_SPEED], speed_def->range_min, speed_def->range_max));

    float rpm_max = dash_channels[MAXXECU_CH_RPM].range_max;
    int32_t rpm = (int32_t)dash_latest[MAXXECU_CH_RPM];
    if (rpm < 0)
    {
        rpm = 0;
    }
    else if (rpm > (int32_t)rpm_max)
    {
        rpm = (int32_t)rpm_max;
    }
    lv_img_set_angle(ui_RPMIndicator, dash_needle_angle((float)rpm, 0.0f, rpm_max));

    // Horizontal slider: SquareLine left it in RANGE mode with both ends at
    // the midpoint (50/100) - keep the left (fixed) end pinned there and
    // let the right end track RPM, so the fill visibly grows outward from
    // center toward redline instead of filling left-to-right.
    lv_slider_set_value(ui_AnalogRPMSliderHorizontal, 50 + (rpm * 50) / (int32_t)rpm_max, LV_ANIM_OFF);

    // Vertical one is a plain lv_bar (rotated -90 deg purely visually via a
    // style transform) - normal single-ended 0-100 fill.
    lv_bar_set_value(ui_AnalogRPMSlider, (rpm * 100) / (int32_t)rpm_max, LV_ANIM_OFF);
}

// ---- AnalogClusterScreen's 3 tap-cycled small gauges (bottom row) ----
// Same cyclable channel list as MainScreen's digital tiles, just driving a
// needle angle + the existing AnalogGaugeNameN label instead of a text
// value. The gauge face's Low/Mid/High scale labels ("E"/"1/2"/"F"
// placeholders) are re-labeled per channel too, using the channel's own
// range_min/max (through the same unit conversion as the live value) - e.g.
// Battery becomes "8"/"12"/"16" psi->same, Oil Pressure becomes a bar or
// psi range depending on dash_unit_system.
typedef struct {
    lv_obj_t *needle;
    lv_obj_t *name_label;
    lv_obj_t *low_label;
    lv_obj_t *mid_label;
    lv_obj_t *high_label;
    uint8_t cyclable_index;
} dash_analog_gauge_tile_t;

static dash_analog_gauge_tile_t dash_analog_tiles[3];

static void dash_analog_gauge_refresh_value(dash_analog_gauge_tile_t *tile)
{
    maxxecu_channel_id_t ch = dash_cyclable_channels[tile->cyclable_index];
    const dash_channel_def_t *def = &dash_channels[ch];
    lv_img_set_angle(tile->needle, dash_needle_angle(dash_latest[ch], def->range_min, def->range_max));
}

static void dash_analog_gauge_select(dash_analog_gauge_tile_t *tile, maxxecu_channel_id_t channel)
{
    tile->cyclable_index = dash_cyclable_index_of(channel);
    const dash_channel_def_t *def = &dash_channels[channel];
    char buf[16];

    lv_label_set_text(tile->name_label, def->name);

    dash_format_value(channel, def->range_min, buf, sizeof(buf));
    lv_label_set_text(tile->low_label, buf);
    dash_format_value(channel, (def->range_min + def->range_max) / 2.0f, buf, sizeof(buf));
    lv_label_set_text(tile->mid_label, buf);
    dash_format_value(channel, def->range_max, buf, sizeof(buf));
    lv_label_set_text(tile->high_label, buf);

    dash_analog_gauge_refresh_value(tile);
}

static void dash_analog_gauge_click_cb(lv_event_t *e)
{
    dash_analog_gauge_tile_t *tile = lv_event_get_user_data(e);
    uint8_t next = (tile->cyclable_index + 1) % DASH_CYCLABLE_COUNT;
    dash_analog_gauge_select(tile, dash_cyclable_channels[next]);
}

static void dash_analog_gauge_tile_init(dash_analog_gauge_tile_t *tile, lv_obj_t *gauge_img, lv_obj_t *needle,
                                         lv_obj_t *name_label, lv_obj_t *low_label, lv_obj_t *mid_label,
                                         lv_obj_t *high_label, maxxecu_channel_id_t start_channel)
{
    // lv_img objects aren't clickable by default (unlike lv_obj/lv_btn) -
    // needs an explicit flag before LV_EVENT_CLICKED will ever fire.
    lv_obj_add_flag(gauge_img, LV_OBJ_FLAG_CLICKABLE);

    tile->needle = needle;
    tile->name_label = name_label;
    tile->low_label = low_label;
    tile->mid_label = mid_label;
    tile->high_label = high_label;
    dash_analog_gauge_select(tile, start_channel);
    lv_obj_add_event_cb(gauge_img, dash_analog_gauge_click_cb, LV_EVENT_CLICKED, tile);
}

static void dash_analog_cluster_init(void)
{
    dash_analog_gauge_tile_init(&dash_analog_tiles[0], ui_AnalogGauge1, ui_Image24, ui_AnalogGaugeName1,
                                 ui_LowLevel1, ui_MidLevel, ui_HighLevel1, MAXXECU_CH_BATTERY);
    dash_analog_gauge_tile_init(&dash_analog_tiles[1], ui_AnalogGauge2, ui_Image2, ui_AnalogGaugeName2,
                                 ui_LowLevel2, ui_MidLevel1, ui_HighLevel2, MAXXECU_CH_COOLANT);
    dash_analog_gauge_tile_init(&dash_analog_tiles[2], ui_AnalogGauge3, ui_Image4, ui_AnalogGaugeName3,
                                 ui_LowLevel3, ui_MidLevel2, ui_HighLevel3, MAXXECU_CH_OIL_TEMP);
}

// ---- AllChannelsScreen: all 24 channels at once, one fixed row each ----
// Unlike MainScreen's tiles / AnalogCluster's gauges, nothing here is tap-
// cycled - tile N (1-based) is permanently maxxecu_channel_id_t (N-1), i.e.
// enum declaration order, and ui_Container18 (the row-wrapping flex
// container these live in) scrolls to reach whatever doesn't fit on one
// screen. Each row's Bar is a plain 0-100 lv_bar showing the value's
// position within that channel's range_min/range_max.
typedef struct {
    lv_obj_t *label;
    lv_obj_t *value;
    lv_obj_t *symbol;
    lv_obj_t *bar;
} dash_all_channels_tile_t;

static dash_all_channels_tile_t dash_all_tiles[MAXXECU_CH_COUNT];

static void dash_all_channels_init(void)
{
    lv_obj_t *labels[MAXXECU_CH_COUNT] = {
        ui_AuxChannelLabel1,  ui_AuxChannelLabel2,  ui_AuxChannelLabel3,  ui_AuxChannelLabel4,
        ui_AuxChannelLabel5,  ui_AuxChannelLabel6,  ui_AuxChannelLabel7,  ui_AuxChannelLabel8,
        ui_AuxChannelLabel9,  ui_AuxChannelLabel10, ui_AuxChannelLabel11, ui_AuxChannelLabel12,
        ui_AuxChannelLabel13, ui_AuxChannelLabel14, ui_AuxChannelLabel15, ui_AuxChannelLabel16,
        ui_AuxChannelLabel17, ui_AuxChannelLabel18, ui_AuxChannelLabel19, ui_AuxChannelLabel20,
        ui_AuxChannelLabel21, ui_AuxChannelLabel22, ui_AuxChannelLabel23, ui_AuxChannelLabel24,
    };
    lv_obj_t *values[MAXXECU_CH_COUNT] = {
        ui_AuxChannelValue1,  ui_AuxChannelValue2,  ui_AuxChannelValue3,  ui_AuxChannelValue4,
        ui_AuxChannelValue5,  ui_AuxChannelValue6,  ui_AuxChannelValue7,  ui_AuxChannelValue8,
        ui_AuxChannelValue9,  ui_AuxChannelValue10, ui_AuxChannelValue11, ui_AuxChannelValue12,
        ui_AuxChannelValue13, ui_AuxChannelValue14, ui_AuxChannelValue15, ui_AuxChannelValue16,
        ui_AuxChannelValue17, ui_AuxChannelValue18, ui_AuxChannelValue19, ui_AuxChannelValue20,
        ui_AuxChannelValue21, ui_AuxChannelValue22, ui_AuxChannelValue23, ui_AuxChannelValue24,
    };
    lv_obj_t *symbols[MAXXECU_CH_COUNT] = {
        ui_AuxChannelSymbol1,  ui_AuxChannelSymbol2,  ui_AuxChannelSymbol3,  ui_AuxChannelSymbol4,
        ui_AuxChannelSymbol5,  ui_AuxChannelSymbol6,  ui_AuxChannelSymbol7,  ui_AuxChannelSymbol8,
        ui_AuxChannelSymbol9,  ui_AuxChannelSymbol10, ui_AuxChannelSymbol11, ui_AuxChannelSymbol12,
        ui_AuxChannelSymbol13, ui_AuxChannelSymbol14, ui_AuxChannelSymbol15, ui_AuxChannelSymbol16,
        ui_AuxChannelSymbol17, ui_AuxChannelSymbol18, ui_AuxChannelSymbol19, ui_AuxChannelSymbol20,
        ui_AuxChannelSymbol21, ui_AuxChannelSymbol22, ui_AuxChannelSymbol23, ui_AuxChannelSymbol24,
    };
    lv_obj_t *bars[MAXXECU_CH_COUNT] = {
        ui_AuxChannelBar1,  ui_AuxChannelBar2,  ui_AuxChannelBar3,  ui_AuxChannelBar4,
        ui_AuxChannelBar5,  ui_AuxChannelBar6,  ui_AuxChannelBar7,  ui_AuxChannelBar8,
        ui_AuxChannelBar9,  ui_AuxChannelBar10, ui_AuxChannelBar11, ui_AuxChannelBar12,
        ui_AuxChannelBar13, ui_AuxChannelBar14, ui_AuxChannelBar15, ui_AuxChannelBar16,
        ui_AuxChannelBar17, ui_AuxChannelBar18, ui_AuxChannelBar19, ui_AuxChannelBar20,
        ui_AuxChannelBar21, ui_AuxChannelBar22, ui_AuxChannelBar23, ui_AuxChannelBar24,
    };

    for (int ch = 0; ch < MAXXECU_CH_COUNT; ch++)
    {
        dash_all_tiles[ch].label = labels[ch];
        dash_all_tiles[ch].value = values[ch];
        dash_all_tiles[ch].symbol = symbols[ch];
        dash_all_tiles[ch].bar = bars[ch];
        lv_label_set_text(labels[ch], dash_channels[ch].name);
        lv_label_set_text(symbols[ch], dash_channel_symbol(ch));
    }
}

static void dash_all_channels_refresh(void)
{
    for (int ch = 0; ch < MAXXECU_CH_COUNT; ch++)
    {
        const dash_channel_def_t *def = &dash_channels[ch];
        char buf[16];

        dash_format_value(ch, dash_latest[ch], buf, sizeof(buf));
        lv_label_set_text(dash_all_tiles[ch].value, buf);

        float frac = dash_fraction_in_range(dash_latest[ch], def->range_min, def->range_max);
        lv_bar_set_value(dash_all_tiles[ch].bar, (int32_t)(frac * 100.0f), LV_ANIM_OFF);
    }
}

// lv_label_set_text_fmt() goes through LVGL's own printf, which has
// LV_SPRINTF_USE_FLOAT off in this project's config - %f prints as a bare
// "f". Format floats with the real libc snprintf instead (dash_format_value
// above uses it too).
//
// Runs as an lv_timer callback, i.e. from inside lv_timer_handler() on the
// LVGL task, which already holds the LVGL port lock while it does so (see
// lvgl_port_task() in lvgl_port.c) - no manual lock/unlock needed here.
static void dash_ui_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    char buf[16];

    // Dismiss SplashScreen the moment a CAN link appears, rather than after
    // a fixed timer. Only fires once - dash_can_link_up never goes back to
    // false, so this can't re-trigger and yank the user back to Splash if
    // they're already navigating around the dash.
    static bool splash_dismissed = false;
    if (!splash_dismissed && dash_can_link_up)
    {
        splash_dismissed = true;
        lv_disp_load_scr(ui_MainScreen);
    }

    float rpm_max = dash_channels[MAXXECU_CH_RPM].range_max;
    int32_t rpm = (int32_t)dash_latest[MAXXECU_CH_RPM];
    if (rpm < 0)
    {
        rpm = 0;
    }
    else if (rpm > (int32_t)rpm_max)
    {
        rpm = (int32_t)rpm_max;
    }
    lv_slider_set_value(ui_RPMSlider, (rpm * 1000) / (int32_t)rpm_max, LV_ANIM_OFF);

    dash_format_value(MAXXECU_CH_SPEED, dash_latest[MAXXECU_CH_SPEED], buf, sizeof(buf));
    lv_label_set_text(ui_SpeedValue, buf);

    dash_format_value(MAXXECU_CH_LAMBDA, dash_latest[MAXXECU_CH_LAMBDA], buf, sizeof(buf));
    lv_label_set_text(ui_LambdaValue, buf);

    int16_t gear = (int16_t)dash_latest[MAXXECU_CH_GEAR];
    if (gear < 0)
    {
        lv_label_set_text(ui_GearShiftValue, "R");
    }
    else if (gear == 0)
    {
        lv_label_set_text(ui_GearShiftValue, "N");
    }
    else
    {
        lv_label_set_text_fmt(ui_GearShiftValue, "%d", gear);
    }

    dash_warnings_refresh();

    // Re-format whichever channel each digital channel tile is currently
    // showing - the value keeps live-updating even between taps.
    for (size_t i = 0; i < sizeof(dash_tiles) / sizeof(dash_tiles[0]); i++)
    {
        dash_digital_channel_refresh_value(&dash_tiles[i]);
    }

    // AnalogClusterScreen: top Speed/RPM gauges + both RPM bars, and
    // whichever channel each of the 3 bottom gauges is currently showing.
    dash_analog_top_gauges_refresh();
    for (size_t i = 0; i < sizeof(dash_analog_tiles) / sizeof(dash_analog_tiles[0]); i++)
    {
        dash_analog_gauge_refresh_value(&dash_analog_tiles[i]);
    }

    // AllChannelsScreen: all 24 rows, every tick.
    dash_all_channels_refresh();
}

static void splash_slider_anim_cb(void *var, int32_t value)
{
    lv_slider_set_value((lv_obj_t *)var, value, LV_ANIM_OFF);
}

// Pulses indefinitely (fill, then drain back, repeat) instead of running
// once and dismissing itself - SplashScreen now stays up for however long
// it takes a CAN link to appear (see dash_ui_timer_cb(), which watches
// dash_can_link_up and switches to MainScreen the moment it goes true), not
// a fixed timer regardless of whether anything is actually connected.
static void start_splash_slider_anim(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui_LoadingSlider);
    lv_anim_set_exec_cb(&a, splash_slider_anim_cb);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 1500);           // fill
    lv_anim_set_playback_time(&a, 1500);  // then drain back down
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

/**
 * @brief Main application function.
 *
 * This function initializes the necessary hardware components such as the touch screen
 * and RGB LCD display, sets up the LVGL library for graphics rendering, and runs
 * the LVGL demo UI.
 *
 * - Initializes the GT911 touch screen controller.
 * - Initializes the Waveshare ESP32-S3 RGB LCD display.
 * - Initializes the LVGL library for graphics rendering.
 * - Runs the LVGL demo UI.
 *
 * @return None
 */
void app_main()
{
    static esp_lcd_panel_handle_t panel_handle = NULL; // Handle for the LCD panel
    static esp_lcd_touch_handle_t tp_handle = NULL;    // Handle for the touch panel

    // Initialize the GT911 touch screen controller
    // This sets up the touch functionality of the screen.
    tp_handle = touch_gt911_init();

    // Initialize the Waveshare ESP32-S3 RGB LCD hardware
    // This prepares the LCD panel for display operations.
    panel_handle = waveshare_esp32_s3_rgb_lcd_init();

    // Turn on the LCD backlight
    // This ensures the display is visible.
    wavesahre_rgb_lcd_bl_on();

    // Initialize the LVGL library, linking it to the LCD and touch panel handles
    // LVGL is a lightweight graphics library used for rendering UI elements.
    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle));

    ESP_LOGI(TAG, "Display LVGL demos");

    // Lock the LVGL port to ensure thread safety during API calls
    // This prevents concurrent access issues when using LVGL functions.
    if (lvgl_port_lock(-1))
    {

        // Initialize the UI components with LVGL (e.g., demo screens, sliders)
        // This sets up the user interface elements using the LVGL library.
        ui_init();
        start_splash_slider_anim();

        // Make MainScreen's 4 digital channel tiles show live data and
        // respond to taps.
        dash_digital_channels_init();

        // AnalogClusterScreen: top Speed/RPM gauges + RPM bars always on,
        // bottom 3 gauges tap-cycled.
        dash_analog_cluster_init();

        // AllChannelsScreen: all 24 channels, one fixed row each.
        dash_all_channels_init();

        // Each screen's SettingsBtn -> Settings, Settings' 3 buttons -> screens.
        dash_navigation_init();

        // Periodic repaint of the live CAN channel widgets, decoupled from
        // the CAN RX task - see dash_latest[] comment for why.
        lv_timer_create(dash_ui_timer_cb, DASH_UI_REFRESH_PERIOD_MS, NULL);

        // Release the mutex after LVGL operations are complete
        // This allows other tasks to access the LVGL port.
        lvgl_port_unlock();
    }

    // Start the MaxxECU CAN log stream (relies on the IO extension / I2C
    // bus already brought up by touch_gt911_init() above).
    maxxecu_can_set_channel_callback(dash_on_channel);
    maxxecu_can_start();
}
