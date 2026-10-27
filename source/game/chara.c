#include "sh3ds/chara.h"
#include "sh3ds/tim.h"
#include "sh3ds/fixed.h"

#include <math.h>
#include <string.h>

#define LM_MAGIC '0'
#define LM_VERSION 6
#define LM_MODEL_SIZE 16
#define LM_MESH_SIZE 24
#define PRIM_SIZE 20
#define Q8_TO_METERS (1.0f / 256.0f)
#define Q12_ONE 4096

typedef struct AnmBind {
    int8_t parent;
    int8_t rot_idx;
    int8_t trans_idx;
    int8_t bind_t[3];
} AnmBind;

typedef struct BoneXform {
    int32_t m[3][3]; /* Q12 */
    int32_t t[3];
} BoneXform;

/* Keep large scratch off the 3DS main-thread stack. */
static float s_scratch[512][3];
static BoneXform s_world[SH_CHARA_MAX_BONES];
static BoneXform s_local[SH_CHARA_MAX_BONES];
static AnmBind s_binds[SH_CHARA_MAX_BONES];
static unsigned s_pose_bone_count;

bool sh_chara_last_bone(unsigned bone, ShCharaBone *out)
{
    if (!out || bone >= s_pose_bone_count) {
        return false;
    }
    memcpy(out->rotation, s_world[bone].m, sizeof out->rotation);
    memcpy(out->translation, s_world[bone].t, sizeof out->translation);
    return true;
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int16_t rd_s16(const uint8_t *p)
{
    return (int16_t)(p[0] | (p[1] << 8));
}

static void xform_identity(BoneXform *x)
{
    memset(x, 0, sizeof(*x));
    x->m[0][0] = x->m[1][1] = x->m[2][2] = Q12_ONE;
}

static void xform_mul(BoneXform *out, const BoneXform *parent, const BoneXform *local)
{
    BoneXform r;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r.m[i][j] = (parent->m[i][0] * local->m[0][j] + parent->m[i][1] * local->m[1][j] +
                         parent->m[i][2] * local->m[2][j]) >>
                        12;
        }
        r.t[i] = ((parent->m[i][0] * local->t[0] + parent->m[i][1] * local->t[1] +
                   parent->m[i][2] * local->t[2]) >>
                  12) +
                 parent->t[i];
    }
    *out = r;
}

static void xform_point(const BoneXform *x, int32_t lx, int32_t ly, int32_t lz, float out[3])
{
    int32_t w[3];
    for (int r = 0; r < 3; r++) {
        w[r] = ((x->m[r][0] * lx + x->m[r][1] * ly + x->m[r][2] * lz) >> 12) + x->t[r];
    }
    out[0] = w[0] * Q8_TO_METERS;
    out[1] = -w[1] * Q8_TO_METERS;
    out[2] = -w[2] * Q8_TO_METERS;
}

void sh_chara_clut_map_init(ShCharaClutMap *map, bool npc)
{
    if (!map) {
        return;
    }
    memset(map, 0, sizeof(*map));
    if (npc) {
        map->sheet_w = SH_NPC_SHEET_W;
        map->sheet_h = SH_NPC_SHEET_H;
        map->tex_w = SH_NPC_TEX_W;
        map->tex_h = SH_NPC_TEX_H;
        map->per_row = SH_NPC_SHEETS_PER_ROW;
        map->max_slots = SH_NPC_CLUT_SLOTS;
    } else {
        map->sheet_w = SH_CHARA_SHEET_W;
        map->sheet_h = SH_CHARA_SHEET_H;
        map->tex_w = SH_CHARA_TEX_W;
        map->tex_h = SH_CHARA_TEX_H;
        map->per_row = SH_CHARA_SHEETS_PER_ROW;
        map->max_slots = SH_CHARA_CLUT_SLOTS;
    }
}

static void clut_map_defaults(ShCharaClutMap *map)
{
    if (map && map->tex_w == 0) {
        const int count = map->count;
        sh_chara_clut_map_init(map, false);
        map->count = count;
    }
}

static int clut_slot(ShCharaClutMap *map, unsigned delta, int tim_src)
{
    if (!map) {
        return 0;
    }
    if (tim_src < 0) {
        tim_src = 0;
    }
    if (tim_src > 1) {
        tim_src = 1;
    }
    for (int i = 0; i < map->count; i++) {
        if (map->delta[i] == delta && map->tim_src[i] == (uint8_t)tim_src) {
            return i;
        }
    }
    const int max = map->max_slots > 0 && map->max_slots <= SH_CHARA_CLUT_SLOTS ? map->max_slots : SH_CHARA_CLUT_SLOTS;
    if (map->count >= max) {
        return 0;
    }
    map->delta[map->count] = delta;
    map->tim_src[map->count] = (uint8_t)tim_src;
    return map->count++;
}

static void emit_tri(ShMapVertex *verts, size_t capacity, size_t *count, const float *p0, const float *p1,
                     const float *p2, uint8_t u0, uint8_t v0, uint8_t u1, uint8_t v1, uint8_t u2, uint8_t v2,
                     unsigned clut_delta, int tim_src, const ShTim *hero_tim, const ShTim *held_tim,
                     ShCharaClutMap *clut_map)
{
    if (*count + 3 > capacity) {
        return;
    }

    const float ux = p1[0] - p0[0], uy = p1[1] - p0[1], uz = p1[2] - p0[2];
    const float vx = p2[0] - p0[0], vy = p2[1] - p0[1], vz = p2[2] - p0[2];
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    const float len = nx * nx + ny * ny + nz * nz;
    if (len < 1e-12f) {
        return;
    }
    const float inv = 1.0f / sqrtf(len);
    nx *= inv;
    ny *= inv;
    nz *= inv;

    const float shade = 0.75f + 0.25f * fabsf(nx * 0.35f + ny * 0.85f + nz * 0.40f);
    const ShTim *tim = (tim_src && held_tim) ? held_tim : hero_tim;
    const int clut = tim ? sh_tim_clut_offset(tim, clut_delta) : 0;
    const int slot = clut_slot(clut_map, clut_delta, tim_src);
    const int per_row = clut_map ? clut_map->per_row : SH_CHARA_SHEETS_PER_ROW;
    const int sheet_w = clut_map ? clut_map->sheet_w : SH_CHARA_SHEET_W;
    const int sheet_h = clut_map ? clut_map->sheet_h : SH_CHARA_SHEET_H;
    const float tex_w = (float)(clut_map ? clut_map->tex_w : SH_CHARA_TEX_W);
    const float tex_h = (float)(clut_map ? clut_map->tex_h : SH_CHARA_TEX_H);
    const int sheet_x = (slot % per_row) * sheet_w;
    const int sheet_y = (slot / per_row) * sheet_h;
    const float *pts[3] = {p0, p1, p2};
    const uint8_t us[3] = {u0, u1, u2};
    const uint8_t vs[3] = {v0, v1, v2};
    const bool gpu_uv = clut_map != NULL && tim != NULL;

    for (int i = 0; i < 3; i++) {
        uint8_t r = (uint8_t)(shade * 200.0f);
        uint8_t g = (uint8_t)(shade * 170.0f);
        uint8_t b = (uint8_t)(shade * 140.0f);
        uint8_t a = 255;
        float tu = 0.0f, tv = 0.0f;

        if (tim) {
            const uint32_t c = sh_tim_pixel_rgba_clut(tim, us[i], vs[i], clut);
            a = (uint8_t)(c & 0xFF);
            if (gpu_uv) {
                const int vv = vs[i] < sheet_h ? (int)vs[i] : (sheet_h - 1);
                tu = (float)(sheet_x + us[i]) / tex_w;
                tv = 1.0f - (float)(sheet_y + vv) / tex_h;
                const uint8_t s = (uint8_t)(shade * 255.0f);
                r = g = b = a ? s : 0;
            } else if (a == 0) {
                r = g = b = 0;
            } else {
                r = (uint8_t)(((c >> 24) & 0xFF) * shade);
                g = (uint8_t)(((c >> 16) & 0xFF) * shade);
                b = (uint8_t)(((c >> 8) & 0xFF) * shade);
            }
        }

        ShMapVertex *v = &verts[(*count)++];
        v->pos[0] = pts[i][0];
        v->pos[1] = pts[i][1];
        v->pos[2] = pts[i][2];
        v->uv[0] = tu;
        v->uv[1] = tv;
        v->color[0] = r;
        v->color[1] = g;
        v->color[2] = b;
        v->color[3] = a ? 255 : 0;
    }
}

int sh_chara_anm_keyframe_count(const uint8_t *anm, size_t anm_size)
{
    if (!anm || anm_size < 0x14) {
        return 0;
    }
    const unsigned data_offset = (unsigned)anm[0] | ((unsigned)anm[1] << 8);
    const unsigned kf_size = (unsigned)anm[4] | ((unsigned)anm[5] << 8);
    const unsigned declared = (unsigned)anm[0x10] | ((unsigned)anm[0x11] << 8);
    if (kf_size == 0 || data_offset >= anm_size) {
        return 0;
    }
    const unsigned fit = (unsigned)((anm_size - data_offset) / kf_size);
    return (int)(declared < fit ? declared : fit);
}

static const uint8_t *anm_keyframe_ptr(const uint8_t *anm, size_t anm_size, unsigned kf_size, int keyframe)
{
    if (!anm || anm_size < 0x14 || kf_size == 0 || keyframe < 0) {
        return NULL;
    }
    const unsigned data_offset = (unsigned)anm[0] | ((unsigned)anm[1] << 8);
    if (data_offset + (unsigned)(keyframe + 1) * kf_size > anm_size) {
        return NULL;
    }
    return anm + data_offset + (size_t)keyframe * kf_size;
}

/* Math_RotMatrixZxyNegGte pure Y (libkmath.s) and MulMatrix preserve translation. */
static void split_twist_y(BoneXform *bone, int16_t angle)
{
    const int32_t sine = sh_fixed_sin(angle), cosine = sh_fixed_cos(angle);
    for (int row = 0; row < 3; row++) {
        const int32_t x = bone->m[row][0], z = bone->m[row][2];
        bone->m[row][0] = (int32_t)(((int64_t)x * cosine - (int64_t)z * sine) >> 12);
        bone->m[row][2] = (int32_t)(((int64_t)x * sine + (int64_t)z * cosine) >> 12);
    }
}

#include "split_head_twist.h"
#include "bone_interpolation.h"
#include "match_pose_data.h"

/* func_8007D090 / func_80044F14: postmultiply local rotation, retaining translation. */
static bool match_arm_pose(BoneXform *bones, int count)
{
    for (unsigned i = 0; i < sizeof(s_match_pose) / sizeof(s_match_pose[0]); i++) {
        if (s_match_pose[i].bone >= (unsigned)count) { return false; }
        BoneXform rotation;
        xform_identity(&rotation);
        for (int row = 0; row < 3; row++) {
            for (int column = 0; column < 3; column++) {
                rotation.m[row][column] = s_match_pose[i].matrix[row][column];
            }
        }
        xform_mul(&bones[s_match_pose[i].bone], &bones[s_match_pose[i].bone], &rotation);
    }
    return true;
}

static bool build_skeleton(int *bone_count_out, const uint8_t *anm, size_t anm_size, int keyframe,
                          const uint8_t *wep_kf, size_t wep_kf_size, int wep_kf_base, bool is_player, int16_t twist,
                          const ShAnimPoseTiming *pose, bool match_arm)
{
    s_pose_bone_count = 0;
    if (!anm || anm_size < 0x14) {
        return false;
    }

    const unsigned rot_count = anm[2];
    const unsigned trans_count = anm[3];
    const unsigned kf_size = (unsigned)anm[4] | ((unsigned)anm[5] << 8);
    const int bone_count = anm[6];
    const unsigned scale_log2 = anm[0x12];
    const unsigned root_y = anm[0x13];
    int kf_count = sh_chara_anm_keyframe_count(anm, anm_size);
    if (wep_kf && kf_size > 0) {
        const int wc = wep_kf_base + (int)(wep_kf_size / kf_size);
        if (wc > kf_count) {
            kf_count = wc;
        }
    }

    if (bone_count < 2 || bone_count > SH_CHARA_MAX_BONES || kf_count < 1) {
        return false;
    }
    if (keyframe < 0) {
        keyframe = 0;
    }
    if (keyframe >= kf_count) {
        keyframe = kf_count - 1;
    }
    if (0x14u + (unsigned)bone_count * 6u > anm_size) {
        return false;
    }
    if (kf_size != rot_count * 9u + trans_count * 3u) {
        return false;
    }

    const uint8_t *frame = NULL;
    /* HB_WEPn.ANM is headerless: raw keyframes starting at global index wep_kf_base (568). */
    if (wep_kf && keyframe >= wep_kf_base) {
        const size_t off = (size_t)(keyframe - wep_kf_base) * kf_size;
        if (off + kf_size <= wep_kf_size) {
            frame = wep_kf + off;
        }
    }
    if (!frame) {
        frame = anm_keyframe_ptr(anm, anm_size, kf_size, keyframe);
    }
    if (!frame) {
        return false;
    }
    const uint8_t *second_frame = frame;
    int32_t alpha = 0;
    if (pose) {
        if (pose->first != keyframe || pose->second < 0 || pose->second >= kf_count ||
            pose->alpha < 0 || pose->alpha > 4096 || scale_log2 > 12) { return false; }
        second_frame = anm_keyframe_ptr(anm, anm_size, kf_size, pose->second);
        if (!second_frame) { return false; }
        alpha = pose->alpha;
    }

    for (int i = 0; i < bone_count; i++) {
        const uint8_t *bp = anm + 0x14 + (size_t)i * 6u;
        s_binds[i].parent = (int8_t)bp[0];
        s_binds[i].rot_idx = (int8_t)bp[1];
        s_binds[i].trans_idx = (int8_t)bp[2];
        s_binds[i].bind_t[0] = (int8_t)bp[3];
        s_binds[i].bind_t[1] = (int8_t)bp[4];
        s_binds[i].bind_t[2] = (int8_t)bp[5];
        xform_identity(&s_local[i]);
    }

    for (int i = 1; i < bone_count; i++) {
        if (s_binds[i].trans_idx < 0) {
            for (int k = 0; k < 3; k++) {
                s_local[i].t[k] = (int32_t)s_binds[i].bind_t[k] << scale_log2;
            }
        }
    }

    const uint8_t *frame_r = frame + trans_count * 3u;
    const uint8_t *second_r = second_frame + trans_count * 3u;
    for (int i = 1; i < bone_count; i++) {
        if (s_binds[i].trans_idx >= 0) {
            const uint8_t *td = frame + (unsigned)s_binds[i].trans_idx * 3u;
            const uint8_t *second_td = second_frame + (unsigned)s_binds[i].trans_idx * 3u;
            for (int k = 0; k < 3; k++) {
                s_local[i].t[k] = pose ? bone_translation_lerp((int8_t)td[k], (int8_t)second_td[k], alpha, scale_log2) :
                    (int32_t)(int8_t)td[k] * (1 << scale_log2);
            }
            if (s_binds[i].trans_idx == 0) {
                s_local[i].t[1] -= (int32_t)root_y;
            }
        }
        if (s_binds[i].rot_idx >= 0) {
            const uint8_t *rd = frame_r + (unsigned)s_binds[i].rot_idx * 9u;
            const uint8_t *second_rd = second_r + (unsigned)s_binds[i].rot_idx * 9u;
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    s_local[i].m[r][c] = pose ? bone_rotation_lerp((int8_t)rd[r * 3 + c], (int8_t)second_rd[r * 3 + c], alpha) :
                        (int32_t)(int8_t)rd[r * 3 + c] * 32;
                }
            }
        }
    }

    /* Anim_BoneUpdate: only the player copies hips translation onto the torso. */
    if (is_player && bone_count > 11) {
        s_local[1].t[0] = s_local[11].t[0];
        s_local[1].t[1] = s_local[11].t[1];
        s_local[1].t[2] = s_local[11].t[2];
    }

    if (!split_twist_bones(s_local, bone_count, twist)) { return false; }
    if (match_arm && (!is_player || !match_arm_pose(s_local, bone_count))) { return false; }
    xform_identity(&s_world[0]);
    for (int i = 1; i < bone_count; i++) {
        const int parent = s_binds[i].parent;
        if (parent < 0 || parent >= bone_count) {
            s_world[i] = s_local[i];
        } else {
            xform_mul(&s_world[i], &s_world[parent], &s_local[i]);
        }
    }

    *bone_count_out = bone_count;
    s_pose_bone_count = (unsigned)bone_count;
    return true;
}

/*
 * HERO.ILM hand swaps (decomp WorldGfx_CharaMeshSwap):
 *   17 06LHAND, 18 06LHAND2, 19 10RHAND, 20 10RHAND2, 21 10RHAND3, 22 10RHAND4
 */
#include "split_head_mesh.h"

static bool model_is_active(unsigned model_index, unsigned flags)
{
    if (flags & (SH_CHARA_F_SPLIT_HEAD_1 | SH_CHARA_F_SPLIT_HEAD_2)) { return split_model_active(model_index, flags); }
    const bool hand =
        model_index == 17u || model_index == 18u || model_index == 19u || model_index == 20u ||
        model_index == 21u || model_index == 22u;
    if (!hand) {
        return true;
    }
    if (flags & SH_CHARA_F_HARRY_MATCH_HAND) {
        return model_index == 17u || model_index == 20u;
    }
    if (flags & SH_CHARA_F_HARRY_GUN) {
        return model_index == 17u || model_index == 21u; /* empty L + gun R */
    }
    if (flags & SH_CHARA_F_HARRY_KNIFE) {
        return model_index == 18u || model_index == 20u; /* melee L+R */
    }
    if (flags & SH_CHARA_F_HARRY_HANDS) {
        return model_index == 17u || model_index == 19u; /* empty */
    }
    return true;
}

bool sh_chara_collect_cluts(const uint8_t *ilm, size_t ilm_size, ShCharaClutMap *clut_map, int tim_src)
{
    if (!ilm || ilm_size < 0x14 || ilm[0] != LM_MAGIC || ilm[1] != LM_VERSION || !clut_map) {
        return false;
    }
    clut_map_defaults(clut_map);
    /* Only the first collect may reset. Later calls (item PLMs) must append. */

    const unsigned model_count = ilm[8];
    const size_t models = rd_u32(ilm + 0xC);
    if (models + (size_t)model_count * LM_MODEL_SIZE > ilm_size) {
        return false;
    }
    for (unsigned mi = 0; mi < model_count; mi++) {
        const uint8_t *model = ilm + models + (size_t)mi * LM_MODEL_SIZE;
        const unsigned mesh_count = model[8];
        const size_t meshes = rd_u32(model + 0xC);
        if (meshes + (size_t)mesh_count * LM_MESH_SIZE > ilm_size) {
            continue;
        }
        for (unsigned m = 0; m < mesh_count; m++) {
            const uint8_t *mesh = ilm + meshes + (size_t)m * LM_MESH_SIZE;
            const unsigned prim_count = mesh[0];
            const size_t prims = rd_u32(mesh + 0x4);
            if (prims + (size_t)prim_count * PRIM_SIZE > ilm_size) {
                continue;
            }
            for (unsigned p = 0; p < prim_count; p++) {
                const uint8_t *prim = ilm + prims + (size_t)p * PRIM_SIZE;
                const unsigned clut_delta = (unsigned)(prim[2] | (prim[3] << 8));
                (void)clut_slot(clut_map, clut_delta, tim_src);
            }
        }
    }
    return clut_map->count > 0;
}

static void emit_plm_meshes(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *plm,
                            size_t plm_size, const BoneXform *bone, int tim_src, const ShTim *hero_tim,
                            const ShTim *held_tim, ShCharaClutMap *clut_map);

static void xform_from_pose(BoneXform *x, float yaw, const float pos[3], float scale)
{
    xform_identity(x);
    const int32_t sc = (int32_t)(scale * (float)Q12_ONE);
    const float c = cosf(yaw);
    const float s = sinf(yaw);
    x->m[0][0] = (int32_t)(c * (float)sc);
    x->m[0][2] = (int32_t)(s * (float)sc);
    x->m[2][0] = (int32_t)(-s * (float)sc);
    x->m[2][2] = (int32_t)(c * (float)sc);
    x->m[1][1] = sc;
    x->t[0] = (int32_t)(pos[0] * 256.0f);
    x->t[1] = (int32_t)(-pos[1] * 256.0f);
    x->t[2] = (int32_t)(-pos[2] * 256.0f);
}

bool sh_chara_bake_harry(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *ilm,
                         size_t ilm_size, const uint8_t *anm, size_t anm_size, const ShTim *tim,
                         int keyframe, ShCharaClutMap *clut_map)
{
    return sh_chara_bake(out, capacity, count_out, ilm, ilm_size, anm, anm_size, tim, keyframe, clut_map,
                         SH_CHARA_F_HARRY_HANDS, NULL, 0, SH_CHARA_WEP_KF_BASE, NULL, NULL, 0, -1);
}

static bool chara_bake(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *ilm, size_t ilm_size,
                   const uint8_t *anm, size_t anm_size, const ShTim *tim, int keyframe, ShCharaClutMap *clut_map,
                   unsigned flags, const uint8_t *wep_kf, size_t wep_kf_size, int wep_kf_base,
                   const ShTim *held_tim, const uint8_t *held_plm, size_t held_plm_size, int held_bone, int16_t twist,
                   const ShAnimPoseTiming *pose)
{
    if (!out || !count_out || capacity < 3 || !ilm || ilm_size < 0x14 || ilm[0] != LM_MAGIC ||
        ilm[1] != LM_VERSION) {
        return false;
    }

    *count_out = 0;
    if (clut_map) {
        /* Keep a pre-seeded atlas map (sh_chara_collect_cluts) so UV slots stay stable. */
        clut_map_defaults(clut_map);
    }
    memset(s_scratch, 0, sizeof(s_scratch));

    int bone_count = 0;
    const bool is_player = (flags & (SH_CHARA_F_HARRY_HANDS | SH_CHARA_F_HARRY_GUN | SH_CHARA_F_HARRY_KNIFE)) != 0;
    if (!build_skeleton(&bone_count, anm, anm_size, keyframe, wep_kf, wep_kf_size, wep_kf_base, is_player, twist, pose,
                        (flags & SH_CHARA_F_HARRY_MATCH_ARM) != 0)) {
        return false;
    }

    const unsigned model_count = ilm[8];
    const size_t models = rd_u32(ilm + 0xC);
    const size_t order_off = rd_u32(ilm + 0x10);
    if (models + (size_t)model_count * LM_MODEL_SIZE > ilm_size) {
        return false;
    }
    /* PS1 draws in modelOrder so joint verts stay in the shared pool for stitching. */
    const bool have_order = order_off != 0 && order_off + model_count <= ilm_size;
    const uint8_t *order = have_order ? ilm + order_off : NULL;

    for (unsigned oi = 0; oi < model_count; oi++) {
        const unsigned mi = order ? order[oi] : oi;
        if (mi >= model_count) {
            continue;
        }
        const uint8_t *model = ilm + models + (size_t)mi * LM_MODEL_SIZE;
        if ((flags & (SH_CHARA_F_HARRY_HANDS | SH_CHARA_F_HARRY_GUN | SH_CHARA_F_HARRY_KNIFE |
                      SH_CHARA_F_SPLIT_HEAD_1 | SH_CHARA_F_SPLIT_HEAD_2)) &&
            !model_is_active(mi, flags)) {
            continue;
        }

        int bone_idx = 0;
        if (model[0] >= '0' && model[0] <= '9' && model[1] >= '0' && model[1] <= '9') {
            bone_idx = (model[0] - '0') * 10 + (model[1] - '0');
        }
        if (bone_idx < 0 || bone_idx >= bone_count) {
            bone_idx = 0;
        }

        const unsigned mesh_count = model[8];
        const unsigned vert_offset = model[9];
        const size_t meshes = rd_u32(model + 0xC);
        if (meshes + (size_t)mesh_count * LM_MESH_SIZE > ilm_size) {
            continue;
        }

        for (unsigned m = 0; m < mesh_count; m++) {
            const uint8_t *mesh = ilm + meshes + (size_t)m * LM_MESH_SIZE;
            const unsigned prim_count = mesh[0];
            const unsigned vert_count = mesh[1];
            const size_t prims = rd_u32(mesh + 0x4);
            const size_t xy = rd_u32(mesh + 0x8);
            const size_t z = rd_u32(mesh + 0xC);

            if (prims + (size_t)prim_count * PRIM_SIZE > ilm_size || xy + (size_t)vert_count * 4u > ilm_size ||
                z + (size_t)vert_count * 2u > ilm_size || vert_offset + vert_count > 512) {
                continue;
            }

            for (unsigned i = 0; i < vert_count; i++) {
                const int32_t lx = rd_s16(ilm + xy + i * 4);
                const int32_t ly = rd_s16(ilm + xy + i * 4 + 2);
                const int32_t lz = rd_s16(ilm + z + i * 2);
                xform_point(&s_world[bone_idx], lx, ly, lz, s_scratch[vert_offset + i]);
            }

            for (unsigned p = 0; p < prim_count; p++) {
                const uint8_t *prim = ilm + prims + (size_t)p * PRIM_SIZE;
                const uint8_t *idx = prim + 0xC;
                const unsigned clut_delta = (unsigned)(prim[2] | (prim[3] << 8));
                if (idx[0] >= 512 || idx[1] >= 512 || idx[2] >= 512) {
                    continue;
                }
                emit_tri(out, capacity, count_out, s_scratch[idx[0]], s_scratch[idx[1]], s_scratch[idx[2]], prim[0],
                         prim[1], prim[4], prim[5], prim[8], prim[9], clut_delta, 0, tim, held_tim, clut_map);
                if (idx[3] != 0xFF && idx[3] < 512) {
                    emit_tri(out, capacity, count_out, s_scratch[idx[1]], s_scratch[idx[3]], s_scratch[idx[2]], prim[4],
                             prim[5], prim[0xA], prim[0xB], prim[8], prim[9], clut_delta, 0, tim, held_tim,
                             clut_map);
                }
            }
        }
    }

    if (held_plm && held_plm_size > 0 && held_bone >= 0 && held_bone < bone_count) {
        const int plm_src = held_tim ? 1 : 0;
        emit_plm_meshes(out, capacity, count_out, held_plm, held_plm_size, &s_world[held_bone], plm_src, tim,
                        held_tim, clut_map);
    }

    return *count_out >= 3;
}

bool sh_chara_bake(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *ilm, size_t ilm_size,
    const uint8_t *anm, size_t anm_size, const ShTim *tim, int keyframe, ShCharaClutMap *clut_map,
    unsigned flags, const uint8_t *wep_kf, size_t wep_kf_size, int wep_kf_base,
    const ShTim *held_tim, const uint8_t *held_plm, size_t held_plm_size, int held_bone)
{
    return chara_bake(out, capacity, count_out, ilm, ilm_size, anm, anm_size, tim, keyframe, clut_map,
        flags, wep_kf, wep_kf_size, wep_kf_base, held_tim, held_plm, held_plm_size, held_bone, 0, NULL);
}

bool sh_chara_bake_split_head(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *ilm, size_t ilm_size,
    const uint8_t *anm, size_t anm_size, const ShTim *tim, int keyframe, ShCharaClutMap *cluts, int variant, int16_t twist)
{
    if (variant != 1 && variant != 2) { return false; }
    const unsigned flags = variant == 1 ? SH_CHARA_F_SPLIT_HEAD_1 : SH_CHARA_F_SPLIT_HEAD_2;
    return chara_bake(out, capacity, count_out, ilm, ilm_size, anm, anm_size, tim, keyframe, cluts,
        flags, NULL, 0, 0, NULL, NULL, 0, -1, twist, NULL);
}

bool sh_chara_bake_split_head_blend(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *ilm, size_t ilm_size,
    const uint8_t *anm, size_t anm_size, const ShTim *tim, const ShAnimPoseTiming *pose,
    ShCharaClutMap *cluts, int variant, int16_t twist)
{
    if (!pose || (variant != 1 && variant != 2)) { return false; }
    const unsigned flags = variant == 1 ? SH_CHARA_F_SPLIT_HEAD_1 : SH_CHARA_F_SPLIT_HEAD_2;
    return chara_bake(out, capacity, count_out, ilm, ilm_size, anm, anm_size, tim, pose->first, cluts,
        flags, NULL, 0, 0, NULL, NULL, 0, -1, twist, pose);
}

static void emit_plm_meshes(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *plm,
                            size_t plm_size, const BoneXform *bone, int tim_src, const ShTim *hero_tim,
                            const ShTim *held_tim, ShCharaClutMap *clut_map)
{
    if (!plm || plm_size < 0x14 || plm[0] != LM_MAGIC || plm[1] != LM_VERSION || !bone) {
        return;
    }
    const unsigned model_count = plm[8];
    const size_t models = rd_u32(plm + 0xC);
    if (models + (size_t)model_count * LM_MODEL_SIZE > plm_size) {
        return;
    }
    for (unsigned mi = 0; mi < model_count; mi++) {
        const uint8_t *model = plm + models + (size_t)mi * LM_MODEL_SIZE;
        const unsigned mesh_count = model[8];
        const unsigned vert_offset = model[9];
        const size_t meshes = rd_u32(model + 0xC);
        if (meshes + (size_t)mesh_count * LM_MESH_SIZE > plm_size) {
            continue;
        }
        for (unsigned m = 0; m < mesh_count; m++) {
            const uint8_t *mesh = plm + meshes + (size_t)m * LM_MESH_SIZE;
            const unsigned prim_count = mesh[0];
            const unsigned vert_count = mesh[1];
            const size_t prims = rd_u32(mesh + 0x4);
            const size_t xy = rd_u32(mesh + 0x8);
            const size_t z = rd_u32(mesh + 0xC);
            if (prims + (size_t)prim_count * PRIM_SIZE > plm_size || xy + (size_t)vert_count * 4u > plm_size ||
                z + (size_t)vert_count * 2u > plm_size || vert_offset + vert_count > 512) {
                continue;
            }
            for (unsigned i = 0; i < vert_count; i++) {
                const int32_t lx = rd_s16(plm + xy + i * 4);
                const int32_t ly = rd_s16(plm + xy + i * 4 + 2);
                const int32_t lz = rd_s16(plm + z + i * 2);
                xform_point(bone, lx, ly, lz, s_scratch[vert_offset + i]);
            }
            for (unsigned p = 0; p < prim_count; p++) {
                const uint8_t *prim = plm + prims + (size_t)p * PRIM_SIZE;
                const uint8_t *idx = prim + 0xC;
                const unsigned clut_delta = (unsigned)(prim[2] | (prim[3] << 8));
                if (idx[0] >= 512 || idx[1] >= 512 || idx[2] >= 512) {
                    continue;
                }
                emit_tri(out, capacity, count_out, s_scratch[idx[0]], s_scratch[idx[1]], s_scratch[idx[2]], prim[0],
                         prim[1], prim[4], prim[5], prim[8], prim[9], clut_delta, tim_src, hero_tim, held_tim,
                         clut_map);
                if (idx[3] != 0xFF && idx[3] < 512) {
                    emit_tri(out, capacity, count_out, s_scratch[idx[1]], s_scratch[idx[3]], s_scratch[idx[2]], prim[4],
                             prim[5], prim[0xA], prim[0xB], prim[8], prim[9], clut_delta, tim_src, hero_tim,
                             held_tim, clut_map);
                }
            }
        }
    }
}

bool sh_chara_bake_prop(ShMapVertex *out, size_t capacity, size_t *count_out, const uint8_t *plm, size_t plm_size,
                        const ShTim *tim, ShCharaClutMap *clut_map, const float pos[3], float yaw, float scale)
{
    if (!out || !count_out || capacity < 3 || !plm || plm_size < 0x14) {
        return false;
    }
    *count_out = 0;
    if (clut_map) {
        clut_map_defaults(clut_map);
    }
    BoneXform root;
    xform_from_pose(&root, yaw, pos, scale);
    emit_plm_meshes(out, capacity, count_out, plm, plm_size, &root, 0, tim, NULL, clut_map);
    return *count_out >= 3;
}
