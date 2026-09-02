/*
 * Sensor rail control (SENSOR_EN on D10 -> Q2 -> Q1 high-side switch).
 *
 * The rail feeds the sensor LDO (5 V for TFmini Plus, 3.3 V for TSD20) and
 * the battery divider. The TFmini UART pins are put in a Hi-Z state before
 * the rail is cut so that the MCU cannot ghost-power the sensor through
 * its RX pin (spec section 12.3).
 */
#ifndef SNOWGAUGE_SENSOR_RAIL_H
#define SNOWGAUGE_SENSOR_RAIL_H

#include <stdbool.h>

/* Configure SENSOR_EN low and park the sensor UART. Call once at boot. */
int sensor_rail_init(void);

/*
 * Drive SENSOR_EN high, wait CONFIG_SNOWGAUGE_RAIL_SETTLE_MS, then resume
 * the sensor UART. Returns 0 or a negative errno.
 */
int sensor_rail_on(void);

/* Suspend the sensor UART (pins Hi-Z) and drive SENSOR_EN low. */
int sensor_rail_off(void);

bool sensor_rail_is_on(void);

#endif /* SNOWGAUGE_SENSOR_RAIL_H */
