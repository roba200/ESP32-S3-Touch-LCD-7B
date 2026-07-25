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

#ifndef __MAXXECU_CAN_H
#define __MAXXECU_CAN_H

#include <stdint.h>

/**
 * @brief Selects the CAN interface, starts the TWAI driver and spawns the
 *        RX logging task.
 */
void maxxecu_can_start(void);

/**
 * @brief Callback type for the live RPM value decoded from CAN ID 0x520.
 *
 * Called from the CAN RX task context on every 0x520 frame (roughly 50 Hz),
 * not just when a log line is printed. Callers touching LVGL from here must
 * take the LVGL port lock themselves - this component has no UI dependency.
 */
typedef void (*maxxecu_can_rpm_cb_t)(int16_t rpm);

/**
 * @brief Registers a callback to receive the live RPM value. Pass NULL to
 *        unregister. Call before maxxecu_can_start() to avoid missing the
 *        first few frames.
 */
void maxxecu_can_set_rpm_callback(maxxecu_can_rpm_cb_t cb);

#endif // __MAXXECU_CAN_H
