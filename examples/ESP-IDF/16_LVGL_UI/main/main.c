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

static void dash_on_rpm(int16_t rpm)
{
    if (rpm < 0)
    {
        rpm = 0;
    }
    else if (rpm > DASH_REDLINE_RPM)
    {
        rpm = DASH_REDLINE_RPM;
    }
    int32_t slider_value = ((int32_t)rpm * 1000) / DASH_REDLINE_RPM;

    if (lvgl_port_lock(-1))
    {
        lv_slider_set_value(ui_RPMSlider, slider_value, LV_ANIM_OFF);
        lvgl_port_unlock();
    }
}

// lv_label_set_text_fmt() goes through LVGL's own printf, which has
// LV_SPRINTF_USE_FLOAT off in this project's config - %f prints as a bare
// "f". Format floats with the real libc snprintf instead.

static void dash_on_speed(float speed_kmh)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", speed_kmh);

    if (lvgl_port_lock(-1))
    {
        lv_label_set_text(ui_SpeedValue, buf);
        lvgl_port_unlock();
    }
}

static void dash_on_lambda(float lambda)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", lambda);

    if (lvgl_port_lock(-1))
    {
        lv_label_set_text(ui_LambdaValue, buf);
        lvgl_port_unlock();
    }
}

static void dash_on_gear(int16_t gear)
{
    if (lvgl_port_lock(-1))
    {
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
        lvgl_port_unlock();
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