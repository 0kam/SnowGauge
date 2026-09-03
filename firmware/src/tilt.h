/*
 * TiltSensor abstraction (spec section 12.1).
 *
 * The application only depends on this interface; the on-board
 * LSM6DS3TR-C implementation lives in tilt_lsm6dsl.c. A replacement part
 * (external I2C IMU on J3) only needs another implementation file.
 *
 * Axes are the sensor's own. The tilt angle is measured between the
 * gravity vector and the sensor +Z axis (normal to the PCB); the
 * installation ZERO calibration turns it into the laser aiming angle.
 */
#ifndef SNOWGAUGE_TILT_H
#define SNOWGAUGE_TILT_H

#include <stdint.h>

struct tilt_reading {
	int16_t ax_mg, ay_mg, az_mg;  /* averaged acceleration, milli-g */
	float tilt_deg;               /* angle between gravity and +Z, 0 = board horizontal */
	float pitch_deg;              /* rotation about Y (X axis vs. horizontal) */
	float roll_deg;               /* rotation about X (Y axis vs. horizontal) */
	float temp_c;                 /* IMU die temperature */
	uint8_t n_samples;            /* samples actually averaged */
};

/* Probe the sensor and leave it powered down. */
int tilt_init(void);

/*
 * Wake the sensor, average n_samples accelerometer readings, power it
 * down again and fill *r. Returns 0 or a negative errno.
 */
int tilt_read(struct tilt_reading *r, uint8_t n_samples);

/* Force the sensor into its lowest-power state. */
int tilt_power_down(void);

#endif /* SNOWGAUGE_TILT_H */
