/* Low-power housekeeping - see power.h */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/logging/log.h>

#include "power.h"

LOG_MODULE_REGISTER(power, CONFIG_LOG_DEFAULT_LEVEL);

static const struct device *const qspi_flash = DEVICE_DT_GET(DT_NODELABEL(p25q16h));
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

int power_init(void)
{
	enum pm_device_state st = PM_DEVICE_STATE_ACTIVE;

	/* LED off (active low): drive the pin inactive. */
	if (gpio_is_ready_dt(&led_green)) {
		(void)gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
	}

	/*
	 * The 2 MB QSPI flash (P25Q16H) draws tens of uA in standby; deep
	 * power-down brings it to ~1 uA. The driver handles this itself through
	 * runtime PM (zephyr,pm-device-runtime-auto in the overlay): it suspends
	 * CONFIG_NORDIC_QSPI_NOR_ACTIVE_DWELL_MS after the last access. Just
	 * report the state here (spec section 7.3).
	 */
	if (!device_is_ready(qspi_flash)) {
		LOG_WRN("QSPI flash not ready");
		return -ENODEV;
	}
	if (!pm_device_runtime_is_enabled(qspi_flash)) {
		LOG_ERR("QSPI flash runtime PM is not enabled - it will not power down");
		return -EINVAL;
	}
	(void)pm_device_state_get(qspi_flash, &st);
	LOG_INF("QSPI flash runtime PM on (state now %s)", pm_device_state_str(st));
	return 0;
}

void led_pulse(uint32_t ms)
{
	if (!gpio_is_ready_dt(&led_green)) {
		return;
	}
	gpio_pin_set_dt(&led_green, 1);
	k_sleep(K_MSEC(ms));
	gpio_pin_set_dt(&led_green, 0);
}
