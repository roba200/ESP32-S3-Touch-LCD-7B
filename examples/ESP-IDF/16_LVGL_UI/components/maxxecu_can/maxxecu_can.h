/*****************************************************************************
 * | File         :   maxxecu_can.h
 * | Function     :   MaxxECU CAN log stream
 * | Info         :
 * |                 Selects the CAN transceiver, brings up TWAI at
 * |                 500 kbit/s and starts a task that prints every
 * |                 received frame's raw bytes plus decoded values for
 * |                 a handful of common MaxxECU broadcast channels.
 *
 ******************************************************************************/

#ifndef MAXXECU_CAN
#define MAXXECU_CAN

#include <stdint.h>

/**
 * @brief Selects the CAN interface, starts the TWAI driver and spawns the
 *        RX logging task.
 */
void maxxecu_can_start(void);

/**
 * @brief Callback types for live channel values, one per decoded CAN ID.
 *
 * All are called from the CAN RX task context on every frame of their ID
 * (roughly 50 Hz for 0x520/0x522, 10 Hz for 0x530/0x536), not just when a
 * log line is printed. Callers touching LVGL from here must take the LVGL
 * port lock themselves - this component has no UI dependency.
 */
typedef void (*maxxecu_can_rpm_cb_t)(int16_t rpm);          // CAN ID 0x520
typedef void (*maxxecu_can_speed_cb_t)(float speed_kmh);    // CAN ID 0x522
typedef void (*maxxecu_can_lambda_cb_t)(float lambda);      // CAN ID 0x520
typedef void (*maxxecu_can_gear_cb_t)(int16_t gear);        // CAN ID 0x536
typedef void (*maxxecu_can_map_cb_t)(float map_kpa);        // CAN ID 0x520
typedef void (*maxxecu_can_battery_cb_t)(float battery_v);  // CAN ID 0x530
typedef void (*maxxecu_can_iat_cb_t)(float iat_c);          // CAN ID 0x530
typedef void (*maxxecu_can_coolant_cb_t)(float coolant_c);  // CAN ID 0x530
typedef void (*maxxecu_can_oil_temp_cb_t)(float oil_temp_c); // CAN ID 0x536

/**
 * @brief Registers a callback to receive a live channel value. Pass NULL to
 *        unregister. Call before maxxecu_can_start() to avoid missing the
 *        first few frames.
 */
void maxxecu_can_set_rpm_callback(maxxecu_can_rpm_cb_t cb);
void maxxecu_can_set_speed_callback(maxxecu_can_speed_cb_t cb);
void maxxecu_can_set_lambda_callback(maxxecu_can_lambda_cb_t cb);
void maxxecu_can_set_gear_callback(maxxecu_can_gear_cb_t cb);
void maxxecu_can_set_map_callback(maxxecu_can_map_cb_t cb);
void maxxecu_can_set_battery_callback(maxxecu_can_battery_cb_t cb);
void maxxecu_can_set_iat_callback(maxxecu_can_iat_cb_t cb);
void maxxecu_can_set_coolant_callback(maxxecu_can_coolant_cb_t cb);
void maxxecu_can_set_oil_temp_callback(maxxecu_can_oil_temp_cb_t cb);

#endif /* MAXXECU_CAN */
