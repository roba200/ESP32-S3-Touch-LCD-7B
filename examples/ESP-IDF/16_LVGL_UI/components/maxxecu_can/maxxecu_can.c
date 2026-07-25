#include "maxxecu_can.h"

#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"

#include "io_extension.h"

static const char *TAG = "MAXXECU";

#define MAXXECU_CAN_TX_GPIO_NUM GPIO_NUM_20
#define MAXXECU_CAN_RX_GPIO_NUM GPIO_NUM_19

// Installs and starts the TWAI driver at 500 kbit/s, accept-all filter,
// normal mode. Kept private to this file so its symbol name can't collide
// with any other CAN/TWAI helper component in the tree.
static esp_err_t maxxecu_twai_bus_init(void)
{
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(MAXXECU_CAN_TX_GPIO_NUM, MAXXECU_CAN_RX_GPIO_NUM, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    err = twai_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Driver start failed: %s", esp_err_to_name(err));
        twai_driver_uninstall();
        return err;
    }

    ESP_LOGI(TAG, "TWAI started: 500 kbit/s, accept-all filter, normal mode");
    return ESP_OK;
}

// MaxxECU standard broadcast frame IDs (11-bit)
#define ID_RPM_MAP_LAMBDA 0x520
#define ID_SPEED 0x522
#define ID_BATT_IAT_COOLANT 0x530
#define ID_GEAR_OILTEMP 0x536

// Rate limit for the decoded summary lines, per CAN ID
#define DECODE_PRINT_PERIOD_TICKS pdMS_TO_TICKS(1000)

static inline int16_t le_i16(const uint8_t *b)
{
    return (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}

static maxxecu_can_rpm_cb_t s_rpm_cb = NULL;
static maxxecu_can_speed_cb_t s_speed_cb = NULL;
static maxxecu_can_lambda_cb_t s_lambda_cb = NULL;
static maxxecu_can_gear_cb_t s_gear_cb = NULL;

void maxxecu_can_set_rpm_callback(maxxecu_can_rpm_cb_t cb)
{
    s_rpm_cb = cb;
}

void maxxecu_can_set_speed_callback(maxxecu_can_speed_cb_t cb)
{
    s_speed_cb = cb;
}

void maxxecu_can_set_lambda_callback(maxxecu_can_lambda_cb_t cb)
{
    s_lambda_cb = cb;
}

void maxxecu_can_set_gear_callback(maxxecu_can_gear_cb_t cb)
{
    s_gear_cb = cb;
}

static void decode_and_print(const twai_message_t *msg)
{
    // One rate-limit slot per known ID, in the same order as the switch below
    static TickType_t last_print_tick[4] = {0};
    TickType_t now = xTaskGetTickCount();

    if (msg->data_length_code < 8)
    {
        return;
    }

    switch (msg->identifier)
    {
    case ID_RPM_MAP_LAMBDA:
    {
        int16_t rpm = le_i16(&msg->data[0]);
        float lambda = le_i16(&msg->data[6]) * 0.001f;

        // Fire every frame (~50 Hz) so UI gauges stay smooth, independent of
        // the once-a-second console print below.
        if (s_rpm_cb)
        {
            s_rpm_cb(rpm);
        }
        if (s_lambda_cb)
        {
            s_lambda_cb(lambda);
        }

        if (now - last_print_tick[0] >= DECODE_PRINT_PERIOD_TICKS)
        {
            float map_kpa = le_i16(&msg->data[4]) * 0.1f;
            printf("  RPM=%d  MAP=%.1f kPa  Lambda=%.3f\n", rpm, map_kpa, lambda);
            last_print_tick[0] = now;
        }
        break;
    }

    case ID_SPEED:
    {
        float speed_kmh = le_i16(&msg->data[6]) * 0.1f;

        if (s_speed_cb)
        {
            s_speed_cb(speed_kmh);
        }

        if (now - last_print_tick[1] >= DECODE_PRINT_PERIOD_TICKS)
        {
            printf("  Speed=%.1f km/h\n", speed_kmh);
            last_print_tick[1] = now;
        }
        break;
    }

    case ID_BATT_IAT_COOLANT:
        if (now - last_print_tick[2] >= DECODE_PRINT_PERIOD_TICKS)
        {
            float battery_v = le_i16(&msg->data[0]) * 0.01f;
            float iat_c = le_i16(&msg->data[4]) * 0.1f;
            float coolant_c = le_i16(&msg->data[6]) * 0.1f;
            printf("  Battery=%.2f V  IAT=%.1f C  Coolant=%.1f C\n", battery_v, iat_c, coolant_c);
            last_print_tick[2] = now;
        }
        break;

    case ID_GEAR_OILTEMP:
    {
        int16_t gear = le_i16(&msg->data[0]);

        if (s_gear_cb)
        {
            s_gear_cb(gear);
        }

        if (now - last_print_tick[3] >= DECODE_PRINT_PERIOD_TICKS)
        {
            float oil_temp_c = le_i16(&msg->data[6]) * 0.1f;
            printf("  Gear=%d  OilTemp=%.1f C\n", gear, oil_temp_c);
            last_print_tick[3] = now;
        }
        break;
    }

    default:
        // Unknown IDs are printed raw only, nothing more to decode.
        break;
    }
}

// Reports link activity once a second regardless of ID, so it's possible to
// tell "no frames arriving" apart from "frames arriving, none matched" while
// bringing the link up.
static void log_link_heartbeat(const twai_message_t *msg)
{
    static uint32_t rx_count = 0;
    static TickType_t last_heartbeat = 0;

    rx_count++;
    TickType_t now = xTaskGetTickCount();
    if (now - last_heartbeat >= pdMS_TO_TICKS(1000))
    {
        ESP_LOGI(TAG, "link alive: rx_count=%lu last_id=0x%03X last_dlc=%d",
                 (unsigned long)rx_count, (unsigned int)msg->identifier, msg->data_length_code);
        last_heartbeat = now;
    }
}

static void maxxecu_can_task(void *arg)
{
    twai_message_t msg;

    while (1)
    {
        if (twai_receive(&msg, pdMS_TO_TICKS(100)) != ESP_OK)
        {
            continue;
        }
        if (msg.rtr)
        {
            continue;
        }

        log_link_heartbeat(&msg);
        decode_and_print(&msg);
    }
}

void maxxecu_can_start(void)
{
    // Select CAN on the board's shared USB/CAN transceiver (0 = USB, 1 = CAN)
    IO_EXTENSION_Output(IO_EXTENSION_IO_5, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    if (maxxecu_twai_bus_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "CAN init failed, MaxxECU logger not started");
        return;
    }

    xTaskCreate(maxxecu_can_task, "maxxecu_can", 4096, NULL, 5, NULL);
}
