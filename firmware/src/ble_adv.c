/* BLE advertising - see ble_adv.h */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

#include "ble_adv.h"
#include "storage.h"
#include "timekeeping.h"
#include "record.h"

LOG_MODULE_REGISTER(ble_adv, CONFIG_LOG_DEFAULT_LEVEL);

#define MFG_LEN 16

static uint8_t mfg[2 + MFG_LEN];
static char name[CONFIG_BT_DEVICE_NAME_MAX + 1];
static struct bt_le_adv_param param;
static bool advertising;
static bool connected;
static uint32_t cur_min_ms, cur_max_ms;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg, sizeof(mfg)),
};

static struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, name, 0), /* length filled at init */
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, SMP_BT_SVC_UUID_VAL),
};

static void build_payload(bool last_failed)
{
	struct record last;
	bool have_last = (storage_last_record(&last) == 0);
	uint8_t flags = 0;
	int64_t up_h = k_uptime_get() / (3600LL * 1000LL);

	switch (time_get_state()) {
	case TIME_SYNCED:
		flags |= BLE_ADV_FLAG_TIME_SYNCED;
		break;
	case TIME_ESTIMATED:
		flags |= BLE_ADV_FLAG_TIME_ESTIMATED;
		break;
	default:
		break;
	}
	if (have_last && (last.flags & RECORD_FLAG_LIDAR_OK)) {
		flags |= BLE_ADV_FLAG_LAST_LIDAR_OK;
	}
	if (storage_fs_ok()) {
		flags |= BLE_ADV_FLAG_FS_OK;
	}
	if (storage_mirror_count() >= storage_mirror_capacity()) {
		flags |= BLE_ADV_FLAG_MIRROR_FULL;
	}
	if (last_failed) {
		flags |= BLE_ADV_FLAG_LAST_MEAS_ERR;
	}

	sys_put_le16(BLE_ADV_COMPANY_ID, &mfg[0]);
	uint8_t *p = &mfg[2];

	p[0] = BLE_ADV_PAYLOAD_VERSION;
	sys_put_le16(have_last ? last.vbat_end_mv : 0, &p[1]);
	sys_put_le32(storage_record_count(), &p[3]);
	sys_put_le16(have_last ? last.dist_median_cm : 0xFFFF, &p[7]);
	p[9] = flags;
	sys_put_le32(have_last ? last.epoch : 0, &p[10]);
	sys_put_le16((uint16_t)MIN(up_h, 0xFFFF), &p[14]);
}

static int adv_start(void)
{
	int ret;

	if (cur_max_ms == 0) {
		return 0;
	}
	param.id = BT_ID_DEFAULT;
	param.sid = 0;
	param.secondary_max_skip = 0;
	param.options = BT_LE_ADV_OPT_CONN;
	param.interval_min = (uint32_t)(cur_min_ms * 1000 / 625); /* 0.625 ms units */
	param.interval_max = (uint32_t)(cur_max_ms * 1000 / 625);
	param.peer = NULL;

	ret = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (ret && ret != -EALREADY) {
		LOG_ERR("adv start: %d", ret);
		return ret;
	}
	advertising = true;
	return 0;
}

static void on_connected(struct bt_conn *conn, uint8_t err)
{
	ARG_UNUSED(conn);
	connected = (err == 0);
	LOG_INF("BLE connected (err %u)", err);
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	connected = false;
	LOG_INF("BLE disconnected (reason 0x%02x)", reason);
	/* Legacy advertising resumes by itself; be explicit anyway. */
	(void)adv_start();
}

BT_CONN_CB_DEFINE(conn_cbs) = {
	.connected = on_connected,
	.disconnected = on_disconnected,
};

int ble_adv_init(void)
{
	uint8_t id[8] = { 0 };
	int ret;

	ret = bt_enable(NULL);
	if (ret) {
		LOG_ERR("bt_enable: %d", ret);
		return ret;
	}

	(void)hwinfo_get_device_id(id, sizeof(id));
	snprintf(name, sizeof(name), "SG-%02X%02X", id[6], id[7]);
	ret = bt_set_name(name);
	if (ret) {
		LOG_WRN("bt_set_name: %d", ret);
	}
	sd[0].data_len = strlen(name);

	build_payload(false);
	cur_min_ms = CONFIG_SNOWGAUGE_BLE_ADV_INT_MIN_MS;
	cur_max_ms = CONFIG_SNOWGAUGE_BLE_ADV_INT_MAX_MS;
	ret = adv_start();
	if (ret == 0) {
		LOG_INF("advertising as %s, %u-%u ms", name, cur_min_ms, cur_max_ms);
	}
	return ret;
}

int ble_adv_update(bool last_measurement_failed)
{
	int ret;

	build_payload(last_measurement_failed);
	if (!advertising) {
		return 0;
	}
	ret = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (ret && ret != -EALREADY) {
		LOG_WRN("adv update: %d", ret);
	}
	return ret;
}

int ble_adv_set_interval(uint32_t min_ms, uint32_t max_ms)
{
	int ret;

	if (advertising) {
		ret = bt_le_adv_stop();
		if (ret) {
			LOG_WRN("adv stop: %d", ret);
		}
		advertising = false;
	}
	if (max_ms == 0) {
		cur_min_ms = cur_max_ms = 0;
		return 0;
	}
	if (min_ms < 20 || max_ms > 10240 || min_ms > max_ms) {
		return -EINVAL;
	}
	cur_min_ms = min_ms;
	cur_max_ms = max_ms;
	return adv_start();
}

void ble_adv_status(void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	char hex[2 * sizeof(mfg) + 1];

	for (size_t i = 0; i < sizeof(mfg); i++) {
		snprintf(&hex[2 * i], 3, "%02x", mfg[i]);
	}
	out(ctx, "name=%s  adv=%s (%u-%u ms)  connected=%s", name,
	    advertising ? "on" : "off", cur_min_ms, cur_max_ms, connected ? "yes" : "no");
	out(ctx, "manufacturer data: %s", hex);
}
