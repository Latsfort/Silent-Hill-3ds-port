#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sh3ds/map.h"
#include "sh3ds/tim.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Unique item models (ITEM/UNQxx.TMD) as drawn by Gfx_PickupItemAnimate: the
 * item screen camera sits 1872 GTE units in front of the model, which is tilted
 * by INV_ITEM_ROTATIONS, spun around Y and lit by ambient 0.5 plus two white
 * flat lights (into the screen and toward +X).
 */

#define SH_ITEM_TEX_SIZE 256
#define SH_ITEM_MAX_VERTS (400 * 3)

typedef struct ShItemModel {
    const uint8_t *tmd;
    size_t tmd_size;
    unsigned clut_delta; /* relative to the item TIM's CLUT origin */
    bool ok;
} ShItemModel;

/* `tim_file` is the raw TIM00 file: its CLUT origin turns TMD CBA ids into deltas. */
bool sh_item_model_init(ShItemModel *model, const uint8_t *tmd, size_t tmd_size, const uint8_t *tim_file,
                        size_t tim_size);

/*
 * Camera-space triangles (meters, -Z forward), back-to-front, UVs normalized for a
 * SH_ITEM_TEX_SIZE texture, vertex color = PS1 light intensity / 2 (draw with 2x
 * modulate). Angles in radians; `scale` is the pickup grow factor (0..1).
 */
bool sh_item_model_bake(const ShItemModel *model, ShMapVertex *out, size_t capacity, size_t *count_out,
                        float rot_x, float rot_y, float rot_z, float scale, float fov_deg);

#ifdef __cplusplus
}
#endif
