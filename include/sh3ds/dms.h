#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cutscene keyframe files (bodyprog/dms.c). All outputs are in PS1 space:
 * meters, Y down, Z forward; rotations in radians (PS1 Q3_12 angles * 2π/4096).
 */

#define SH_DMS_MAX_CHARS 8

typedef struct ShDmsEntry {
    int kf_count;
    int hold_count;
    char name[5];
    size_t holds; /* file offset of hold ranges (6 B each) */
    size_t kfs;   /* file offset of keyframes (12 B char / 16 B camera) */
} ShDmsEntry;

typedef struct ShDms {
    const uint8_t *data;
    size_t size;
    int char_count;
    int seg_count;
    size_t segments;  /* file offset, 4 B each: s16 start, s16 count */
    float origin[3];  /* meters */
    ShDmsEntry chars[SH_DMS_MAX_CHARS];
    ShDmsEntry camera;
} ShDms;

bool sh_dms_parse(ShDms *dms, const uint8_t *data, size_t size);
int sh_dms_find_char(const ShDms *dms, const char *name);

/* Sample character `idx` at DMS frame `frame` (float). */
bool sh_dms_char_sample(const ShDms *dms, int idx, float frame, float pos_ps1[3], float rot_rad[3]);

/* Sample camera: position + look-at (PS1 meters) and projection distance (pixels). */
bool sh_dms_camera_sample(const ShDms *dms, float frame, float pos_ps1[3], float look_ps1[3], float *proj_dist);

#ifdef __cplusplus
}
#endif
