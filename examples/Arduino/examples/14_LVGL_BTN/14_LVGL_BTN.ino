/*
* Create a button using LVGL to control an external LED.
*/

#include "lvgl_port.h"       // LVGL porting functions for integration
#include <demos/lv_demos.h>        // LVGL demo headers

#define LED_GPIO_PIN GPIO_NUM_6
/**
 * @brief  Event callback for button interactions
 * @param  *e: A pointer to `lv_event_t` containing event-related data
 * @return None
 */
static void btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e); // Retrieve the event code
    if (code == LV_EVENT_CLICKED) {
        // Toggle the state of the GPIO pin connected to the LED
        digitalWrite(LED_GPIO_PIN, !digitalRead(LED_GPIO_PIN));
    }
}

/**
 * @brief  Creates an LVGL button that toggles an external LED
 * @return None
 */
void lvgl_btn()
{
    // Configure the LED GPIO pin as both input and output
    pinMode(LED_GPIO_PIN, GPIO_MODE_INPUT_OUTPUT);

    // Set the initial LED state to ON
    digitalWrite(LED_GPIO_PIN, 1);

    // Create a button on the active screen
    lv_obj_t * btn = lv_btn_create(lv_scr_act());     
    lv_obj_set_pos(btn, 10, 10);                      // Set the button's position
    lv_obj_set_size(btn, 120, 50);                    // Set the button's size
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);         // Align the button to the center of the screen

    // Add the event callback to handle button actions
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);

    // Create a label and add it to the button
    lv_obj_t * label = lv_label_create(btn);          
    lv_label_set_text(label, "Button");               // Set the label's text
    lv_obj_center(label);                             // Center the label within the button
}

void setup() {
    static esp_lcd_panel_handle_t panel_handle = NULL; // Declare a handle for the LCD panel
    static esp_lcd_touch_handle_t tp_handle = NULL;    // Declare a handle for the touch panel

    // Initialize the GT911 touch screen controller
    tp_handle = touch_gt911_init();  
    
    // Initialize the Waveshare ESP32-S3 RGB LCD hardware
    panel_handle = waveshare_esp32_s3_rgb_lcd_init(); 

    // Turn on the LCD backlight
    // wavesahre_rgb_lcd_bl_on();   

    // Initialize LVGL with the panel and touch handles
    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle));

    ESP_LOGI(TAG, "Display LVGL demos");

    // Lock the mutex because LVGL APIs are not thread-safe
    if (lvgl_port_lock(-1)) {
         lvgl_btn();
        // Release the mutex after the demo execution
        lvgl_port_unlock();
    }
}

void loop() {
  // put your main code here, to run repeatedly:

}
