/* Record encode/decode - see record.h */

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "record.h"

static int16_t clamp_i16(float v)
{
	if (v > 32767.0f) {
		return INT16_MAX;
	}
	if (v < -32768.0f) {
		return INT16_MIN;
	}
	return (int16_t)lroundf(v);
}

static uint16_t clamp_u16(float v)
{
	if (v < 0.0f) {
		return 0;
	}
	if (v > 65535.0f) {
		return UINT16_MAX;
	}
	return (uint16_t)lroundf(v);
}

void record_from_measurement(struct record *r, const struct measurement *m)
{
	const struct tfmini_stats *s = &m->lidar;

	memset(r, 0, sizeof(*r));

	if (s->n_valid > 0) {
		r->flags |= RECORD_FLAG_LIDAR_OK;
		r->dist_median_cm = s->dist_median_cm;
		r->dist_var_cm2 = clamp_u16(s->dist_var_cm2);
	} else {
		r->dist_median_cm = UINT16_MAX;
		r->dist_var_cm2 = UINT16_MAX;
	}
	r->strength = s->strength_median;
	r->n_frames = s->n_frames;
	r->n_valid = s->n_valid;
	r->n_out_of_range = s->n_invalid + s->n_saturated + s->n_weak;
	r->lidar_temp_dc = (s->n_frames > 0) ? s->temp_c_x10 : INT16_MIN;

	if (m->tilt_ret == 0) {
		r->flags |= RECORD_FLAG_TILT_OK;
		r->tilt_cdeg = clamp_i16(m->tilt.tilt_deg * 100.0f);
		r->pitch_cdeg = clamp_i16(m->tilt.pitch_deg * 100.0f);
		r->roll_cdeg = clamp_i16(m->tilt.roll_deg * 100.0f);
		r->imu_temp_dc = clamp_i16(m->tilt.temp_c * 10.0f);
	} else {
		r->tilt_cdeg = r->pitch_cdeg = r->roll_cdeg = INT16_MIN;
		r->imu_temp_dc = INT16_MIN;
	}
	r->vbat_start_mv = m->vbat_mv_start;
	r->vbat_end_mv = m->vbat_mv_end;
}

void record_encode(const struct record *r, uint8_t buf[RECORD_SIZE])
{
	sys_put_le16(RECORD_MAGIC, &buf[0]);
	buf[2] = RECORD_VERSION;
	buf[3] = r->flags;
	sys_put_le32(r->epoch, &buf[4]);
	sys_put_le32(r->seq, &buf[8]);
	sys_put_le16(r->dist_median_cm, &buf[12]);
	sys_put_le16(r->dist_var_cm2, &buf[14]);
	sys_put_le16(r->strength, &buf[16]);
	sys_put_le16(r->n_frames, &buf[18]);
	sys_put_le16(r->n_valid, &buf[20]);
	sys_put_le16(r->n_out_of_range, &buf[22]);
	sys_put_le16((uint16_t)r->tilt_cdeg, &buf[24]);
	sys_put_le16((uint16_t)r->pitch_cdeg, &buf[26]);
	sys_put_le16((uint16_t)r->roll_cdeg, &buf[28]);
	sys_put_le16((uint16_t)r->imu_temp_dc, &buf[30]);
	sys_put_le16((uint16_t)r->lidar_temp_dc, &buf[32]);
	sys_put_le16(r->vbat_start_mv, &buf[34]);
	sys_put_le16(r->vbat_end_mv, &buf[36]);
	sys_put_le16(crc16_ccitt(0xFFFF, buf, RECORD_SIZE - 2), &buf[38]);
}

bool record_decode(const uint8_t buf[RECORD_SIZE], struct record *r)
{
	if (sys_get_le16(&buf[0]) != RECORD_MAGIC || buf[2] != RECORD_VERSION) {
		return false;
	}
	if (sys_get_le16(&buf[38]) != crc16_ccitt(0xFFFF, buf, RECORD_SIZE - 2)) {
		return false;
	}
	r->flags = buf[3];
	r->epoch = sys_get_le32(&buf[4]);
	r->seq = sys_get_le32(&buf[8]);
	r->dist_median_cm = sys_get_le16(&buf[12]);
	r->dist_var_cm2 = sys_get_le16(&buf[14]);
	r->strength = sys_get_le16(&buf[16]);
	r->n_frames = sys_get_le16(&buf[18]);
	r->n_valid = sys_get_le16(&buf[20]);
	r->n_out_of_range = sys_get_le16(&buf[22]);
	r->tilt_cdeg = (int16_t)sys_get_le16(&buf[24]);
	r->pitch_cdeg = (int16_t)sys_get_le16(&buf[26]);
	r->roll_cdeg = (int16_t)sys_get_le16(&buf[28]);
	r->imu_temp_dc = (int16_t)sys_get_le16(&buf[30]);
	r->lidar_temp_dc = (int16_t)sys_get_le16(&buf[32]);
	r->vbat_start_mv = sys_get_le16(&buf[34]);
	r->vbat_end_mv = sys_get_le16(&buf[36]);
	return true;
}

void record_print_header(void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	out(ctx, "seq,epoch,flags,dist_cm,var_cm2,strength,n_frames,n_valid,n_oor,"
		 "tilt_deg,pitch_deg,roll_deg,imu_temp_c,lidar_temp_c,vbat_start_mv,vbat_end_mv");
}

/* Signed fixed-point to text ("-0.50"), INT16_MIN -> "" (missing). */
static const char *fixp(int16_t v, int scale, char *buf, size_t len)
{
	if (v == INT16_MIN) {
		buf[0] = '\0';
	} else if (scale == 100) {
		snprintf(buf, len, "%s%d.%02d", v < 0 ? "-" : "", abs(v) / 100, abs(v) % 100);
	} else {
		snprintf(buf, len, "%s%d.%d", v < 0 ? "-" : "", abs(v) / 10, abs(v) % 10);
	}
	return buf;
}

void record_print(const struct record *r,
		  void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	char t[12], p[12], ro[12], it[12], lt[12];

	out(ctx, "%u,%u,0x%02x,%u,%u,%u,%u,%u,%u,%s,%s,%s,%s,%s,%u,%u",
	    r->seq, r->epoch, r->flags, r->dist_median_cm, r->dist_var_cm2, r->strength,
	    r->n_frames, r->n_valid, r->n_out_of_range,
	    fixp(r->tilt_cdeg, 100, t, sizeof(t)), fixp(r->pitch_cdeg, 100, p, sizeof(p)),
	    fixp(r->roll_cdeg, 100, ro, sizeof(ro)), fixp(r->imu_temp_dc, 10, it, sizeof(it)),
	    fixp(r->lidar_temp_dc, 10, lt, sizeof(lt)), r->vbat_start_mv, r->vbat_end_mv);
}
