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

    // The MaxxECU simulator fires its whole fast group (9 frames) and then
    // immediately its whole slow group (13 frames) back-to-back in the same
    // 100ms tick - ~22 frames landing within a few ms. The driver's default
    // queue (5) doesn't have headroom for that burst; one scheduling hiccup
    // on this task during it and the driver silently drops whichever frame
    // arrived at that moment (observed: always the first slow-group frame,
    // 0x530, right at the peak of the burst). Give it real headroom.
    g_config.rx_queue_len = 32;

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

    // So drops/errors show up as a direct log line instead of needing to be
    // inferred from which IDs stop appearing. RX_QUEUE_FULL is the driver's
    // software ring buffer (just resized above); RX_FIFO_OVERRUN is the
    // TWAI peripheral's own small hardware FIFO overrunning if its ISR
    // isn't serviced fast enough - a separate failure mode a bigger SW
    // queue can't fix. BUS_ERROR/ERR_PASS catch wire-level corruption.
    uint32_t alerts_to_enable = TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_RX_FIFO_OVERRUN |
                                 TWAI_ALERT_BUS_ERROR | TWAI_ALERT_ERR_PASS;
    if (twai_reconfigure_alerts(alerts_to_enable, NULL) != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to enable TWAI alerts");
    }

    ESP_LOGI(TAG, "TWAI started: 500 kbit/s, accept-all filter, normal mode, rx_queue_len=%lu",
             (unsigned long)g_config.rx_queue_len);
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
static maxxecu_can_map_cb_t s_map_cb = NULL;
static maxxecu_can_battery_cb_t s_battery_cb = NULL;
static maxxecu_can_iat_cb_t s_iat_cb = NULL;
static maxxecu_can_coolant_cb_t s_coolant_cb = NULL;
static maxxecu_can_oil_temp_cb_t s_oil_temp_cb = NULL;

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

void maxxecu_can_set_map_callback(maxxecu_can_map_cb_t cb)
{
    s_map_cb = cb;
}

void maxxecu_can_set_battery_callback(maxxecu_can_battery_cb_t cb)
{
    s_battery_cb = cb;
}

void maxxecu_can_set_iat_callback(maxxecu_can_iat_cb_t cb)
{
    s_iat_cb = cb;
}

void maxxecu_can_set_coolant_callback(maxxecu_can_coolant_cb_t cb)
{
    s_coolant_cb = cb;
}

void maxxecu_can_set_oil_temp_callback(maxxecu_can_oil_temp_cb_t cb)
{
    s_oil_temp_cb = cb;
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
        float map_kpa = le_i16(&msg->data[4]) * 0.1f;
        float lambda = le_i16(&msg->data[6]) * 0.001f;

        // Fire every frame (~50 Hz) so UI gauges stay smooth, independent of
        // the once-a-second console print below.
        if (s_rpm_cb)
        {
            s_rpm_cb(rpm);
        }
        if (s_map_cb)
        {
            s_map_cb(map_kpa);
        }
        if (s_lambda_cb)
        {
            s_lambda_cb(lambda);
        }

        if (now - last_print_tick[0] >= DECODE_PRINT_PERIOD_TICKS)
        {
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
    {
        float battery_v = le_i16(&msg->data[0]) * 0.01f;
        float iat_c = le_i16(&msg->data[4]) * 0.1f;
        float coolant_c = le_i16(&msg->data[6]) * 0.1f;

        if (s_battery_cb)
        {
            s_battery_cb(battery_v);
        }
        if (s_iat_cb)
        {
            s_iat_cb(iat_c);
        }
        if (s_coolant_cb)
        {
            s_coolant_cb(coolant_c);
        }

        if (now - last_print_tick[2] >= DECODE_PRINT_PERIOD_TICKS)
        {
            printf("  Battery=%.2f V  IAT=%.1f C  Coolant=%.1f C\n", battery_v, iat_c, coolant_c);
            last_print_tick[2] = now;
        }
        break;
    }

    case ID_GEAR_OILTEMP:
    {
        int16_t gear = le_i16(&msg->data[0]);
        float oil_temp_c = le_i16(&msg->data[6]) * 0.1f;

        if (s_gear_cb)
        {
            s_gear_cb(gear);
        }
        if (s_oil_temp_cb)
        {
            s_oil_temp_cb(oil_temp_c);
        }

        if (now - last_print_tick[3] >= DECODE_PRINT_PERIOD_TICKS)
        {
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

static void check_twai_alerts(void)
{
    static uint32_t rx_queue_full_count = 0;
    static uint32_t rx_fifo_overrun_count = 0;
    static uint32_t bus_error_count = 0;
    uint32_t alerts = 0;

    // Non-blocking: this only drains alerts already latched by the driver,
    // it doesn't wait for one.
    if (twai_read_alerts(&alerts, 0) != ESP_OK)
    {
        return;
    }

    if (alerts & TWAI_ALERT_RX_QUEUE_FULL)
    {
        rx_queue_full_count++;
        ESP_LOGW(TAG, "driver RX queue overflowed, frame dropped (count=%lu)", (unsigned long)rx_queue_full_count);
    }
    if (alerts & TWAI_ALERT_RX_FIFO_OVERRUN)
    {
        rx_fifo_overrun_count++;
        ESP_LOGW(TAG, "TWAI peripheral RX FIFO overran, frame dropped (count=%lu)",
                 (unsigned long)rx_fifo_overrun_count);
    }
    if (alerts & (TWAI_ALERT_BUS_ERROR | TWAI_ALERT_ERR_PASS))
    {
        bus_error_count++;
        ESP_LOGW(TAG, "bus error / error-passive state (count=%lu)", (unsigned long)bus_error_count);
    }
}

static void maxxecu_can_task(void *arg)
{
    twai_message_t msg;

    while (1)
    {
        check_twai_alerts();

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
