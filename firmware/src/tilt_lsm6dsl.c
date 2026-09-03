/*
 * TiltSensor implementation for the XIAO nRF52840 Sense on-board
 * LSM6DS3TR-C, through Zephyr's lsm6dsl sensor driver (i2c0).
 *
 * The accelerometer is kept in power-down between readings (ODR = 0).
 * The gyroscope is never enabled.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <math.h>

#include "tilt.h"

LOG_MODULE_REGISTER(tilt, CONFIG_LOG_DEFAULT_LEVEL);

#define IMU_NODE DT_NODELABEL(lsm6ds3tr_c)
#define ACCEL_ODR_HZ 52
#define SETTLE_SAMPLES 3        /* discard the first samples after wake-up */
#define SAMPLE_PERIOD_MS (1000 / ACCEL_ODR_HZ + 1)
#define G_MS2 9.80665f
#define IMU_BOOT_MS 30
#define RAD2DEG 57.2957795f

static const struct device *const imu = DEVICE_DT_GET(IMU_NODE);
/* GPIO regulator (P1.08) feeding the IMU; enabled at boot by the board. */
static const struct device *const imu_reg = DEVICE_DT_GET(DT_PATH(lsm6ds3tr_c_en));

/*
 * With CONFIG_SNOWGAUGE_IMU_POWER_GATE the IMU supply is cut between
 * readings (0 uA instead of the ~3 uA power-down mode). The chip comes back
 * with its register defaults, which match what a burst read needs (FS 2 g,
 * IF_INC on); only the ODR is written before each read.
 */
static int imu_supply(bool on)
{
	int ret;

	if (!IS_ENABLED(CONFIG_SNOWGAUGE_IMU_POWER_GATE)) {
		return 0;
	}
	ret = on ? regulator_enable(imu_reg) : regulator_disable(imu_reg);
	if (ret && ret != -EALREADY) {
		LOG_ERR("IMU supply %s failed (%d)", on ? "on" : "off", ret);
		return ret;
	}
	if (on) {
		k_sleep(K_MSEC(IMU_BOOT_MS));
	}
	return 0;
}

static int set_odr(int hz)
{
	struct sensor_value v = { .val1 = hz, .val2 = 0 };

	return sensor_attr_set(imu, SENSOR_CHAN_ACCEL_XYZ,
			       SENSOR_ATTR_SAMPLING_FREQUENCY, &v);
}

int tilt_init(void)
{
	int ret;

	/* Deferred init (see the board overlay): give the IMU time to boot. */
	k_sleep(K_MSEC(IMU_BOOT_MS));
	ret = device_init(imu);
	if (ret) {
		LOG_ERR("IMU init failed (%d)", ret);
		return ret;
	}
	if (!device_is_ready(imu)) {
		LOG_ERR("IMU not ready");
		return -ENODEV;
	}
	ret = tilt_power_down();
	(void)imu_supply(false);
	return ret;
}

int tilt_power_down(void)
{
	int ret = set_odr(0);

	if (ret) {
		LOG_ERR("IMU power-down failed (%d)", ret);
	}
	return ret;
}

int tilt_read(struct tilt_reading *r, uint8_t n_samples)
{
	int ret;
	float sx = 0.0f, sy = 0.0f, sz = 0.0f, st = 0.0f;
	uint8_t n = 0;

	if (r == NULL || n_samples == 0) {
		return -EINVAL;
	}
	if (!device_is_ready(imu)) {
		return -ENODEV;
	}

	ret = imu_supply(true);
	if (ret) {
		return ret;
	}
	ret = set_odr(ACCEL_ODR_HZ);
	if (ret) {
		LOG_ERR("IMU wake failed (%d)", ret);
		(void)imu_supply(false);
		return ret;
	}
	k_sleep(K_MSEC(SAMPLE_PERIOD_MS * SETTLE_SAMPLES));

	for (uint8_t i = 0; i < n_samples; i++) {
		struct sensor_value acc[3], temp;

		ret = sensor_sample_fetch_chan(imu, SENSOR_CHAN_ACCEL_XYZ);
		if (ret == 0) {
			ret = sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, acc);
		}
		if (ret) {
			LOG_WRN("IMU sample %u failed (%d)", i, ret);
			k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
			continue;
		}
		sx += sensor_value_to_float(&acc[0]);
		sy += sensor_value_to_float(&acc[1]);
		sz += sensor_value_to_float(&acc[2]);
#if IS_ENABLED(CONFIG_LSM6DSL_ENABLE_TEMP)
		if (sensor_sample_fetch_chan(imu, SENSOR_CHAN_DIE_TEMP) == 0 &&
		    sensor_channel_get(imu, SENSOR_CHAN_DIE_TEMP, &temp) == 0) {
			st += sensor_value_to_float(&temp);
		}
#else
		ARG_UNUSED(temp);
#endif
		n++;
		k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
	}

	(void)tilt_power_down();
	(void)imu_supply(false);

	if (n == 0) {
		return -EIO;
	}

	float ax = sx / n / G_MS2, ay = sy / n / G_MS2, az = sz / n / G_MS2;
	float norm = sqrtf(ax * ax + ay * ay + az * az);

	r->n_samples = n;
	r->ax_mg = (int16_t)lroundf(ax * 1000.0f);
	r->ay_mg = (int16_t)lroundf(ay * 1000.0f);
	r->az_mg = (int16_t)lroundf(az * 1000.0f);
	r->temp_c = st / n;
	if (norm < 0.1f) {
		r->tilt_deg = r->pitch_deg = r->roll_deg = NAN;
		return -ERANGE;
	}
	r->tilt_deg = acosf(fabsf(az) / norm) * RAD2DEG;
	r->pitch_deg = atan2f(ax, sqrtf(ay * ay + az * az)) * RAD2DEG;
	r->roll_deg = atan2f(ay, sqrtf(ax * ax + az * az)) * RAD2DEG;

	LOG_DBG("n=%u a=(%d,%d,%d) mg tilt=%.2f pitch=%.2f roll=%.2f T=%.1f",
		n, r->ax_mg, r->ay_mg, r->az_mg, (double)r->tilt_deg,
		(double)r->pitch_deg, (double)r->roll_deg, (double)r->temp_c);
	return 0;
}
