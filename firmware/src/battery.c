/* Battery voltage measurement - see battery.h */

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <errno.h>

#include "battery.h"
#include "sensor_rail.h"

LOG_MODULE_REGISTER(battery, CONFIG_LOG_DEFAULT_LEVEL);

#define DIVIDER_RATIO 2 /* 1M + 1M */

static const struct adc_dt_spec adc_node =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

int battery_init(void)
{
	int ret;

	if (!adc_is_ready_dt(&adc_node)) {
		LOG_ERR("ADC not ready");
		return -ENODEV;
	}
	ret = adc_channel_setup_dt(&adc_node);
	if (ret) {
		LOG_ERR("ADC channel setup failed (%d)", ret);
	}
	return ret;
}

int battery_read_mv(uint16_t *vbat_mv)
{
	int16_t raw;
	int32_t mv;
	int ret;
	struct adc_sequence seq = {
		.buffer = &raw,
		.buffer_size = sizeof(raw),
	};

	if (!sensor_rail_is_on()) {
		return -EBUSY;
	}

	ret = adc_sequence_init_dt(&adc_node, &seq);
	if (ret) {
		return ret;
	}
	ret = adc_read_dt(&adc_node, &seq);
	if (ret) {
		LOG_ERR("ADC read failed (%d)", ret);
		return ret;
	}

	mv = raw;
	ret = adc_raw_to_millivolts_dt(&adc_node, &mv);
	if (ret) {
		return ret;
	}
	if (mv < 0) {
		mv = 0;
	}
	*vbat_mv = (uint16_t)(mv * DIVIDER_RATIO);
	LOG_DBG("raw=%d node=%d mV vbat=%u mV", raw, mv, *vbat_mv);
	return 0;
}
