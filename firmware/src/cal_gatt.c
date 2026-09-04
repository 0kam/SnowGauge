/* Calibration GATT service - see cal_gatt.h */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "cal_gatt.h"
#include "app.h"
#include "config.h"
#include "sensor_rail.h"
#include "tfmini.h"
#include "tilt.h"
#include "battery.h"
#include "storage.h"
#include "timekeeping.h"
#include "ble_adv.h"
#include "measure.h"

LOG_MODULE_REGISTER(cal_gatt, CONFIG_LOG_DEFAULT_LEVEL);

#define CAL_UUID(n) BT_UUID_128_ENCODE(0x53470000 + (n), 0x6E63, 0x4D61, 0x8B0C, 0x7A8E9F0B1C2D)
static const struct bt_uuid_128 uuid_svc = BT_UUID_INIT_128(CAL_UUID(1));
static const struct bt_uuid_128 uuid_live = BT_UUID_INIT_128(CAL_UUID(2));
static const struct bt_uuid_128 uuid_ctrl = BT_UUID_INIT_128(CAL_UUID(3));
static const struct bt_uuid_128 uuid_status = BT_UUID_INIT_128(CAL_UUID(4));

#define CMD_LIVE_OFF 0x00
#define CMD_LIVE_ON  0x01
#define CMD_ZERO     0x10
#define CMD_REF_DEPTH 0x11
#define CMD_ERASE    0x20

static bool live_on;
static bool live_notify_enabled;
static int8_t last_result;
static bool cmd_busy;
static uint8_t cmd_seq;
static uint32_t live_gen;          /* bumped by cal_live_stop() */
static bool records_downloaded;    /* fs mgmt read of rec_*.bin since boot */
static int64_t live_started_ms;
static uint8_t live_buf[12];

/*
 * Own work queue: ZERO (seconds) and ERASE (flash erase, seconds) must not
 * block the system work queue, which the BLE host and SMP transport use.
 */
K_THREAD_STACK_DEFINE(cal_wq_stack, 2048);
static struct k_work_q cal_wq;

static void live_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(live_work, live_work_fn);
static void cmd_work_fn(struct k_work *work);
static K_WORK_DEFINE(cmd_work, cmd_work_fn);
static uint8_t pending_cmd;
static uint16_t pending_depth_cm;

static ssize_t read_status(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	struct app_config c;
	uint8_t st[12];

	config_get(&c);
	sys_put_le16(c.cal_d0_cm, &st[0]);
	sys_put_le16((uint16_t)c.cal_theta0_cdeg, &st[2]);
	sys_put_le32(c.cal_set_epoch, &st[4]);
	st[8] = live_on ? 1 : 0;
	st[9] = (uint8_t)last_result;
	st[10] = cmd_busy ? 1 : 0;
	st[11] = cmd_seq;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, st, sizeof(st));
}

static ssize_t write_ctrl(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
			  uint16_t len, uint16_t offset, uint8_t flags)
{
	const uint8_t *d = buf;

	if (offset != 0 || len < 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	if (cmd_busy) {
		return BT_GATT_ERR(BT_ATT_ERR_PROCEDURE_IN_PROGRESS);
	}
	switch (d[0]) {
	case CMD_LIVE_OFF:
	case CMD_LIVE_ON:
	case CMD_ZERO:
		pending_cmd = d[0];
		pending_depth_cm = 0;
		break;
	case CMD_REF_DEPTH:
		if (len != 3) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		pending_cmd = d[0];
		pending_depth_cm = sys_get_le16(&d[1]);
		break;
	case CMD_ERASE:
		if (len != 6 || memcmp(&d[1], "ERASE", 5) != 0) {
			last_result = -EACCES;
			return BT_GATT_ERR(BT_ATT_ERR_AUTHORIZATION);
		}
		if (!records_downloaded && storage_record_count() > 0) {
			LOG_WRN("ERASE refused: records not downloaded since boot");
			last_result = -EACCES;
			return BT_GATT_ERR(BT_ATT_ERR_AUTHORIZATION);
		}
		pending_cmd = d[0];
		break;
	default:
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	cmd_busy = true;
	cmd_seq++;
	last_result = 0;
	k_work_submit_to_queue(&cal_wq, &cmd_work);
	return len;
}

static void live_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	live_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

BT_GATT_SERVICE_DEFINE(cal_svc,
	BT_GATT_PRIMARY_SERVICE(&uuid_svc),
	BT_GATT_CHARACTERISTIC(&uuid_live.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL,
			       live_buf),
	BT_GATT_CCC(live_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(&uuid_ctrl.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL,
			       write_ctrl, NULL),
	BT_GATT_CHARACTERISTIC(&uuid_status.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_status, NULL, NULL),
);

/* attribute index of the live characteristic value (service, chrc decl, value) */
#define LIVE_ATTR (&cal_svc.attrs[2])

static void live_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	struct tfmini_stats s;
	struct tilt_reading t;
	uint16_t vbat = 0;
	int n;

	if (!live_on) {
		return;
	}
	if (k_uptime_get() - live_started_ms > CAL_LIVE_TIMEOUT_S * 1000LL) {
		LOG_WRN("live mode timeout");
		cal_live_stop();
		return;
	}
	/* A scheduled measurement owns the sensors: skip this sample. */
	if (k_mutex_lock(&sensor_lock, K_MSEC(50)) != 0) {
		k_work_schedule_for_queue(&cal_wq, &live_work, K_MSEC(200));
		return;
	}
	if (!sensor_rail_is_on()) {
		(void)sensor_rail_on(); /* a measurement cut the rail meanwhile */
		k_sleep(K_MSEC(CONFIG_SNOWGAUGE_RAIL_SETTLE_MS));
	}
	n = tfmini_capture(10, K_MSEC(400), &s);
	(void)tilt_read(&t, 4);
	(void)battery_read_mv(&vbat);
	k_mutex_unlock(&sensor_lock);

	sys_put_le16((n > 0 && s.n_valid > 0) ? s.dist_median_cm : 0xFFFF, &live_buf[0]);
	sys_put_le16(n > 0 ? s.strength_median : 0, &live_buf[2]);
	sys_put_le16((uint16_t)(int16_t)(t.tilt_deg * 100.0f), &live_buf[4]);
	sys_put_le16(vbat, &live_buf[6]);
	live_buf[8] = (uint8_t)MIN(s.n_valid, 255);
	live_buf[9] = (uint8_t)MIN(s.n_frames, 255);
	sys_put_le16((uint16_t)MIN(s.dist_var_cm2, 65535.0f), &live_buf[10]);

	if (live_notify_enabled) {
		(void)bt_gatt_notify(NULL, LIVE_ATTR, live_buf, sizeof(live_buf));
	}
	k_work_schedule_for_queue(&cal_wq, &live_work, K_MSEC(100));
}

static int live_start(void)
{
	int ret;

	if (live_on) {
		return 0;
	}
	ret = sensor_rail_on();
	if (ret) {
		return ret;
	}
	live_on = true;
	live_started_ms = k_uptime_get();
	k_work_schedule_for_queue(&cal_wq, &live_work, K_MSEC(200));
	LOG_INF("live mode on");
	return 0;
}

void cal_live_stop(void)
{
	if (!live_on) {
		return;
	}
	live_on = false;
	live_gen++;
	k_work_cancel_delayable(&live_work);
	k_mutex_lock(&sensor_lock, K_FOREVER); /* not while a capture is running */
	(void)sensor_rail_off();
	k_mutex_unlock(&sensor_lock);
	LOG_INF("live mode off");
}

bool cal_live_is_on(void)
{
	return live_on;
}

int cal_zero(uint16_t depth_cm, uint16_t *d0_cm, int16_t *theta0_cdeg)
{
	struct measurement m;
	struct record r;
	bool was_live = live_on;
	uint32_t gen;
	uint32_t epoch = 0;
	int ret;

	cal_live_stop();
	gen = live_gen;
	ret = app_measure_and_store(true, &m, &r);
	if (ret) {
		goto out;
	}
	if (!(r.flags & RECORD_FLAG_LIDAR_OK) || !(r.flags & RECORD_FLAG_TILT_OK)) {
		ret = -ENODATA;
		goto out;
	}
	(void)time_now(&epoch);
	/* Reference slant distance to bare ground: d0 = d + depth / cos(tilt). */
	float cos_t = cosf((float)r.tilt_cdeg / 100.0f * 3.14159265f / 180.0f);
	float d0f = (float)r.dist_median_cm + (cos_t > 0.5f ? (float)depth_cm / cos_t : (float)depth_cm);
	uint16_t d0 = (uint16_t)MIN(d0f + 0.5f, 65535.0f);

	ret = config_set_cal(d0, r.tilt_cdeg, epoch);
	if (ret == 0) {
		LOG_INF("reference: d=%u cm depth=%u cm -> d0=%u cm theta0=%d.%02d deg",
			r.dist_median_cm, depth_cm, d0, r.tilt_cdeg / 100, abs(r.tilt_cdeg % 100));
		if (d0_cm) {
			*d0_cm = d0;
		}
		if (theta0_cdeg) {
			*theta0_cdeg = r.tilt_cdeg;
		}
	}
out:
	/* Resume the live view only if it was on, nobody stopped it meanwhile
	 * (disconnect bumps live_gen) and the phone is still connected. */
	if (was_live && gen == live_gen && ble_adv_is_connected()) {
		(void)live_start();
	}
	return ret;
}

static void cmd_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	int ret = 0;

	switch (pending_cmd) {
	case CMD_LIVE_ON:
		ret = live_start();
		break;
	case CMD_LIVE_OFF:
		cal_live_stop();
		break;
	case CMD_ZERO:
	case CMD_REF_DEPTH:
		ret = cal_zero(pending_depth_cm, NULL, NULL);
		break;
	case CMD_ERASE:
		cal_live_stop();
		ret = storage_erase_all();
		(void)ble_adv_update(false);
		LOG_WRN("ERASE over BLE: %d", ret);
		break;
	default:
		ret = -EINVAL;
	}
	last_result = (int8_t)CLAMP(ret, -127, 127);
	cmd_busy = false;
}

void cal_note_records_downloaded(void)
{
	records_downloaded = true;
}

int cal_gatt_init(void)
{
	k_work_queue_start(&cal_wq, cal_wq_stack, K_THREAD_STACK_SIZEOF(cal_wq_stack),
			   K_PRIO_PREEMPT(10), NULL);
	k_thread_name_set(&cal_wq.thread, "cal_wq");
	return 0;
}
