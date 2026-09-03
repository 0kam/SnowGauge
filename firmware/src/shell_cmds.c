/*
 * Shell commands for bench testing (USB CDC ACM console).
 *
 *   rail on|off|status
 *   tfmini read [n] [timeout_ms]   burst capture -> statistics
 *   tfmini raw [ms]                live frame dump
 *   tfmini rate <hz>               set frame rate (volatile until 'tfmini save')
 *   tfmini save                    persist TFmini settings
 *   batt                           battery voltage (turns the rail on briefly if needed)
 *   tilt [n]                       IMU tilt angle from n averaged samples
 *   dfu                            reboot into the bootloader (serial DFU)
 *   measure                        one full cycle: rail -> batt -> N frames -> batt -> rail off,
 *                                  stored as a record (flag MANUAL)
 *   auto [s]                       automatic measurement period (0 = off)
 *   rec count|ls|dump [n]|hex [n]|erase ERASE   record storage
 *   reboot                         warm reset into the application
 *   time [set <epoch>]             wall clock (UTC epoch seconds)
 *   ble [adv <min_ms> <max_ms>]    advertising status / interval (max 0 = stop)
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/gpio.h>
#include <hal/nrf_gpio.h>
#include <nrfx.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "sensor_rail.h"
#include "tfmini.h"
#include "battery.h"
#include "measure.h"
#include "app.h"
#include "tilt.h"
#include "record.h"
#include "storage.h"
#include "timekeeping.h"
#include "ble_adv.h"
#include "config.h"
#include "cal_gatt.h"

static void shell_out(void *ctx, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	shell_vfprintf((const struct shell *)ctx, SHELL_NORMAL, fmt, ap);
	shell_fprintf((const struct shell *)ctx, SHELL_NORMAL, "\n");
	va_end(ap);
}

/* ---- rail ---- */

static int cmd_rail_on(const struct shell *sh, size_t argc, char **argv)
{
	int ret = sensor_rail_on();

	shell_print(sh, ret ? "rail on failed (%d)" : "rail ON", ret);
	return ret;
}

static int cmd_rail_off(const struct shell *sh, size_t argc, char **argv)
{
	int ret = sensor_rail_off();

	shell_print(sh, ret ? "rail off failed (%d)" : "rail OFF", ret);
	return ret;
}

static int cmd_rail_status(const struct shell *sh, size_t argc, char **argv)
{
	const struct gpio_dt_spec en = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), sensor_en_gpios);
	uint32_t abs_pin = NRF_GPIO_PIN_MAP((en.port == DEVICE_DT_GET(DT_NODELABEL(gpio1))) ? 1 : 0,
					    en.pin);

	shell_print(sh, "rail is %s; SENSOR_EN = P%u.%02u dir=%s out=%u",
		    sensor_rail_is_on() ? "ON" : "OFF",
		    abs_pin >> 5, abs_pin & 0x1F,
		    (nrf_gpio_pin_dir_get(abs_pin) == NRF_GPIO_PIN_DIR_OUTPUT) ? "out" : "in",
		    nrf_gpio_pin_out_read(abs_pin));
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_rail,
	SHELL_CMD(on, NULL, "Sensor rail on (SENSOR_EN high)", cmd_rail_on),
	SHELL_CMD(off, NULL, "Sensor rail off (UART Hi-Z, then SENSOR_EN low)", cmd_rail_off),
	SHELL_CMD(status, NULL, "Show rail state", cmd_rail_status),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(rail, &sub_rail, "Sensor rail control", NULL);

/* ---- tfmini ---- */

static int require_rail(const struct shell *sh)
{
	if (!sensor_rail_is_on()) {
		shell_error(sh, "sensor rail is off - run 'rail on' first");
		return -EBUSY;
	}
	return 0;
}

static int cmd_tfmini_read(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t n = CONFIG_SNOWGAUGE_TFMINI_SAMPLES;
	uint32_t timeout_ms = CONFIG_SNOWGAUGE_TFMINI_CAPTURE_TIMEOUT_MS;
	struct measurement m = { 0 };
	int ret;

	if (require_rail(sh)) {
		return -EBUSY;
	}
	if (argc > 1) {
		n = (uint16_t)strtoul(argv[1], NULL, 0);
	}
	if (argc > 2) {
		timeout_ms = strtoul(argv[2], NULL, 0);
	}

	m.uptime_ms = k_uptime_get();
	ret = tfmini_capture(n, K_MSEC(timeout_ms), &m.lidar);
	if (ret < 0) {
		shell_error(sh, "capture failed (%d)", ret);
		return ret;
	}
	measure_print(&m, shell_out, (void *)sh);
	return 0;
}

static int cmd_tfmini_raw(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t ms = 1000;

	if (require_rail(sh)) {
		return -EBUSY;
	}
	if (argc > 1) {
		ms = strtoul(argv[1], NULL, 0);
	}

	tfmini_flush();
	k_timepoint_t end = sys_timepoint_calc(K_MSEC(ms));

	while (!sys_timepoint_expired(end)) {
		struct tfmini_frame f;

		if (tfmini_read_frame(&f, sys_timepoint_timeout(end)) == 0) {
			shell_print(sh, "dist=%5u cm  str=%5u  temp=%d.%d C",
				    f.dist_cm, f.strength,
				    f.temp_c_x10 / 10, abs(f.temp_c_x10 % 10));
		}
	}
	return 0;
}

static int cmd_tfmini_rate(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t hz = (uint16_t)strtoul(argv[1], NULL, 0);

	if (require_rail(sh)) {
		return -EBUSY;
	}
	shell_print(sh, "set frame rate %u Hz (use 'tfmini save' to persist)", hz);
	return tfmini_set_frame_rate(hz);
}

static int cmd_tfmini_save(const struct shell *sh, size_t argc, char **argv)
{
	if (require_rail(sh)) {
		return -EBUSY;
	}
	shell_print(sh, "saving TFmini settings");
	return tfmini_save_settings();
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_tfmini,
	SHELL_CMD_ARG(read, NULL, "Burst capture: read [n] [timeout_ms]", cmd_tfmini_read, 1, 2),
	SHELL_CMD_ARG(raw, NULL, "Live frame dump: raw [ms]", cmd_tfmini_raw, 1, 1),
	SHELL_CMD_ARG(rate, NULL, "Set frame rate: rate <hz>", cmd_tfmini_rate, 2, 0),
	SHELL_CMD(save, NULL, "Persist TFmini settings", cmd_tfmini_save),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(tfmini, &sub_tfmini, "TFmini Plus LiDAR", NULL);

/* ---- battery ---- */

static int cmd_batt(const struct shell *sh, size_t argc, char **argv)
{
	bool was_on = sensor_rail_is_on();
	uint16_t mv = 0;
	int ret;

	if (!was_on) {
		ret = sensor_rail_on();
		if (ret) {
			shell_error(sh, "rail on failed (%d)", ret);
			return ret;
		}
	}
	ret = battery_read_mv(&mv);
	if (!was_on) {
		(void)sensor_rail_off();
	}
	if (ret) {
		shell_error(sh, "battery read failed (%d)", ret);
		return ret;
	}
	shell_print(sh, "vbat = %u mV (A0 x2%s)", mv, was_on ? "" : ", rail pulsed");
	return 0;
}
SHELL_CMD_REGISTER(batt, NULL, "Battery voltage", cmd_batt);

/* ---- tilt ---- */

static int cmd_tilt(const struct shell *sh, size_t argc, char **argv)
{
	struct tilt_reading r;
	uint8_t n = CONFIG_SNOWGAUGE_TILT_SAMPLES;
	int ret;

	if (argc > 1) {
		n = (uint8_t)strtoul(argv[1], NULL, 0);
	}
	ret = tilt_read(&r, n);
	if (ret) {
		shell_error(sh, "tilt read failed (%d)", ret);
		return ret;
	}
	shell_print(sh, "tilt=%.2f deg  pitch=%.2f  roll=%.2f  a=(%d,%d,%d) mg  temp=%.1f C  n=%u",
		    (double)r.tilt_deg, (double)r.pitch_deg, (double)r.roll_deg,
		    r.ax_mg, r.ay_mg, r.az_mg, (double)r.temp_c, r.n_samples);
	return 0;
}
SHELL_CMD_ARG_REGISTER(tilt, NULL, "IMU tilt: tilt [n]", cmd_tilt, 1, 1);

/* ---- dfu: reboot into the Adafruit bootloader (serial DFU mode) ---- */

#define ADAFRUIT_DFU_MAGIC_SERIAL_ONLY_RESET 0x4E

static int cmd_dfu(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "rebooting into bootloader (serial DFU) ...");
	(void)sensor_rail_off();
	k_sleep(K_MSEC(200));
	NRF_POWER->GPREGRET = ADAFRUIT_DFU_MAGIC_SERIAL_ONLY_RESET;
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}
SHELL_CMD_REGISTER(dfu, NULL, "Reboot into the bootloader for serial DFU", cmd_dfu);

static int cmd_reboot(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "rebooting ...");
	(void)sensor_rail_off();
	k_sleep(K_MSEC(100));
	sys_reboot(SYS_REBOOT_WARM);
	return 0;
}
SHELL_CMD_REGISTER(reboot, NULL, "Warm reset into the application", cmd_reboot);

/* ---- measure / auto ---- */

static int cmd_measure(const struct shell *sh, size_t argc, char **argv)
{
	struct measurement m;
	struct record r;
	int ret = app_measure_and_store(true, &m, &r);

	measure_print(&m, shell_out, (void *)sh);
	if (ret == 0) {
		shell_print(sh, "stored as record %u (%u total)", r.seq, storage_record_count());
		record_print_header(shell_out, (void *)sh);
		record_print(&r, shell_out, (void *)sh);
	} else {
		shell_error(sh, "not stored (%d)", ret);
	}
	return ret;
}
SHELL_CMD_REGISTER(measure, NULL, "One full measurement cycle, stored as a record", cmd_measure);

static int cmd_auto(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 1) {
		app_set_auto_period(strtoul(argv[1], NULL, 0));
	}
	uint32_t s = app_get_auto_period();

	if (s) {
		shell_print(sh, "auto measurement every %u s", s);
	} else {
		shell_print(sh, "auto measurement off");
	}
	return 0;
}
SHELL_CMD_ARG_REGISTER(auto, NULL, "Auto measurement period: auto [s] (0 = off)", cmd_auto, 1, 1);

/* ---- rec: record storage ---- */

static int cmd_rec_count(const struct shell *sh, size_t argc, char **argv)
{
	struct record last;
	char ts[24];

	shell_print(sh, "records=%u  next seq=%u  fs=%s  mirror=%u/%u slots",
		    storage_record_count(), storage_next_seq(),
		    storage_fs_ok() ? "mounted" : "NOT mounted",
		    storage_mirror_count(), storage_mirror_capacity());
	if (storage_last_record(&last) == 0) {
		time_format(last.epoch, ts, sizeof(ts));
		shell_print(sh, "last: seq=%u %s dist=%u cm vbat=%u mV flags=0x%02x",
			    last.seq, ts, last.dist_median_cm, last.vbat_end_mv, last.flags);
	}
	return 0;
}

static int cmd_rec_ls(const struct shell *sh, size_t argc, char **argv)
{
	int ret = storage_list_files(shell_out, (void *)sh);

	if (ret) {
		shell_error(sh, "list failed (%d)", ret);
	}
	return ret;
}

static int cmd_rec_dump(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t n = 10;
	uint32_t total = storage_mirror_count();
	struct record r;

	if (argc > 1) {
		n = strtoul(argv[1], NULL, 0);
	}
	if (n == 0 || n > total) {
		n = total;
	}
	record_print_header(shell_out, (void *)sh);
	for (uint32_t i = total - n; i < total; i++) {
		int ret = storage_mirror_read(i, &r);

		if (ret == 0) {
			record_print(&r, shell_out, (void *)sh);
		} else {
			shell_print(sh, "# slot %u: bad (%d)", i, ret);
		}
	}
	return 0;
}

static int cmd_rec_hex(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t n = 1;
	uint32_t total = storage_mirror_count();
	struct record r;
	uint8_t buf[RECORD_SIZE];
	char line[2 * RECORD_SIZE + 1];

	if (argc > 1) {
		n = strtoul(argv[1], NULL, 0);
	}
	if (n == 0 || n > total) {
		n = total;
	}
	for (uint32_t i = total - n; i < total; i++) {
		if (storage_mirror_read(i, &r) != 0) {
			shell_print(sh, "# slot %u: bad", i);
			continue;
		}
		record_encode(&r, buf);
		for (int b = 0; b < RECORD_SIZE; b++) {
			snprintf(&line[2 * b], 3, "%02x", buf[b]);
		}
		shell_print(sh, "%s", line);
	}
	return 0;
}

static int cmd_rec_erase(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	if (argc < 2 || strcmp(argv[1], "ERASE") != 0) {
		shell_error(sh, "refusing: type 'rec erase ERASE' to delete all records");
		return -EINVAL;
	}
	ret = storage_erase_all();
	shell_print(sh, ret ? "erase failed (%d)" : "all records erased", ret);
	return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_rec,
	SHELL_CMD(count, NULL, "Record count and storage state", cmd_rec_count),
	SHELL_CMD(ls, NULL, "List record files on LittleFS", cmd_rec_ls),
	SHELL_CMD_ARG(dump, NULL, "Print the newest n records (CSV): dump [n=10, 0=all]",
		      cmd_rec_dump, 1, 1),
	SHELL_CMD_ARG(hex, NULL, "Newest n records as raw hex (for tools/decode_records.py --hex)",
		      cmd_rec_hex, 1, 1),
	SHELL_CMD_ARG(erase, NULL, "Delete all records: erase ERASE", cmd_rec_erase, 1, 1),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(rec, &sub_rec, "Record storage", NULL);

/* ---- time ---- */

static const char *time_state_str(enum time_state s)
{
	switch (s) {
	case TIME_SYNCED:
		return "synced";
	case TIME_ESTIMATED:
		return "estimated (restored from last record)";
	default:
		return "unset";
	}
}

static int cmd_time(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t epoch = 0;
	char ts[24];

	if (argc == 2 || (argc > 2 && strcmp(argv[1], "set") != 0)) {
		shell_error(sh, "usage: time [set <epoch>]");
		return -EINVAL;
	}
	if (argc > 2) {
		epoch = strtoul(argv[2], NULL, 0);
		if (epoch < 1700000000U) {
			shell_error(sh, "epoch looks wrong (want UTC seconds, e.g. 1780000000)");
			return -EINVAL;
		}
		int ret = time_set(epoch, true);

		if (ret) {
			shell_error(sh, "time set failed (%d)", ret);
			return ret;
		}
	}
	if (time_now(&epoch) == 0) {
		time_format(epoch, ts, sizeof(ts));
		shell_print(sh, "%s (%u) - %s", ts, epoch, time_state_str(time_get_state()));
	} else {
		shell_print(sh, "time unset - 'time set <epoch>' (UTC seconds)");
	}
	return 0;
}
SHELL_CMD_ARG_REGISTER(time, NULL, "Wall clock: time [set <epoch>]", cmd_time, 1, 2);

/* ---- ble ---- */

static int cmd_ble(const struct shell *sh, size_t argc, char **argv)
{
	if (argc >= 2 && strcmp(argv[1], "adv") == 0) {
		uint32_t min_ms = 0, max_ms = 0;

		if (argc == 3) {
			min_ms = max_ms = strtoul(argv[2], NULL, 0);
		} else if (argc == 4) {
			min_ms = strtoul(argv[2], NULL, 0);
			max_ms = strtoul(argv[3], NULL, 0);
		} else {
			shell_error(sh, "usage: ble adv <ms> | ble adv <min_ms> <max_ms> | ble adv 0");
			return -EINVAL;
		}
		int ret = ble_adv_set_interval(min_ms, max_ms);

		if (ret) {
			shell_error(sh, "adv interval failed (%d)", ret);
			return ret;
		}
	} else if (argc != 1) {
		shell_error(sh, "usage: ble [adv ...]");
		return -EINVAL;
	}
	ble_adv_status(shell_out, (void *)sh);
	return 0;
}
SHELL_CMD_ARG_REGISTER(ble, NULL, "BLE: ble [adv <min_ms> [max_ms]]", cmd_ble, 1, 3);

/* ---- cfg: schedule / calibration settings ---- */

static int cmd_cfg_show(const struct shell *sh, size_t argc, char **argv)
{
	struct app_config c;
	uint32_t now = 0, next;
	char ts[24];

	config_get(&c);
	shell_print(sh, "schedule: %02u:%02u-%02u:%02u local, every %u min, tz %+d min%s",
		    c.sched_start_min / 60, c.sched_start_min % 60, c.sched_end_min / 60,
		    c.sched_end_min % 60, c.sched_interval_min, c.tz_min,
		    c.sched_interval_min ? "" : " (OFF)");
	shell_print(sh, "cal: d0=%u cm theta0=%d.%02d deg set_epoch=%u%s", c.cal_d0_cm,
		    c.cal_theta0_cdeg / 100, abs(c.cal_theta0_cdeg % 100), c.cal_set_epoch,
		    c.cal_d0_cm ? "" : " (not set)");
	if (time_now(&now) == 0 && (next = config_next_measurement(now)) != 0) {
		time_format(next, ts, sizeof(ts));
		shell_print(sh, "next scheduled measurement: %s (in %u s)", ts, next - now);
	} else {
		shell_print(sh, "next scheduled measurement: none (clock unset or scheduler off)");
	}
	if (app_get_auto_period()) {
		shell_print(sh, "bench auto period %u s overrides the schedule", app_get_auto_period());
	}
	return 0;
}

static int parse_hhmm(const char *s, uint16_t *min)
{
	unsigned int h, m;

	if (sscanf(s, "%u:%u", &h, &m) != 2 || h > 23 || m > 59) {
		return -EINVAL;
	}
	*min = (uint16_t)(h * 60 + m);
	return 0;
}

static int cmd_cfg_sched(const struct shell *sh, size_t argc, char **argv)
{
	struct app_config c;

	config_get(&c);
	if (parse_hhmm(argv[1], &c.sched_start_min) || parse_hhmm(argv[2], &c.sched_end_min)) {
		shell_error(sh, "usage: cfg sched <HH:MM> <HH:MM> <interval_min> [tz_min]");
		return -EINVAL;
	}
	c.sched_interval_min = (uint16_t)strtoul(argv[3], NULL, 0);
	if (argc > 4) {
		c.tz_min = (int16_t)strtol(argv[4], NULL, 0);
	}
	int ret = config_set(&c);

	if (ret) {
		shell_error(sh, "config save failed (%d)", ret);
		return ret;
	}
	return cmd_cfg_show(sh, 1, argv);
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_cfg,
	SHELL_CMD(show, NULL, "Show schedule and calibration", cmd_cfg_show),
	SHELL_CMD_ARG(sched, NULL, "Set schedule: sched <HH:MM> <HH:MM> <interval_min> [tz_min]",
		      cmd_cfg_sched, 4, 1),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(cfg, &sub_cfg, "Configuration (settings)", NULL);

/* ---- cal ---- */

static int cmd_cal_zero(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t d0;
	int16_t th;
	uint16_t depth = (argc > 1) ? (uint16_t)strtoul(argv[1], NULL, 0) : 0;
	int ret = cal_zero(depth, &d0, &th);

	if (ret) {
		shell_error(sh, "ZERO failed (%d)", ret);
		return ret;
	}
	shell_print(sh, "reference stored (depth %u cm): d0=%u cm theta0=%d.%02d deg", depth, d0,
		    th / 100, abs(th % 100));
	return 0;
}

static int cmd_cal_clear(const struct shell *sh, size_t argc, char **argv)
{
	int ret = config_set_cal(0, 0, 0);

	shell_print(sh, ret ? "clear failed (%d)" : "calibration cleared", ret);
	return ret;
}

static int cmd_cal_live(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "off") == 0) {
		cal_live_stop();
	}
	shell_print(sh, "live mode %s (start it from the BLE page)", cal_live_is_on() ? "on" : "off");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_cal,
	SHELL_CMD_ARG(zero, NULL, "Take the reference: zero [current_snow_depth_cm]", cmd_cal_zero, 1, 1),
	SHELL_CMD(clear, NULL, "Clear the ZERO reference", cmd_cal_clear),
	SHELL_CMD_ARG(live, NULL, "Live mode state: live [off]", cmd_cal_live, 1, 1),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(cal, &sub_cal, "Calibration", NULL);
