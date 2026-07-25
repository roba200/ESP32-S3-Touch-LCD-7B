#include <stdio.h>        // snprintf, used to format live channel values

#include "rgb_lcd_port.h" // Header for Waveshare RGB LCD driver
#include "gt911.h"        // Header for touch screen operations (GT911)
#include "lvgl_port.h"    // Header for LVGL port initialization and locking
#include "ui.h"           // Header for user interface initialization
#include "maxxecu_can.h"  // Header for the MaxxECU CAN log stream

static const char *TAG = "main"; // Tag used for ESP log output

// Redline used to scale the live RPM value onto ui_RPMSlider's 0-1000 range
// (matches the CAN simulator's redline_rpm; change to your ECU's redline).
#define DASH_REDLINE_RPM 7000

// How often the UI polls the latest CAN values and repaints. Deliberately
// decoupled from the CAN frame rate (see dash_latest_* below) - a widget
// update this doesn't need to run at 50 Hz to look smooth.
#define DASH_UI_REFRESH_PERIOD_MS 50

// Latest decoded values, written by the CAN RX task's callbacks and read by
// dash_ui_timer_cb() on the LVGL task. Plain statics are fine here: single
// writer, single reader, word-sized scalars - no tearing on this CPU, and no
// lock needed. Deliberately NOT touching LVGL from the CAN task's callbacks:
// lv_slider_set_value()/lv_label_set_text() would need lvgl_port_lock(-1),
// which blocks indefinitely whenever the LVGL task is mid-render (an
// 800x480 RGB panel flush can take a few ms). With RPM+Lambda+Speed at
// 50 Hz and Gear at 10 Hz, that stalled the CAN task's twai_receive() loop
// often enough to overflow the driver's RX queue and silently drop frames -
// visible as gaps in the raw per-frame gear log, not just a slow-to-update
// screen. Callbacks now just store a value and return immediately.
static volatile int16_t dash_latest_rpm = 0;
static volatile float dash_latest_speed_kmh = 0.0f;
static volatile float dash_latest_lambda = 0.0f;
static volatile int16_t dash_latest_gear = 0;
static volatile float dash_latest_map_kpa = 0.0f;
static volatile float dash_latest_battery_v = 0.0f;
static volatile float dash_latest_iat_c = 0.0f;
static volatile float dash_latest_coolant_c = 0.0f;
static volatile float dash_latest_oil_temp_c = 0.0f;

static void dash_on_rpm(int16_t rpm)
{
    dash_latest_rpm = rpm;
}

static void dash_on_speed(float speed_kmh)
{
    dash_latest_speed_kmh = speed_kmh;
}

static void dash_on_lambda(float lambda)
{
    dash_latest_lambda = lambda;
}

static void dash_on_gear(int16_t gear)
{
    dash_latest_gear = gear;
}

static void dash_on_map(float map_kpa)
{
    dash_latest_map_kpa = map_kpa;
}

static void dash_on_battery(float battery_v)
{
    dash_latest_battery_v = battery_v;
}

static void dash_on_iat(float iat_c)
{
    dash_latest_iat_c = iat_c;
}

static void dash_on_coolant(float coolant_c)
{
    dash_latest_coolant_c = coolant_c;
}

static void dash_on_oil_temp(float oil_temp_c)
{
    dash_latest_oil_temp_c = oil_temp_c;
}

// ---- MainScreen's 4 tappable "digital channel" tiles ----
//
// Each ui_DigitalChannelN is a ui_ParaTile component (Name / Value / Symbol
// labels - see ui_comp_paratile.c) with no fixed meaning of its own.
// Tapping one advances it to the next channel in dash_channels[]; the value
// shown keeps refreshing every tick regardless of which channel is selected.
//
// Only channels already decoded by maxxecu_can are wired up here. The full
// MaxxECU Default protocol has ~50 more (see chat) - add a callback in
// maxxecu_can.c/.h following the existing pattern, a dash_latest_* here, and
// a row below to make one of those tappable too.
typedef enum {
    DASH_CH_MAP,
    DASH_CH_BATTERY,
    DASH_CH_IAT,
    DASH_CH_COOLANT,
    DASH_CH_OIL_TEMP,
    DASH_CH_COUNT,
} dash_channel_kind_t;

typedef struct {
    const char *name;         // shown in the tile's Name label
    const char *symbol;       // shown in the tile's Symbol (unit) label
    const char *fmt;          // printf-style format for the value
    volatile float *value;    // live value backing this channel
} dash_channel_def_t;

static const dash_channel_def_t dash_channels[DASH_CH_COUNT] = {
    [DASH_CH_MAP] = {"M A P", "kPa", "%.0f", &dash_latest_map_kpa},
    [DASH_CH_BATTERY] = {"B A T T E R Y", "V", "%.2f", &dash_latest_battery_v},
    [DASH_CH_IAT] = {"I A T", "\xC2\xB0" "C", "%.0f", &dash_latest_iat_c},
    [DASH_CH_COOLANT] = {"C O O L A N T", "\xC2\xB0" "C", "%.0f", &dash_latest_coolant_c},
    [DASH_CH_OIL_TEMP] = {"O I L  T E M P", "\xC2\xB0" "C", "%.0f", &dash_latest_oil_temp_c},
};

typedef struct {
    lv_obj_t *panel;       // the tile's clickable Panel2 child - see below
    lv_obj_t *name_label;
    lv_obj_t *value_label;
    lv_obj_t *symbol_label;
    uint8_t channel_index;
} dash_digital_channel_tile_t;

static dash_digital_channel_tile_t dash_tiles[4];

// Re-formats the currently selected channel's live value. Cheap enough to
// call every UI tick regardless of whether the channel changed.
static void dash_digital_channel_refresh_value(dash_digital_channel_tile_t *tile)
{
    const dash_channel_def_t *def = &dash_channels[tile->channel_index];
    char buf[16];

    snprintf(buf, sizeof(buf), def->fmt, *def->value);
    lv_label_set_text(tile->value_label, buf);
}

// Switches the tile to a new channel: sets Name/Symbol (only needed once
// per selection, unlike the value) and refreshes the value immediately.
static void dash_digital_channel_select(dash_digital_channel_tile_t *tile, dash_channel_kind_t channel)
{
    tile->channel_index = channel;
    const dash_channel_def_t *def = &dash_channels[channel];
    lv_label_set_text(tile->name_label, def->name);
    lv_label_set_text(tile->symbol_label, def->symbol);
    dash_digital_channel_refresh_value(tile);
}

static void dash_digital_channel_click_cb(lv_event_t *e)
{
    dash_digital_channel_tile_t *tile = lv_event_get_user_data(e);
    dash_digital_channel_select(tile, (tile->channel_index + 1) % DASH_CH_COUNT);
}

// Wires one ui_DigitalChannelN tile to a starting channel and makes it
// tappable. Must run under lvgl_port_lock (called from app_main's locked
// ui_init() block).
static void dash_digital_channel_tile_init(dash_digital_channel_tile_t *tile, lv_obj_t *root,
                                            dash_channel_kind_t start_channel)
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
    dash_digital_channel_tile_init(&dash_tiles[0], ui_DigitalChannel1, DASH_CH_MAP);
    dash_digital_channel_tile_init(&dash_tiles[1], ui_DigitalChannel2, DASH_CH_BATTERY);
    dash_digital_channel_tile_init(&dash_tiles[2], ui_DigitalChannel3, DASH_CH_IAT);
    dash_digital_channel_tile_init(&dash_tiles[3], ui_DigitalChannel4, DASH_CH_COOLANT);
}

// lv_label_set_text_fmt() goes through LVGL's own printf, which has
// LV_SPRINTF_USE_FLOAT off in this project's config - %f prints as a bare
// "f". Format floats with the real libc snprintf instead.
//
// Runs as an lv_timer callback, i.e. from inside lv_timer_handler() on the
// LVGL task, which already holds the LVGL port lock while it does so (see
// lvgl_port_task() in lvgl_port.c) - no manual lock/unlock needed here.
static void dash_ui_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    char buf[16];

    int16_t rpm = dash_latest_rpm;
    if (rpm < 0)
    {
        rpm = 0;
    }
    else if (rpm > DASH_REDLINE_RPM)
    {
        rpm = DASH_REDLINE_RPM;
    }
    lv_slider_set_value(ui_RPMSlider, ((int32_t)rpm * 1000) / DASH_REDLINE_RPM, LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "%.0f", dash_latest_speed_kmh);
    lv_label_set_text(ui_SpeedValue, buf);

    snprintf(buf, sizeof(buf), "%.2f", dash_latest_lambda);
    lv_label_set_text(ui_LambdaValue, buf);

    int16_t gear = dash_latest_gear;
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

    // Re-format whichever channel each digital channel tile is currently
    // showing - the value keeps live-updating even between taps.
    for (size_t i = 0; i < sizeof(dash_tiles) / sizeof(dash_tiles[0]); i++)
    {
        dash_digital_channel_refresh_value(&dash_tiles[i]);
    }
}

static void splash_slider_anim_cb(void *var, int32_t value)
{
    lv_slider_set_value((lv_obj_t *)var, value, LV_ANIM_OFF);
}

static void splash_slider_anim_ready_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    lv_disp_load_scr(ui_MainScreen);
}

static void start_splash_slider_anim(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui_LoadingSlider);
    lv_anim_set_exec_cb(&a, splash_slider_anim_cb);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 5000); // Fill over 5 seconds
    lv_anim_set_ready_cb(&a, splash_slider_anim_ready_cb);
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

        // Periodic repaint of the live CAN channel widgets, decoupled from
        // the CAN RX task - see dash_ui_timer_cb() for why.
        lv_timer_create(dash_ui_timer_cb, DASH_UI_REFRESH_PERIOD_MS, NULL);

        // Release the mutex after LVGL operations are complete
        // This allows other tasks to access the LVGL port.
        lvgl_port_unlock();
    }

    // Start the MaxxECU CAN log stream (relies on the IO extension / I2C
    // bus already brought up by touch_gt911_init() above).
    maxxecu_can_set_rpm_callback(dash_on_rpm);
    maxxecu_can_set_speed_callback(dash_on_speed);
    maxxecu_can_set_lambda_callback(dash_on_lambda);
    maxxecu_can_set_gear_callback(dash_on_gear);
    maxxecu_can_set_map_callback(dash_on_map);
    maxxecu_can_set_battery_callback(dash_on_battery);
    maxxecu_can_set_iat_callback(dash_on_iat);
    maxxecu_can_set_coolant_callback(dash_on_coolant);
    maxxecu_can_set_oil_temp_callback(dash_on_oil_temp);
    maxxecu_can_start();
}