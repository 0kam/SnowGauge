/*
 * mcumgr / SMP over BLE (spec section 12.2, layer 1).
 *
 * Standard groups only, usable from the nRF Connect Device Manager app:
 *   fs mgmt : download the record files (/lfs1/rec_YYYYMM.bin)
 *   os mgmt : echo (liveness), datetime get/set (clock sync), reset
 *
 * The datetime commands talk to the `rtc` alias (rtc-emul). The hooks below
 * keep it in step with the uptime-based application clock (timekeeping.c):
 * on SET the application clock is set from the incoming value and marked
 * SYNCED; before GET the emulated RTC is refreshed from the application
 * clock so the phone reads the accurate value.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/sys/timeutil.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/grp/os_mgmt/os_mgmt_callbacks.h>
#include <zephyr/logging/log.h>

#include "timekeeping.h"

LOG_MODULE_REGISTER(smp_mgmt, CONFIG_LOG_DEFAULT_LEVEL);

static enum mgmt_cb_return datetime_cb(uint32_t event, enum mgmt_cb_return prev_status,
				       int32_t *rc, uint16_t *group, bool *abort_more,
				       void *data, size_t data_size)
{
	ARG_UNUSED(prev_status);
	ARG_UNUSED(rc);
	ARG_UNUSED(group);
	ARG_UNUSED(abort_more);

	if (event == MGMT_EVT_OP_OS_MGMT_DATETIME_SET && data_size == sizeof(struct rtc_time)) {
		struct rtc_time *t = data;
		uint32_t epoch = (uint32_t)timeutil_timegm(rtc_time_to_tm(t));
		char ts[24];

		(void)time_set(epoch, true);
		time_format(epoch, ts, sizeof(ts));
		LOG_INF("clock set over SMP: %s", ts);
	} else if (event == MGMT_EVT_OP_OS_MGMT_DATETIME_GET) {
		uint32_t epoch;

		/* Re-seed rtc-emul from the accurate clock right before it is read. */
		if (time_now(&epoch) == 0) {
			enum time_state st = time_get_state();

			(void)time_set(epoch, st == TIME_SYNCED);
		}
	}
	return MGMT_CB_OK;
}

static struct mgmt_callback datetime_hook = {
	.callback = datetime_cb,
	.event_id = MGMT_EVT_OP_OS_MGMT_DATETIME_GET | MGMT_EVT_OP_OS_MGMT_DATETIME_SET,
};

static int smp_mgmt_init(void)
{
	mgmt_callback_register(&datetime_hook);
	return 0;
}

SYS_INIT(smp_mgmt_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
