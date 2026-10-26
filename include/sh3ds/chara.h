#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sh3ds/map.h"
#include "sh3ds/tim.h"
#include "sh3ds/anim_timing.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Harry (HERO.ILM) posed with HB_BASE.ANM.
 * Vertices are in meters, Y-up, feet at the origin, facing -Z (yaw 0).
 *
 * Keyframe ranges from HARRY_BASE_ANIM_INFOS (decomp):
 *   idle loop  503..542
 *   walk loop    1..23
 *   run loop    26..45
 *
 * GPU atlas layout (matches sh_gfx_set_player_tim): 1024x512, sheets 256x192
 * in row-major slots; UV set when `clut_map` is non-NULL.
 */

#define SH_CHARA_MAX_VERTS (900 * 3)
#define SH_CHARA_KF_IDLE_START 503
#define SH_CHARA_KF_IDLE_END 542
#define SH_CHARA_KF_WALK_START 1
#define SH_CHARA_KF_WALK_END 23
#define SH_CHARA_KF_RUN_START 26
#define SH_CHARA_KF_RUN_END 45
/* Weapon keyframes in HB_WEP*.ANM append after this index in HB_BASE.ANM. */
#define SH_CHARA_WEP_KF_BASE 568
#define SH_CHARA_MAP_KF_BASE 676

#define SH_CHARA_CLUT_SLOTS 8
#define SH_CHARA_SHEET_W 256
#define SH_CHARA_SHEET_H 256 /* full TIM height; 192 bled into the next sheet */
#define SH_CHARA_TEX_W 1024
#define SH_CHARA_TEX_H 512
#define SH_CHARA_SHEETS_PER_ROW 4

/* NPC atlas: 512x256, sheets 256x128 (REBIRD 128x128, SIBYL 256x128). */
#define SH_NPC_SHEET_W 256
#define SH_NPC_SHEET_H 128
#define SH_NPC_TEX_W 512
#define SH_NPC_TEX_H 256
#define SH_NPC_SHEETS_PER_ROW 2
#define SH_NPC_CLUT_SLOTS 4

typedef struct ShCharaClutMap {
    unsigned delta[SH_CHARA_CLUT_SLOTS];
    uint8_t tim_src[SH_CHARA_CLUT_SLOTS]; /* 0 = HERO.TIM, 1 = held-item TIM (HPRGUN…) */
    int count;
    /* Atlas layout the UVs are written for (0 = Harry defaults). */
    int sheet_w, sheet_h, tex_w, tex_h, per_row, max_slots;
} ShCharaClutMap;

/* Set `map` for the Harry (1024x512) or NPC (512x256) atlas. */
void sh_chara_clut_map_init(ShCharaClutMap *map, bool npc);

int sh_chara_anm_keyframe_count(const uint8_t *anm, size_t anm_size);

typedef enum ShCharaFlags {
    SH_CHARA_F_NONE = 0,
    SH_CHARA_F_HARRY_HANDS = 1 << 0, /* empty hands: keep 06LHAND + 10RHAND */
    SH_CHARA_F_HARRY_GUN = 1 << 1,   /* handgun: 06LHAND + 10RHAND3 */
    SH_CHARA_F_HARRY_KNIFE = 1 << 2, /* kitchen knife: 06LHAND2 + 10RHAND2 */
    SH_CHARA_F_SPLIT_HEAD_1 = 1 << 3,
    SH_CHARA_F_SPLIT_HEAD_2 = 1 << 4
} ShCharaFlags;

#define SH_CHARA_MAX_BONES 32

typedef struct ShCharaBone {
    int32_t rotation[3][3]; /* PS1 model-space Q12. */
    int32_t translation[3]; /* PS1 model-space Q8. */
} ShCharaBone;

/* Valid immediately after a successful pose bake, before another character is baked. */
bool sh_chara_last_bone(unsigned bone, ShCharaBone *out);

/* Collect CLUT deltas from an ILM/PLM; `tim_src` selects which TIM sheet uploads use. */
bool sh_chara_collect_cluts(const uint8_t *ilm, size_t ilm_size, ShCharaClutMap *clut_map, int tim_src);

/*
 * Pose any ILM with any ANM (same skeleton family). `clut_map` non-NULL: fill
 * slots + write atlas UVs; vertex RGB = shade for MODULATE.
 */
/*
 * `wep_kf`: optional HB_WEP3/4 raw keyframe blob (keyframes >= wep_kf_base).
 * `held_plm`: optional HANDGUN.PLM / KNIFE.PLM skinned to `held_bone` (10 = right hand); -1 = none.
 */
bool sh_chara_bake(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *ilm, size_t ilm_size,
                   const uint8_t *anm, size_t anm_size, const ShTim *tim, int keyframe, ShCharaClutMap *clut_map,
                   unsigned flags, const uint8_t *wep_kf, size_t wep_kf_size, int wep_kf_base,
                   const ShTim *held_tim, const uint8_t *held_plm, size_t held_plm_size, int held_bone);

bool sh_chara_bake_split_head(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *ilm, size_t ilm_size,
    const uint8_t *anm, size_t anm_size, const ShTim *tim, int keyframe, ShCharaClutMap *cluts, int variant, int16_t twist);
bool sh_chara_bake_split_head_blend(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *ilm, size_t ilm_size,
    const uint8_t *anm, size_t anm_size, const ShTim *tim, const ShAnimPoseTiming *pose,
    ShCharaClutMap *cluts, int variant, int16_t twist);

/* Static LM/PLM at world position (pickup close-up). */
bool sh_chara_bake_prop(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *plm, size_t plm_size,
                        const ShTim *tim, ShCharaClutMap *clut_map, const float pos[3], float yaw, float scale);

/* Harry convenience wrapper (HERO.ILM + HB_BASE.ANM). */
bool sh_chara_bake_harry(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *ilm,
                         size_t ilm_size, const uint8_t *anm, size_t anm_size, const ShTim *tim,
                         int keyframe, ShCharaClutMap *clut_map);

#ifdef __cplusplus
}
#endif
