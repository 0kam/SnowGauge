/* Persistent configuration - see config.h */

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"

LOG_MODULE_REGISTER(config, CONFIG_LOG_DEFAULT_LEVEL);

static struct app_config cfg = {
	.sched_start_min = 0,
	.sched_end_min = 0,
	.sched_interval_min = CONFIG_SNOWGAUGE_SCHED_DEFAULT_INTERVAL_MIN,
	.tz_min = CONFIG_SNOWGAUGE_SCHED_DEFAULT_TZ_MIN,
};
static K_MUTEX_DEFINE(lock);
static void (*change_cb)(void);

struct key_map {
	const char *name;
	void *ptr;
	size_t size;
};

#define KEY(n, f) { n, &cfg.f, sizeof(cfg.f) }
static const struct key_map keys[] = {
	KEY("sched/start_min", sched_start_min),
	KEY("sched/end_min", sched_end_min),
	KEY("sched/interval_min", sched_interval_min),
	KEY("sched/tz_min", tz_min),
	KEY("cal/d0_cm", cal_d0_cm),
	KEY("cal/theta0_cdeg", cal_theta0_cdeg),
	KEY("cal/set_epoch", cal_set_epoch),
};

static const struct key_map *find_key(const char *name)
{
	for (size_t i = 0; i < ARRAY_SIZE(keys); i++) {
		if (strcmp(keys[i].name, name) == 0) {
			return &keys[i];
		}
	}
	return NULL;
}

static bool validate(void)
{
	return cfg.sched_start_min < 1440 && cfg.sched_end_min < 1440 &&
	       cfg.sched_interval_min <= 1440 &&
	       cfg.tz_min >= -14 * 60 && cfg.tz_min <= 14 * 60;
}

static int sg_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	const struct key_map *k = find_key(name);
	uint8_t tmp[8];

	if (!k) {
		return -ENOENT;
	}
	if (len != k->size) {
		LOG_WRN("setting %s: bad length %u (want %u)", name, (unsigned int)len,
			(unsigned int)k->size);
		return -EINVAL;
	}
	if (read_cb(cb_arg, tmp, len) != (ssize_t)len) {
		return -EIO;
	}
	k_mutex_lock(&lock, K_FOREVER);
	struct app_config saved = cfg;

	memcpy(k->ptr, tmp, len);
	if (!validate()) {
		cfg = saved;
		k_mutex_unlock(&lock);
		LOG_WRN("setting %s: value out of range, rejected", name);
		return -EINVAL;
	}
	k_mutex_unlock(&lock);
	if (change_cb) {
		change_cb();
	}
	return 0;
}

static int sg_get(const char *name, char *val, int val_len_max)
{
	const struct key_map *k = find_key(name);

	if (!k) {
		return -ENOENT;
	}
	if (val_len_max < (int)k->size) {
		return -ENOMEM;
	}
	k_mutex_lock(&lock, K_FOREVER);
	memcpy(val, k->ptr, k->size);
	k_mutex_unlock(&lock);
	return (int)k->size;
}

static int sg_export(int (*cb)(const char *name, const void *value, size_t val_len))
{
	char name[40];

	for (size_t i = 0; i < ARRAY_SIZE(keys); i++) {
		snprintk(name, sizeof(name), "sg/%s", keys[i].name);
		(void)cb(name, keys[i].ptr, keys[i].size);
	}
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(sg, "sg", sg_get, sg_set, NULL, sg_export);

int config_init(void)
{
	int ret = settings_subsys_init();

	if (ret) {
		LOG_ERR("settings init: %d", ret);
		return ret;
	}
	ret = settings_load_subtree("sg");
	if (ret) {
		LOG_ERR("settings load: %d", ret);
		return ret;
	}
	LOG_INF("schedule %02u:%02u-%02u:%02u every %u min (tz %+d min); d0=%u cm theta0=%d.%02d deg",
		cfg.sched_start_min / 60, cfg.sched_start_min % 60, cfg.sched_end_min / 60,
		cfg.sched_end_min % 60, cfg.sched_interval_min, cfg.tz_min, cfg.cal_d0_cm,
		cfg.cal_theta0_cdeg / 100, abs(cfg.cal_theta0_cdeg % 100));
	return 0;
}

void config_get(struct app_config *out)
{
	k_mutex_lock(&lock, K_FOREVER);
	*out = cfg;
	k_mutex_unlock(&lock);
}

int config_set(const struct app_config *in)
{
	int ret;

	k_mutex_lock(&lock, K_FOREVER);
	cfg = *in;
	if (!validate()) {
		k_mutex_unlock(&lock);
		return -EINVAL;
	}
	k_mutex_unlock(&lock);
	ret = settings_save_subtree("sg");
	if (change_cb) {
		change_cb();
	}
	return ret;
}

int config_set_cal(uint16_t d0_cm, int16_t theta0_cdeg, uint32_t epoch)
{
	int ret;

	k_mutex_lock(&lock, K_FOREVER);
	cfg.cal_d0_cm = d0_cm;
	cfg.cal_theta0_cdeg = theta0_cdeg;
	cfg.cal_set_epoch = epoch;
	k_mutex_unlock(&lock);
	/*
	 * settings_save_subtree() matches the *handler* name ("sg"), so a deeper
	 * subtree silently saves nothing. Save the three keys explicitly.
	 */
	ret = settings_save_one("sg/cal/d0_cm", &cfg.cal_d0_cm, sizeof(cfg.cal_d0_cm));
	if (ret == 0) {
		ret = settings_save_one("sg/cal/theta0_cdeg", &cfg.cal_theta0_cdeg,
					sizeof(cfg.cal_theta0_cdeg));
	}
	if (ret == 0) {
		ret = settings_save_one("sg/cal/set_epoch", &cfg.cal_set_epoch,
					sizeof(cfg.cal_set_epoch));
	}
	return ret;
}

uint32_t config_next_measurement(uint32_t now_epoch)
{
	struct app_config c;

	config_get(&c);
	if (c.sched_interval_min == 0) {
		return 0;
	}

	uint32_t iv = (uint32_t)c.sched_interval_min * 60U;
	int64_t local = (int64_t)now_epoch + (int64_t)c.tz_min * 60;
	int64_t day0 = local - (local % 86400); /* local midnight, as a local timestamp */
	uint32_t start = (uint32_t)c.sched_start_min * 60U;
	uint32_t end = (uint32_t)c.sched_end_min * 60U;

	/*
	 * Candidate windows: the one that opened today and, if it spans
	 * midnight, the one that opened yesterday; pick the earliest slot >= now.
	 */
	for (int d = -1; d <= 1; d++) {
		int64_t w_start = day0 + d * 86400 + start;
		int64_t w_end;

		if (start == end) {
			w_end = w_start + 86400; /* all day */
		} else if (end > start) {
			w_end = day0 + d * 86400 + end;
		} else {
			w_end = day0 + (d + 1) * 86400 + end; /* spans midnight */
		}
		int64_t slot;

		if (local < w_start) {
			slot = w_start;
		} else {
			slot = w_start + ((local - w_start + iv - 1) / iv) * iv;
		}
		if (slot < w_end || (slot == w_end && start != end)) {
			return (uint32_t)(slot - (int64_t)c.tz_min * 60);
		}
	}
	return 0; /* cannot happen: the d = +1 window always yields a slot */
}

void config_set_change_cb(void (*cb)(void))
{
	change_cb = cb;
}
