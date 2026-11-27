#ifndef DRILL_CONFIG_H
#define DRILL_CONFIG_H
/* 0 builds commissioning firmware: PA5 pulses suppressed. Set 1 only after
 * measuring the detector offset and verifying gate timing on isolated signals. */
#ifndef MOTOR_OUTPUT_ENABLED
#define MOTOR_OUTPUT_ENABLED 0
#endif
#ifndef DRILL_GATE_TEST
#define DRILL_GATE_TEST 0
#endif
/* GateTest physically drives PA5 but is for an unpowered mains stage only. */
#define GATE_OUTPUT_ACTIVE (MOTOR_OUTPUT_ENABLED || DRILL_GATE_TEST)
/* H11L1 falling edge occurs AFTER voltage zero; measure this offset. */
#define ZERO_EDGE_OFFSET_US 0u
#define GATE_PULSE_US 300u
#define HALL_TIMEOUT_MS 2000u
#define STARTUP_TIMEOUT_MS 3000u
#define OVERSPEED_RPM 1000u
/* Commissioning limits, not certified machine protection. 1 Hall pulse/rev. */
#define SOFT_START_STEP_MS 20u
#define FOREGROUND_TIMEOUT_MS 100u
/* Standard PCF8574 backpack: P0 RS, P1 RW, P2 E, P3 backlight. */
#define LCD_ADDRESS_7BIT 0x27u
#endif
