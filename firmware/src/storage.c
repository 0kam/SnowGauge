/* Record storage - see storage.h */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>

#include "storage.h"
#include "timekeeping.h"

LOG_MODULE_REGISTER(storage, CONFIG_LOG_DEFAULT_LEVEL);

#define LFS_MOUNT_POINT "/lfs1"
#define REC_PREFIX      "rec_"
#define REC_SUFFIX      ".bin"
#define REC_NOTIME_NAME LFS_MOUNT_POINT "/" REC_PREFIX "notime" REC_SUFFIX

FS_FSTAB_DECLARE_ENTRY(DT_NODELABEL(lfs1));
static struct fs_mount_t *const lfs_mp = &FS_FSTAB_ENTRY(DT_NODELABEL(lfs1));

#define MIRROR_ID DT_FIXED_PARTITION_ID(DT_NODELABEL(mirror_partition))
static const struct flash_area *mirror;
static uint32_t mirror_slots;      /* capacity */
static uint32_t mirror_write_idx;  /* first blank slot */
static uint32_t mirror_valid;      /* valid records found at boot + appended */

static bool fs_mounted;
static bool mirror_full_warned;
static uint32_t record_count;
static uint32_t next_seq;
static K_MUTEX_DEFINE(lock);

/* ---- mirror ---- */

static bool slot_blank(const uint8_t *buf)
{
	for (int i = 0; i < RECORD_SIZE; i++) {
		if (buf[i] != 0xFF) {
			return false;
		}
	}
	return true;
}

static int mirror_scan(struct record *last, bool *have_last)
{
	uint8_t buf[RECORD_SIZE];
	struct record r;
	int ret;

	*have_last = false;
	mirror_write_idx = 0;
	mirror_valid = 0;

	for (uint32_t i = 0; i < mirror_slots; i++) {
		ret = flash_area_read(mirror, (off_t)i * RECORD_SIZE, buf, RECORD_SIZE);
		if (ret) {
			return ret;
		}
		if (slot_blank(buf)) {
			mirror_write_idx = i;
			return 0;
		}
		if (record_decode(buf, &r)) {
			mirror_valid++;
			*last = r;
			*have_last = true;
		} else if (i == 0) {
			/* Leftovers from a previous, unrelated image: reclaim. */
			LOG_WRN("mirror partition holds foreign data - erasing");
			ret = flash_area_erase(mirror, 0, mirror->fa_size);
			if (ret) {
				return ret;
			}
			mirror_write_idx = 0;
			return 0;
		}
		/* torn slot (power loss mid-write): skip it */
	}
	mirror_write_idx = mirror_slots;
	return 0;
}

static int mirror_append(const uint8_t buf[RECORD_SIZE])
{
	int ret;

	if (mirror_write_idx >= mirror_slots) {
		return -ENOSPC;
	}
	ret = flash_area_write(mirror, (off_t)mirror_write_idx * RECORD_SIZE, buf, RECORD_SIZE);
	if (ret) {
		return ret;
	}
	mirror_write_idx++;
	mirror_valid++;
	return 0;
}

/* ---- LittleFS ---- */

static bool is_record_file(const char *name)
{
	size_t n = strlen(name);
	size_t p = strlen(REC_PREFIX), s = strlen(REC_SUFFIX);

	return n > p + s && strncmp(name, REC_PREFIX, p) == 0 &&
	       strcmp(name + n - s, REC_SUFFIX) == 0;
}

static int lfs_count_records(uint32_t *count)
{
	struct fs_dir_t dir;
	struct fs_dirent ent;
	int ret;

	*count = 0;
	fs_dir_t_init(&dir);
	ret = fs_opendir(&dir, LFS_MOUNT_POINT);
	if (ret) {
		return ret;
	}
	while ((ret = fs_readdir(&dir, &ent)) == 0 && ent.name[0] != '\0') {
		if (ent.type == FS_DIR_ENTRY_FILE && is_record_file(ent.name)) {
			*count += ent.size / RECORD_SIZE;
		}
	}
	(void)fs_closedir(&dir);
	return ret;
}

static void record_file_name(uint32_t epoch, char *buf, size_t len)
{
	if (epoch == 0) {
		snprintf(buf, len, "%s", REC_NOTIME_NAME);
	} else {
		snprintf(buf, len, LFS_MOUNT_POINT "/" REC_PREFIX "%06u" REC_SUFFIX,
			 time_yyyymm(epoch));
	}
}

static int lfs_append(const struct record *r, const uint8_t buf[RECORD_SIZE])
{
	struct fs_file_t f;
	char name[40];
	int ret;

	record_file_name(r->epoch, name, sizeof(name));
	fs_file_t_init(&f);
	ret = fs_open(&f, name, FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
	if (ret) {
		LOG_ERR("open %s: %d", name, ret);
		return ret;
	}
	ret = fs_write(&f, buf, RECORD_SIZE);
	if (ret < 0) {
		LOG_ERR("write %s: %d", name, ret);
	} else if (ret != RECORD_SIZE) {
		LOG_ERR("short write %s: %d", name, ret);
		ret = -EIO;
	} else {
		ret = 0;
	}
	(void)fs_close(&f);
	return ret;
}

/* ---- public ---- */

int storage_init(void)
{
	struct record last;
	bool have_last;
	int ret;

	ret = flash_area_open(MIRROR_ID, &mirror);
	if (ret) {
		LOG_ERR("mirror partition open: %d", ret);
		return ret;
	}
	mirror_slots = mirror->fa_size / RECORD_SIZE;

	ret = mirror_scan(&last, &have_last);
	if (ret) {
		LOG_ERR("mirror scan: %d", ret);
		return ret;
	}
	LOG_INF("mirror: %u/%u slots used (%u valid)", mirror_write_idx, mirror_slots,
		mirror_valid);

	ret = fs_mount(lfs_mp);
	if (ret) {
		LOG_ERR("LittleFS mount %s: %d (records go to the mirror only)",
			lfs_mp->mnt_point, ret);
		fs_mounted = false;
	} else {
		struct fs_statvfs st;

		fs_mounted = true;
		if (fs_statvfs(lfs_mp->mnt_point, &st) == 0) {
			LOG_INF("LittleFS: block %lu x %lu, free %lu", st.f_frsize, st.f_blocks,
				st.f_bfree);
		}
	}

	uint32_t fs_count = 0;

	if (fs_mounted) {
		(void)lfs_count_records(&fs_count);
	}
	record_count = MAX(fs_count, mirror_valid);
	next_seq = have_last ? last.seq + 1 : fs_count;

	if (have_last && last.epoch != 0 && time_get_state() == TIME_UNSET) {
		char ts[24];

		(void)time_set(last.epoch, false);
		time_format(last.epoch, ts, sizeof(ts));
		LOG_WRN("clock restored from last record (%s) - set the time", ts);
	}
	LOG_INF("records: %u (fs %u, mirror %u), next seq %u", record_count, fs_count,
		mirror_valid, next_seq);
	return 0;
}

int storage_append(const struct record *r)
{
	uint8_t buf[RECORD_SIZE];
	int ret_fs = -ENODEV, ret_mirror;

	record_encode(r, buf);

	k_mutex_lock(&lock, K_FOREVER);
	if (fs_mounted) {
		ret_fs = lfs_append(r, buf);
	}
	ret_mirror = mirror_append(buf);
	if (ret_mirror == -ENOSPC) {
		if (!mirror_full_warned) {
			LOG_WRN("mirror partition full - records go to LittleFS only");
			mirror_full_warned = true;
		}
	} else if (ret_mirror) {
		LOG_WRN("mirror append: %d", ret_mirror);
	}
	if (ret_fs == 0 || ret_mirror == 0) {
		record_count++;
	}
	if (r->seq >= next_seq) {
		next_seq = r->seq + 1;
	}
	k_mutex_unlock(&lock);

	/* Primary result when the file system is up; the mirror is best effort. */
	return fs_mounted ? ret_fs : ret_mirror;
}

uint32_t storage_record_count(void)
{
	return record_count;
}

uint32_t storage_next_seq(void)
{
	return next_seq;
}

int storage_mirror_read(uint32_t idx, struct record *r)
{
	uint8_t buf[RECORD_SIZE];
	int ret;

	if (idx >= mirror_write_idx) {
		return -ENOENT;
	}
	ret = flash_area_read(mirror, (off_t)idx * RECORD_SIZE, buf, RECORD_SIZE);
	if (ret) {
		return ret;
	}
	return record_decode(buf, r) ? 0 : -EBADMSG;
}

uint32_t storage_mirror_count(void)
{
	return mirror_write_idx;
}

uint32_t storage_mirror_capacity(void)
{
	return mirror_slots;
}

int storage_last_record(struct record *r)
{
	for (uint32_t i = mirror_write_idx; i > 0; i--) {
		if (storage_mirror_read(i - 1, r) == 0) {
			return 0;
		}
	}
	return -ENOENT;
}

bool storage_fs_ok(void)
{
	return fs_mounted;
}

int storage_list_files(void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	struct fs_dir_t dir;
	struct fs_dirent ent;
	int ret;

	if (!fs_mounted) {
		return -ENODEV;
	}
	fs_dir_t_init(&dir);
	ret = fs_opendir(&dir, LFS_MOUNT_POINT);
	if (ret) {
		return ret;
	}
	while ((ret = fs_readdir(&dir, &ent)) == 0 && ent.name[0] != '\0') {
		out(ctx, "%s/%s %u", LFS_MOUNT_POINT, ent.name, (unsigned int)ent.size);
	}
	(void)fs_closedir(&dir);
	return ret;
}

int storage_erase_all(void)
{
	struct fs_dir_t dir;
	struct fs_dirent ent;
	char path[sizeof(LFS_MOUNT_POINT) + 1 + MAX_FILE_NAME + 1];
	int ret = 0;

	k_mutex_lock(&lock, K_FOREVER);
	if (fs_mounted) {
		fs_dir_t_init(&dir);
		ret = fs_opendir(&dir, LFS_MOUNT_POINT);
		if (ret == 0) {
			/* Collect-and-delete one at a time: unlinking while iterating is unsafe. */
			for (;;) {
				ret = fs_readdir(&dir, &ent);
				if (ret || ent.name[0] == '\0') {
					break;
				}
				if (ent.type == FS_DIR_ENTRY_FILE && is_record_file(ent.name)) {
					snprintf(path, sizeof(path), LFS_MOUNT_POINT "/%s", ent.name);
					(void)fs_closedir(&dir);
					ret = fs_unlink(path);
					if (ret) {
						LOG_ERR("unlink %s: %d", path, ret);
						goto out;
					}
					fs_dir_t_init(&dir);
					ret = fs_opendir(&dir, LFS_MOUNT_POINT);
					if (ret) {
						goto out;
					}
				}
			}
			(void)fs_closedir(&dir);
		}
	}
	int ret_erase = flash_area_erase(mirror, 0, mirror->fa_size);

	if (ret_erase) {
		LOG_ERR("mirror erase: %d", ret_erase);
		ret = ret ? ret : ret_erase;
	}
	mirror_write_idx = 0;
	mirror_valid = 0;
	mirror_full_warned = false;
	record_count = 0;
	next_seq = 0;
out:
	k_mutex_unlock(&lock);
	return ret;
}
