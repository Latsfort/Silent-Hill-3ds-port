#include "sh3ds/dms.h"

#include <math.h>
#include <string.h>

#define DMS_HEADER_SIZE 44u
#define DMS_ENTRY_SIZE 16u
#define DMS_HOLD_SIZE 6u
#define DMS_CHAR_KF_SIZE 12u
#define DMS_CAM_KF_SIZE 16u
#define DMS_ANGLE_TO_RAD (6.283185307f / 4096.0f)

static int16_t rd_s16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int32_t rd_s32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static bool in_range(const ShDms *dms, size_t off, size_t len)
{
    return off <= dms->size && len <= dms->size - off;
}

static bool read_entry(const ShDms *dms, size_t off, ShDmsEntry *e, size_t kf_size)
{
    if (!in_range(dms, off, DMS_ENTRY_SIZE)) {
        return false;
    }
    const uint8_t *p = dms->data + off;
    e->kf_count = rd_s16(p);
    e->hold_count = p[2];
    memcpy(e->name, p + 4, 4);
    e->name[4] = 0;
    e->holds = (size_t)rd_s32(p + 8);
    e->kfs = (size_t)rd_s32(p + 12);
    if (e->kf_count <= 0 || !in_range(dms, e->kfs, (size_t)e->kf_count * kf_size) ||
        !in_range(dms, e->holds, (size_t)e->hold_count * DMS_HOLD_SIZE)) {
        return false;
    }
    return true;
}

bool sh_dms_parse(ShDms *dms, const uint8_t *data, size_t size)
{
    if (!dms || !data || size < DMS_HEADER_SIZE) {
        return false;
    }
    memset(dms, 0, sizeof(*dms));
    dms->data = data;
    dms->size = size;
    dms->char_count = data[1];
    dms->seg_count = data[2];
    dms->segments = (size_t)rd_s32(data + 8);
    for (int i = 0; i < 3; i++) {
        dms->origin[i] = (float)rd_s32(data + 0xC + i * 4) / 256.0f;
    }
    const size_t chars = (size_t)rd_s32(data + 0x18);
    if (!in_range(dms, dms->segments, (size_t)dms->seg_count * 4u)) {
        return false;
    }
    if (!read_entry(dms, 0x1C, &dms->camera, DMS_CAM_KF_SIZE)) {
        return false;
    }
    if (dms->char_count > SH_DMS_MAX_CHARS) {
        dms->char_count = SH_DMS_MAX_CHARS;
    }
    for (int c = 0; c < dms->char_count; c++) {
        if (!read_entry(dms, chars + (size_t)c * DMS_ENTRY_SIZE, &dms->chars[c], DMS_CHAR_KF_SIZE)) {
            dms->char_count = c;
            break;
        }
    }
    return true;
}

int sh_dms_find_char(const ShDms *dms, const char *name)
{
    if (!dms || !name) {
        return -1;
    }
    for (int c = 0; c < dms->char_count; c++) {
        if (strncmp(dms->chars[c].name, name, 4) == 0) {
            return c;
        }
    }
    return -1;
}

/* Dms_KeyframeIdxGet */
static int keyframe_index(const ShDms *dms, const ShDmsEntry *e, int frame)
{
    int kf = frame;
    for (int h = 0; h < e->hold_count; h++) {
        const uint8_t *p = dms->data + e->holds + (size_t)h * DMS_HOLD_SIZE;
        const int start = rd_s16(p);
        const int end = rd_s16(p + 2);
        const int held = rd_s16(p + 4);
        if (frame < start) {
            break;
        }
        if (frame <= end) {
            kf = held;
            break;
        }
        kf -= end - start;
    }
    if (kf < 0) {
        kf = 0;
    }
    if (kf > e->kf_count - 1) {
        kf = e->kf_count - 1;
    }
    return kf;
}

/* Dms_SegmentStateGet + Dms_KeyframeInterpGet */
static void interp_get(const ShDms *dms, const ShDmsEntry *e, float frame, int *prev, int *next, float *alpha)
{
    int fi = (int)floorf(frame);
    float frac = frame - (float)fi;
    int pf = fi, nf = fi + 1;
    if (fi < 0) {
        fi = 0;
        frac = 0.0f;
        pf = 0;
        nf = 1;
    }
    for (int s = 0; s < dms->seg_count; s++) {
        const uint8_t *p = dms->data + dms->segments + (size_t)s * 4u;
        const int start = rd_s16(p);
        const int count = rd_s16(p + 2);
        if (fi != start + count - 1) {
            continue;
        }
        if (count > 1) {
            /* Ending: lerp from previous frame past the end. */
            pf = fi - 1;
            nf = fi;
            frac += 1.0f;
        } else {
            pf = nf = fi;
            frac = 0.0f;
        }
        break;
    }
    *prev = keyframe_index(dms, e, pf);
    *next = keyframe_index(dms, e, nf);
    *alpha = frac;
}

static float angle_lerp(int16_t from, int16_t to, float alpha)
{
    int delta = ((to - from) + 2048) & 4095;
    delta -= 2048;
    float a = (float)from + (float)delta * alpha;
    return a * DMS_ANGLE_TO_RAD;
}

bool sh_dms_char_sample(const ShDms *dms, int idx, float frame, float pos_ps1[3], float rot_rad[3])
{
    if (!dms || idx < 0 || idx >= dms->char_count) {
        return false;
    }
    const ShDmsEntry *e = &dms->chars[idx];
    int prev, next;
    float alpha;
    interp_get(dms, e, frame, &prev, &next, &alpha);
    const uint8_t *a = dms->data + e->kfs + (size_t)prev * DMS_CHAR_KF_SIZE;
    const uint8_t *b = dms->data + e->kfs + (size_t)next * DMS_CHAR_KF_SIZE;
    for (int i = 0; i < 3; i++) {
        const float pa = (float)rd_s16(a + i * 2);
        const float pb = (float)rd_s16(b + i * 2);
        pos_ps1[i] = (pa + (pb - pa) * alpha) / 256.0f + dms->origin[i];
        if (rot_rad) {
            rot_rad[i] = angle_lerp(rd_s16(a + 6 + i * 2), rd_s16(b + 6 + i * 2), alpha);
        }
    }
    return true;
}

bool sh_dms_camera_sample(const ShDms *dms, float frame, float pos_ps1[3], float look_ps1[3], float *proj_dist)
{
    if (!dms || dms->camera.kf_count <= 0) {
        return false;
    }
    const ShDmsEntry *e = &dms->camera;
    int prev, next;
    float alpha;
    interp_get(dms, e, frame, &prev, &next, &alpha);
    const uint8_t *a = dms->data + e->kfs + (size_t)prev * DMS_CAM_KF_SIZE;
    const uint8_t *b = dms->data + e->kfs + (size_t)next * DMS_CAM_KF_SIZE;
    for (int i = 0; i < 3; i++) {
        const float pa = (float)rd_s16(a + i * 2), pb = (float)rd_s16(b + i * 2);
        const float la = (float)rd_s16(a + 6 + i * 2), lb = (float)rd_s16(b + 6 + i * 2);
        pos_ps1[i] = (pa + (pb - pa) * alpha) / 256.0f + dms->origin[i];
        look_ps1[i] = (la + (lb - la) * alpha) / 256.0f + dms->origin[i];
    }
    if (proj_dist) {
        const float da = (float)rd_s16(a + 14), db = (float)rd_s16(b + 14);
        *proj_dist = da + (db - da) * alpha;
    }
    return true;
}
