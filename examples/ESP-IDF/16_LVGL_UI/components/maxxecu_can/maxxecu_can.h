/*****************************************************************************
 * | File         :   maxxecu_can.h
 * | Function     :   MaxxECU CAN log stream
 * | Info         :
 * |                 Selects the CAN transceiver, brings up TWAI at
 * |                 500 kbit/s and starts a task that prints every
 * |                 received frame's raw bytes plus decoded values for
 * |                 the MaxxECU Default broadcast channels this dash uses.
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
 * @brief One entry per decoded MaxxECU Default channel. Values are always
 * the raw CAN-native unit (kPa, bar, degC, etc.) - any display-unit
 * conversion (e.g. PSI) is the caller's job, done at format time so both
 * unit systems stay available from the same decoded value.
 */
typedef enum {
    MAXXECU_CH_RPM,              // 0x520 B0, x1
    MAXXECU_CH_TPS,               // 0x520 B2, x0.1 %
    MAXXECU_CH_MAP,                // 0x520 B4, x0.1 kPa
    MAXXECU_CH_LAMBDA,             // 0x520 B6, x0.001 (lambda average)
    MAXXECU_CH_LAMBDA_A,           // 0x521 B0, x0.001 (lambda bank A)
    MAXXECU_CH_IGNITION_TIMING,    // 0x521 B4, x0.1 deg (signed)
    MAXXECU_CH_INJ_PW,             // 0x522 B0, x0.001 ms
    MAXXECU_CH_INJ_DUTY,           // 0x522 B2, x0.1 %
    MAXXECU_CH_SPEED,              // 0x522 B6, x0.1 km/h
    MAXXECU_CH_ETHANOL,            // 0x531 B2, x0.1 %
    MAXXECU_CH_EGT1,               // 0x531 B6, x1 degC (signed)
    MAXXECU_CH_EGT2,               // 0x532 B0, x1 degC (signed)
    MAXXECU_CH_EGT3,               // 0x532 B2, x1 degC (signed)
    MAXXECU_CH_EGT4,               // 0x532 B4, x1 degC (signed)
    MAXXECU_CH_BATTERY,            // 0x530 B0, x0.01 V
    MAXXECU_CH_IAT,                // 0x530 B4, x0.1 degC (signed)
    MAXXECU_CH_COOLANT,            // 0x530 B6, x0.1 degC (signed)
    MAXXECU_CH_GEAR,                // 0x536 B0, x1 (signed)
    MAXXECU_CH_OIL_PRESSURE,       // 0x536 B4, x0.001 bar (signed)
    MAXXECU_CH_OIL_TEMP,           // 0x536 B6, x0.1 degC (signed)
    MAXXECU_CH_FUEL_PRESSURE,      // 0x537 B0, x0.001 bar
    MAXXECU_CH_COOLANT_PRESSURE,   // 0x537 B4, x0.001 bar
    MAXXECU_CH_TRANS_TEMP,         // 0x540 B4, x0.1 degC
    MAXXECU_CH_DIFF_TEMP,          // 0x540 B6, x0.1 degC
    MAXXECU_CH_COUNT,
} maxxecu_channel_id_t;

/**
 * @brief Callback for every decoded channel value.
 *
 * Called from the CAN RX task context every time that channel's frame
 * arrives (roughly 50 Hz for 0x520-0x528, 10 Hz for 0x530-0x542), not just
 * when a log line is printed. Callers touching LVGL from here must take the
 * LVGL port lock themselves - this component has no UI dependency.
 */
typedef void (*maxxecu_can_channel_cb_t)(maxxecu_channel_id_t channel, float value);

/**
 * @brief Registers a callback to receive every live channel value. Pass
 *        NULL to unregister. Call before maxxecu_can_start() to avoid
 *        missing the first few frames.
 */
void maxxecu_can_set_channel_callback(maxxecu_can_channel_cb_t cb);

#endif /* MAXXECU_CAN */
