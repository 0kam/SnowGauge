/*
 * TiltSensor abstraction (spec section 12.1).
 *
 * The application only depends on this interface; the on-board
 * LSM6DS3TR-C implementation lives in tilt_lsm6dsl.c. A replacement part
 * (external I2C IMU on J3) only needs another implementation file.
 *
 * Axes are the sensor's own. The tilt angle is measured between the
 * gravity vector and the LiDAR optical axis expressed in the IMU frame
 * (Kconfig choice SNOWGAUGE_SENSOR_AXIS: -Y for the vertical PCB in the
 * enclosure, +/-Z for a flat board), i.e. it is the laser's deviation from
 * nadir. pitch / roll are the two components of that deviation (see below).
 */
#ifndef SNOWGAUGE_TILT_H
#define SNOWGAUGE_TILT_H

#include <stdint.h>

struct tilt_reading {
	int16_t ax_mg, ay_mg, az_mg;  /* averaged acceleration, milli-g */
	float tilt_deg;               /* angle between gravity and the optical axis, 0 = nadir */
	float pitch_deg;              /* signed component of the deviation toward IMU +X (in-plane, USB side) */
	float roll_deg;               /* signed component toward +Z (-Y axis build: out of plane) / +Y (Z axis build) */
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
