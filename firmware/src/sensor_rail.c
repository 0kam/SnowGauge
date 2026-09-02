/* Sensor rail control - see sensor_rail.h */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device.h>
#include <zephyr/logging/log.h>
#include <errno.h>

#include "sensor_rail.h"

LOG_MODULE_REGISTER(sensor_rail, CONFIG_LOG_DEFAULT_LEVEL);

static const struct gpio_dt_spec sensor_en =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), sensor_en_gpios);
static const struct device *const sensor_uart =
	DEVICE_DT_GET(DT_ALIAS(tfmini_uart));

static bool rail_on;

/*
 * Suspending the nRF UARTE applies its "sleep" pinctrl state, which
 * disconnects TX/RX (input buffer off, no pull). That is what makes the
 * rail cut safe: nothing can leak into the powered-down TFmini via R7.
 */
static int uart_park(void)
{
	int ret = pm_device_action_run(sensor_uart, PM_DEVICE_ACTION_SUSPEND);

	if (ret == -EALREADY) {
		ret = 0;
	}
	if (ret) {
		LOG_ERR("UART suspend failed (%d)", ret);
	}
	return ret;
}

static int uart_wake(void)
{
	int ret = pm_device_action_run(sensor_uart, PM_DEVICE_ACTION_RESUME);

	if (ret == -EALREADY) {
		ret = 0;
	}
	if (ret) {
		LOG_ERR("UART resume failed (%d)", ret);
	}
	return ret;
}

int sensor_rail_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&sensor_en)) {
		LOG_ERR("SENSOR_EN GPIO not ready");
		return -ENODEV;
	}
	if (!device_is_ready(sensor_uart)) {
		LOG_ERR("sensor UART not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&sensor_en, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("SENSOR_EN configure failed (%d)", ret);
		return ret;
	}
	rail_on = false;

	/* The UART driver comes up active (TX idle high) - park it right away. */
	return uart_park();
}

int sensor_rail_on(void)
{
	int ret;

	if (rail_on) {
		return 0;
	}

	ret = gpio_pin_set_dt(&sensor_en, 1);
	if (ret) {
		return ret;
	}
	rail_on = true;
	LOG_DBG("rail ON, settling %d ms", CONFIG_SNOWGAUGE_RAIL_SETTLE_MS);
	k_sleep(K_MSEC(CONFIG_SNOWGAUGE_RAIL_SETTLE_MS));

	return uart_wake();
}

int sensor_rail_off(void)
{
	int ret;

	if (!rail_on) {
		return 0;
	}

	/* Order matters: UART pins Hi-Z first, then cut the rail. */
	ret = uart_park();
	(void)gpio_pin_set_dt(&sensor_en, 0);
	rail_on = false;
	LOG_DBG("rail OFF");

	return ret;
}

bool sensor_rail_is_on(void)
{
	return rail_on;
}
