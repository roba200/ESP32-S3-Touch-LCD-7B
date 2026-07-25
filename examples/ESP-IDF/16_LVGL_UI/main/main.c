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
    maxxecu_can_start();
}