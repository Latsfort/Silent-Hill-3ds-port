#include "sh3ds/vc.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI_F ((float)M_PI)
#define TURN (2.0f * PI_F)
#define DEG(d) ((d) * (PI_F / 180.0f))

enum { RD_WARP_IN = 1 << 1, RD_WARP_OUT = 1 << 2, RD_NO_ENEMY = 1 << 5, RD_NEAR_ENEMY = 1 << 6, RD_MARGE = 1 << 7 };
enum { AREA_TINY = 0, AREA_SMALL = 1, AREA_WIDE = 2 };
enum { TYPE_ROAD = 0, TYPE_EFFECT = 1, TYPE_EVENT = 2 };
enum { MV_CHASE = 0, MV_SETTLE = 1, MV_FIX_ANG = 2, MV_SELF_VIEW = 3, MV_THROUGH_DOOR = 4 };

#define MIN_IN_ROAD_DIST 1.0f
#define NEAR_ROAD_HALF_W 20.0f
#define HERO_HEIGHT 1.7f
#define MAX_NEAR 32

/* Harry's chara_top_y offset and the screen half angles: ratan2(240 or 320, 240) >> 1. */
#define SCR_HALF_ANG_WX (0.5f * 0.927295f)
#define SCR_HALF_ANG_WY (0.5f * 0.785398f)

/* vcNullNearRoad stand-in: free chase camera when no road covers Harry. */
static const ShVcRoad s_null_road = {
    {-1e6f, 1e6f, -1e6f, 1e6f}, {-1e6f, 1e6f, -1e6f, 1e6f}, 0, AREA_WIDE, TYPE_ROAD, MV_CHASE, 6.0f, -6.0f, 0.5f,
    MV_CHASE, 0.0f, 0.0f};

typedef struct NearRoad {
    const ShVcRoad *road;
    int idx;
    float sum_dist;
    int dir_x;
    int prio;
} NearRoad;

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float norm_ang(float a)
{
    while (a > PI_F) {
        a -= TURN;
    }
    while (a < -PI_F) {
        a += TURN;
    }
    return a;
}

/* vcGetXZSumDistFromLimArea */
static float sum_dist(float x, float z, const float lim[4], bool can_ret_minus, float *vx, float *vz)
{
    float xd, zd, ox = 0.0f, oz = 0.0f;
    if (lim[1] < x) {
        ox = lim[1] - x;
        xd = -ox;
    } else if (x < lim[0]) {
        xd = lim[0] - x;
        ox = xd;
    } else {
        xd = x >= 0.5f * (lim[0] + lim[1]) ? x - lim[1] : lim[0] - x;
    }
    if (lim[3] < z) {
        oz = lim[3] - z;
        zd = -oz;
    } else if (z < lim[2]) {
        zd = lim[2] - z;
        oz = zd;
    } else {
        zd = z >= 0.5f * (lim[2] + lim[3]) ? z - lim[3] : lim[2] - z;
    }
    float r;
    if (xd >= 0.0f) {
        r = xd;
        if (zd >= 0.0f) {
            r += zd;
        }
    } else {
        r = zd;
        if (r < 0.0f && r < xd) {
            r = xd;
        }
    }
    if (!can_ret_minus && r < 0.0f) {
        r = 0.0f;
    }
    if (vx) {
        *vx = ox;
    }
    if (vz) {
        *vz = oz;
    }
    return r;
}

/* vcAdjustXzInLimAreaUsingMIN_IN_ROAD_DIST */
static void clamp_in_road(float *x, float *z, const float lim[4])
{
    float min_x = lim[0] + MIN_IN_ROAD_DIST, max_x = lim[1] - MIN_IN_ROAD_DIST;
    float min_z = lim[2] + MIN_IN_ROAD_DIST, max_z = lim[3] - MIN_IN_ROAD_DIST;
    if (max_x < min_x) {
        min_x = max_x = 0.5f * (min_x + max_x);
    }
    if (max_z < min_z) {
        min_z = max_z = 0.5f * (min_z + max_z);
    }
    *x = clampf(*x, min_x, max_x);
    *z = clampf(*z, min_z, max_z);
}

static int collect_near(const ShVc *vc, float x, float z, bool near_enemy, NearRoad *out)
{
    int n = 0;
    for (int i = 0; i < vc->road_count && n < MAX_NEAR; i++) {
        const ShVcRoad *r = &vc->roads[i];
        if (r->flags & (near_enemy ? RD_NO_ENEMY : RD_NEAR_ENEMY)) {
            continue;
        }
        const float lo_x = x - NEAR_ROAD_HALF_W, hi_x = x + NEAR_ROAD_HALF_W;
        const float lo_z = z - NEAR_ROAD_HALF_W, hi_z = z + NEAR_ROAD_HALF_W;
        const bool in_sw = hi_x >= r->sw[0] && r->sw[1] >= lo_x && hi_z >= r->sw[2] && r->sw[3] >= lo_z;
        const bool in_rd = hi_x >= r->rd[0] && r->rd[1] >= lo_x && hi_z >= r->rd[2] && r->rd[3] >= lo_z;
        if (!in_sw && !in_rd) {
            continue;
        }
        NearRoad *nr = &out[n++];
        nr->road = r;
        nr->idx = i;
        nr->sum_dist = sum_dist(x, z, r->sw, (r->flags & RD_MARGE) != 0, NULL, NULL);
        nr->dir_x = (r->rd[3] - r->rd[2]) < (r->rd[1] - r->rd[0]);
        nr->prio = r->rd_type == TYPE_EVENT ? 2 : (r->rd_type == TYPE_EFFECT ? 1 : 0);
    }
    return n;
}

/* vcGetNearestNEAR_ROAD_DATA (switch areas) */
static const NearRoad *nearest_of_type(const NearRoad *ary, int n, int type, float x, float z, float *dist)
{
    const NearRoad *best = NULL;
    float best_d = 1e30f;
    for (int i = 0; i < n; i++) {
        if (ary[i].road->rd_type != type) {
            continue;
        }
        const float d = sum_dist(x, z, ary[i].road->sw, (ary[i].road->flags & RD_MARGE) != 0, NULL, NULL);
        if (best_d >= d) {
            best_d = d;
            best = &ary[i];
        }
    }
    *dist = best_d;
    return best;
}

/* vcGetBestNewCurNearRoad */
static const NearRoad *best_new(const NearRoad *ary, int n, float x, float z, float *out_d)
{
    static const int order[3] = {TYPE_EVENT, TYPE_ROAD, TYPE_EFFECT};
    const NearRoad *cur = NULL;
    float cur_d = 1e30f;
    int cur_p = 0;
    for (int k = 0; k < 3; k++) {
        float d;
        const NearRoad *nr = nearest_of_type(ary, n, order[k], x, z, &d);
        if (!nr) {
            continue;
        }
        bool renew;
        if (cur_p < nr->prio) {
            renew = d <= 0.0f || d < cur_d;
        } else if (nr->prio >= cur_p) {
            renew = d < cur_d;
        } else {
            renew = cur_d > 0.0f && d < cur_d;
        }
        if (renew) {
            cur = nr;
            cur_d = d;
            cur_p = nr->prio;
        }
    }
    *out_d = cur_d;
    return cur;
}

static const ShVcRoad *cur_road(const ShVc *vc)
{
    return vc->cur >= 0 && vc->cur < vc->road_count ? &vc->roads[vc->cur] : &s_null_road;
}

/* vcSetCurNearRoadInVC_WORK; returns warp. */
static bool select_road(ShVc *vc, float x, float z, float mv_ang, bool near_enemy)
{
    NearRoad ary[MAX_NEAR];
    const int n = collect_near(vc, x, z, near_enemy, ary);
    float new_d;
    const NearRoad *nw = best_new(ary, n, x, z, &new_d);
    const ShVcRoad *old_road = cur_road(vc);
    const int new_idx = nw ? nw->idx : -1;
    const unsigned new_flags = nw ? nw->road->flags : 0;
    const int new_prio = nw ? nw->prio : 0;

    const NearRoad *old = NULL;
    for (int i = 0; i < n; i++) {
        if (ary[i].idx == vc->cur) {
            old = &ary[i];
        }
    }

    bool warp = false;
    if (!old) {
        warp = (new_flags & RD_WARP_IN) || (old_road->flags & RD_WARP_OUT);
        vc->cur = new_idx;
        return warp;
    }

    const float adv = old->road->rd_type == TYPE_EVENT ? 0.15f : 0.35f;
    if (new_prio < old->prio && old->sum_dist < adv * 2.0f) {
        return false;
    }
    if (old->prio < new_prio && nw && nw->sum_dist <= 0.0f) {
        warp = (new_flags & RD_WARP_IN) || (old_road->flags & RD_WARP_OUT);
        vc->cur = new_idx;
        return warp;
    }

    const float rd_ang = old->dir_x ? DEG(90.0f) : 0.0f;
    float ofs = norm_ang(mv_ang - rd_ang);
    if (ofs < 0.0f) {
        ofs += PI_F;
    }
    if (ofs > DEG(90.0f)) {
        ofs = PI_F - ofs;
    }
    if (new_d >= old->sum_dist - adv) {
        return false;
    }
    if (old->sum_dist < 0.0f && ofs < DEG(20.0f)) {
        return false;
    }
    if (new_idx == vc->cur) {
        return false;
    }
    warp = (new_flags & RD_WARP_IN) || (old_road->flags & RD_WARP_OUT);
    vc->cur = new_idx;
    return warp;
}

/* vcMakeIdealCamPosUseVC_ROAD_DATA */
static void ideal_road(ShVc *vc, const ShVcRoad *r, const float pos[3], float face, float ang_spd, float out[3])
{
    const float base = face + PI_F;
    float delta = norm_ang(vc->ideal_ang - base);
    const float range = DEG(12.0f);
    if (fabsf(ang_spd) > DEG(20.0f)) {
        delta = clampf(delta, -range, range);
    } else {
        delta = delta >= 0.0f ? range : -range;
    }
    vc->ideal_ang = norm_ang(delta + base);

    const float def = r->area == AREA_TINY ? 1.8f : (r->area == AREA_SMALL ? 2.0f : 2.5f);
    out[1] = clampf(pos[1] - HERO_HEIGHT - 0.4f, r->min_hy, r->max_hy);

    float dy = fabsf(vc->cam_pos[1] - pos[1]) - 1.5f;
    if (dy < 0.0f) {
        dy = 0.0f;
    }
    float tx = pos[0], tz = pos[2];
    clamp_in_road(&tx, &tz, r->rd);
    const float h = sqrtf((tx - pos[0]) * (tx - pos[0]) + dy * dy + (tz - pos[2]) * (tz - pos[2]));

    float bmin, bmax;
    if (r->area == AREA_TINY) {
        bmin = 0.7f;
        bmax = 2.0f;
    } else if (r->area < AREA_WIDE) {
        bmin = 0.7f;
        bmax = 3.0f;
    } else {
        bmin = 2.0f;
        bmax = 5.0f;
    }
    float dist;
    if (bmax >= h) {
        /* Q8(6.4f) against a Q12 distance is 0.4 m. */
        dist = bmin < h ? def + (0.4f - def) * (h - bmin) / (bmax - bmin) : def;
    } else {
        dist = 0.4f;
    }
    out[0] = pos[0] + dist * sinf(vc->ideal_ang);
    out[2] = pos[2] + dist * cosf(vc->ideal_ang);
    clamp_in_road(&out[0], &out[2], r->rd);
}

/* vcMakeIdealCamPosForFixAngCam */
static void ideal_fix(const ShVc *vc, const ShVcRoad *r, const float pos[3], float face, float out[3])
{
    float vx, vz;
    sum_dist(pos[0], pos[2], r->rd, false, &vx, &vz);
    const float off = 0.5f * fmaxf(fabsf(vx), fabsf(vz)) + 1.5f;
    const float d = hypotf(pos[0] - vc->cam_pos[0], pos[2] - vc->cam_pos[2]);
    const float fwd = d >= 7.0f ? 0.0f : (d > 1.5f ? (d - 7.0f) / -5.5f * 0.7f : 0.7f);
    const float fy = DEG(r->fix_y_deg) + PI_F;
    out[0] = pos[0] + fwd * sinf(face) + off * sinf(fy);
    out[2] = pos[2] + fwd * cosf(face) + off * cosf(fy);
    out[1] = pos[1];
    clamp_in_road(&out[0], &out[2], r->rd);
}

/* vcAdjTgtMvVecYByCurNearRoad */
static float clamp_tgt_y(const ShVc *vc, const ShVcRoad *r, const float pos[3], float y)
{
    const float top = pos[1] - HERO_HEIGHT;
    const float to_chara = hypotf(pos[0] - vc->cam_tgt[0], pos[2] - vc->cam_tgt[2]);
    const float d = clampf(to_chara, 1.2f, 7.0f);
    const float near = clampf((7.0f - d) * 0.1724f, 0.0f, 1.0f);
    float lo, hi;
    switch (r->mv_y) {
    case MV_SETTLE:
        lo = hi = r->max_hy * (1.0f - near) + r->min_hy * near;
        break;
    case MV_FIX_ANG:
        lo = hi = r->min_hy * (1.0f - near) + r->max_hy * near;
        break;
    case MV_SELF_VIEW:
        lo = r->min_hy;
        hi = r->max_hy;
        break;
    default: {
        float ofs = (to_chara - 0.3f) * 0.25f;
        if (ofs < 0.0f) {
            ofs = 0.0f;
        }
        hi = ofs + top - 0.25f;
        lo = top - ofs - 0.25f;
        break;
    }
    }
    return clampf(y, lo, hi);
}

/* vwRetNewVelocityToTargetVal */
static float vel_to_target(float spd, float diff, float accel, float max_spd, float dec, float dt)
{
    spd += diff >= 0.0f ? accel * dt : -accel * dt;
    spd = clampf(spd, -max_spd, max_spd);
    const float lim = dec * fabsf(diff);
    return diff >= 0.0f ? fminf(spd, lim) : fmaxf(spd, -lim);
}

/* vcRenewalCamData with cam_mv_prm_user = {10, 3, 6, 2}. */
static void move_cam_params(ShVc *vc, float dt, float accel_xz, float accel_y, float max_xz, float max_y)
{
    const float dx = vc->cam_tgt[0] - vc->cam_pos[0];
    const float dz = vc->cam_tgt[2] - vc->cam_pos[2];
    const float a = atan2f(dx, dz);
    const float sx = sinf(a), sz = cosf(a);

    vc->cam_vel[0] += accel_xz * dt * sx;
    vc->cam_vel[2] += accel_xz * dt * sz;
    const float sp = hypotf(vc->cam_vel[0], vc->cam_vel[2]);
    if (sp > max_xz) {
        vc->cam_vel[0] *= max_xz / sp;
        vc->cam_vel[2] *= max_xz / sp;
    }
    float to_tgt = accel_xz * 0.4f * (hypotf(dx, dz) - 0.1f);
    if (to_tgt < 0.0f) {
        to_tgt = 0.0f;
    }
    /* vwLimitOverLimVector */
    const float fwd = vc->cam_vel[0] * sx + vc->cam_vel[2] * sz - to_tgt;
    if (fwd > 0.0f) {
        vc->cam_vel[0] -= fwd * sx;
        vc->cam_vel[2] -= fwd * sz;
    }
    /* vwDecreaseSideOfVector */
    const float px = sinf(a + DEG(90.0f)), pz = cosf(a + DEG(90.0f));
    const float side0 = vc->cam_vel[0] * px + vc->cam_vel[2] * pz;
    float side = clampf(side0, -0.5f * to_tgt, 0.5f * to_tgt);
    const float dec = 12.0f * dt;
    if (dec < side) {
        side -= dec;
    } else if (side >= -dec) {
        side = 0.0f;
    } else {
        side += dec;
    }
    vc->cam_vel[0] += (side - side0) * px;
    vc->cam_vel[2] += (side - side0) * pz;

    vc->cam_vel[1] = vel_to_target(vc->cam_vel[1], vc->cam_tgt[1] - vc->cam_pos[1], accel_y, max_y, accel_y, dt);

    for (int i = 0; i < 3; i++) {
        vc->cam_pos[i] += vc->cam_vel[i] * dt;
    }
}

static void move_cam(ShVc *vc, float dt)
{
    move_cam_params(vc, dt, 10.0f, 3.0f, 6.0f, 2.0f);
}

#include "script_camera_data.h"

void sh_vc_script_update(ShVc *vc, const float position[3], const float look[3],
                         const ShVcScriptParams *params, bool warp_camera,
                         bool warp_watch, float dt, ShCamera *out)
{
    ShVcScriptParams resolved = s_script_camera_defaults;
    if (params) {
        for (int i = 0; i < 4; i++) {
            if (params->camera[i] != 0.0f) { resolved.camera[i] = params->camera[i]; }
            if (params->watch[i] != 0.0f) { resolved.watch[i] = params->watch[i]; }
        }
    }
    memcpy(vc->cam_tgt, position, sizeof(vc->cam_tgt));
    if (warp_camera) {
        memcpy(vc->cam_pos, position, sizeof(vc->cam_pos));
        memset(vc->cam_vel, 0, sizeof(vc->cam_vel));
    } else {
        move_cam_params(vc, dt, resolved.camera[0], resolved.camera[1], resolved.camera[2], resolved.camera[3]);
    }
    const float dx = look[0] - vc->cam_pos[0], dz = look[2] - vc->cam_pos[2];
    const float target[2] = {atan2f(vc->cam_pos[1] - look[1], hypotf(dx, dz)), atan2f(dx, dz)};
    for (int i = 0; i < 2; i++) {
        if (warp_watch) {
            vc->ang[i] = target[i];
            vc->ang_spd[i] = 0.0f;
        } else {
            vc->ang_spd[i] = vel_to_target(vc->ang_spd[i], norm_ang(target[i] - vc->ang[i]),
                                          resolved.watch[i] * TURN, resolved.watch[i + 2] * TURN,
                                          resolved.watch[i] * (i == 0 ? 8.0f : 3.0f), dt);
            vc->ang[i] = norm_ang(vc->ang[i] + vc->ang_spd[i] * dt);
        }
    }
    out->pos[0] = vc->cam_pos[0];
    out->pos[1] = -vc->cam_pos[1];
    out->pos[2] = -vc->cam_pos[2];
    out->yaw = -vc->ang[1];
    out->pitch = vc->ang[0];
    out->fov_deg = SH_VC_FOV_DEG;
}

/* vcUserCamTarget: override position while retaining automatic watch behavior. */
void sh_vc_camera_target(ShVc *vc, const float position[3], const ShVcScriptParams *params, bool warp)
{
    vc->user_camera = true;
    vc->user_camera_warp = warp;
    memcpy(vc->user_camera_target, position, sizeof(vc->user_camera_target));
    for (int i = 0; i < 4; i++) {
        vc->user_camera_params[i] = params && params->camera[i] != 0.0f ? params->camera[i] : s_script_camera_defaults.camera[i];
    }
}

void sh_vc_camera_release(ShVc *vc, bool warp)
{
    vc->user_camera = false;
    vc->user_camera_warp = false;
    if (warp) { sh_vc_warp(vc); }
}

void sh_vc_init(ShVc *vc, const ShVcRoad *roads, int road_count)
{
    memset(vc, 0, sizeof(*vc));
    vc->roads = roads;
    vc->road_count = road_count;
    vc->cur = -1;
    vc->warp = true;
}

void sh_vc_warp(ShVc *vc)
{
    if (vc) {
        vc->warp = true;
    }
}

void sh_vc_update(ShVc *vc, const float pos[3], float face, bool near_enemy, float dt, ShCamera *out)
{
    if (!vc || !pos || !out) {
        return;
    }
    const bool first = vc->warp;
    float mv_spd = 0.0f, ang_spd = 0.0f, mv_ang = face;
    if (!first && dt > 0.0f) {
        const float mx = pos[0] - vc->prev_pos[0], mz = pos[2] - vc->prev_pos[2];
        mv_spd = hypotf(mx, mz) / dt;
        if (mv_spd > 0.05f) {
            mv_ang = atan2f(mx, mz);
        }
        ang_spd = norm_ang(face - vc->prev_face) / dt;
    }
    if (first) {
        vc->ideal_ang = face + PI_F;
    }

    bool warp = select_road(vc, pos[0], pos[2], mv_ang, near_enemy) || first;
    const ShVcRoad *r = cur_road(vc);

    if (vc->user_camera) {
        warp = first;
        memcpy(vc->cam_tgt, vc->user_camera_target, sizeof(vc->cam_tgt));
        if (first || vc->user_camera_warp) {
            memcpy(vc->cam_pos, vc->cam_tgt, sizeof(vc->cam_pos));
            memset(vc->cam_vel, 0, sizeof(vc->cam_vel));
        } else {
            move_cam_params(vc, dt, vc->user_camera_params[0], vc->user_camera_params[1],
                vc->user_camera_params[2], vc->user_camera_params[3]);
        }
        vc->user_camera_warp = false;
        goto watch_target;
    }

    float ideal[3];
    if (r->cam_mv == MV_FIX_ANG) {
        ideal_fix(vc, r, pos, face, ideal);
    } else {
        ideal_road(vc, r, pos, face, ang_spd, ideal);
    }

    if (warp) {
        memcpy(vc->cam_tgt, ideal, sizeof(ideal));
    }
    {
        float limit = mv_spd + 1.0f + fabsf(ang_spd / TURN * 8.0f);
        limit = clampf(limit, 2.2f, 6.0f) * dt;
        float tx = ideal[0] - vc->cam_tgt[0], tz = ideal[2] - vc->cam_tgt[2];
        const float l = hypotf(tx, tz);
        if (l >= limit && l > 1e-5f) {
            tx *= limit / l;
            tz *= limit / l;
        }
        float ty = ideal[1] - vc->cam_tgt[1];
        if (r->cam_mv != MV_THROUGH_DOOR) {
            ty = clamp_tgt_y(vc, r, pos, vc->cam_tgt[1] + ty) - vc->cam_tgt[1];
        }
        vc->cam_tgt[0] += tx;
        vc->cam_tgt[1] += ty;
        vc->cam_tgt[2] += tz;
    }

    if (warp) {
        memcpy(vc->cam_pos, vc->cam_tgt, sizeof(vc->cam_pos));
        memset(vc->cam_vel, 0, sizeof(vc->cam_vel));
    } else {
        move_cam(vc, dt);
    }

watch_target:;
    /* vcMakeNormalWatchTgtPos → target angles (pitch up-positive, PS1 yaw). */
    float tgt_pitch, tgt_yaw;
    if (r->cam_mv == MV_FIX_ANG) {
        tgt_pitch = norm_ang(DEG(r->fix_x_deg));
        tgt_yaw = norm_ang(DEG(r->fix_y_deg));
    } else {
        const float d = hypotf(pos[0] - vc->cam_pos[0], pos[2] - vc->cam_pos[2]);
        const bool small = r->area == AREA_TINY || r->area == AREA_SMALL;
        const float cir_r = small ? 1.0f : 2.5f;
        const float cir_ext = small ? 3.0f : 5.5f;
        float cir = 0.0f;
        if ((r->cam_mv == MV_CHASE || r->cam_mv == MV_THROUGH_DOOR) && d < cir_r + cir_ext) {
            cir = cir_r + cir_ext - d;
        }
        const float c2c = atan2f(pos[0] - vc->cam_pos[0], pos[2] - vc->cam_pos[2]);
        const float wx = pos[0] + cir * sinf(c2c) + cir_r * sinf(face);
        const float wz = pos[2] + cir * cosf(c2c) + cir_r * cosf(face);
        const float wy = pos[1] + r->ofs_watch_hy;
        const float hx = wx - vc->cam_pos[0], hz = wz - vc->cam_pos[2];
        tgt_yaw = atan2f(hx, hz);
        tgt_pitch = atan2f(vc->cam_pos[1] - wy, hypotf(hx, hz));
    }

    /* vcAdjCamOfsAngByCharaInScreen */
    {
        const float hx = pos[0] - vc->cam_pos[0], hz = pos[2] - vc->cam_pos[2];
        const float hd = hypotf(hx, hz);
        const float btm = atan2f(vc->cam_pos[1] - pos[1], hd) - tgt_pitch;
        const float top = atan2f(vc->cam_pos[1] - (pos[1] - HERO_HEIGHT), hd) - tgt_pitch;
        const float oy = norm_ang(atan2f(hx, hz) - tgt_yaw);
        float adj_y = 0.0f;
        if (oy > SCR_HALF_ANG_WX) {
            adj_y = oy - SCR_HALF_ANG_WX;
        } else if (-SCR_HALF_ANG_WX > oy) {
            adj_y = SCR_HALF_ANG_WX + oy;
        }
        float adj_x = btm >= -SCR_HALF_ANG_WY ? 0.0f : btm + SCR_HALF_ANG_WY;
        if (SCR_HALF_ANG_WY < top - adj_x) {
            adj_x = top - SCR_HALF_ANG_WY;
        }
        adj_x = clampf(adj_x, DEG(-30.0f), DEG(30.0f));
        tgt_yaw = norm_ang(tgt_yaw + adj_y);
        tgt_pitch += adj_x;
    }

    /* vcAdjCamOfsAngByOfsAngSpd with deflt_watch_mv_prm = {1.2, 2.2, 0.22, 0.7} turns. */
    if (warp) {
        vc->ang[0] = tgt_pitch;
        vc->ang[1] = tgt_yaw;
        vc->ang_spd[0] = vc->ang_spd[1] = 0.0f;
    } else {
        vc->ang_spd[0] = vel_to_target(vc->ang_spd[0], norm_ang(tgt_pitch - vc->ang[0]), 1.2f * TURN, 0.22f * TURN,
                                       1.2f * 8.0f, dt);
        vc->ang_spd[1] = vel_to_target(vc->ang_spd[1], norm_ang(tgt_yaw - vc->ang[1]), 2.2f * TURN, 0.7f * TURN,
                                       2.2f * 3.0f, dt);
        vc->ang[0] = norm_ang(vc->ang[0] + vc->ang_spd[0] * dt);
        vc->ang[1] = norm_ang(vc->ang[1] + vc->ang_spd[1] * dt);
    }

    vc->prev_pos[0] = pos[0];
    vc->prev_pos[1] = pos[1];
    vc->prev_pos[2] = pos[2];
    vc->prev_face = face;
    vc->warp = false;

    out->pos[0] = vc->cam_pos[0];
    out->pos[1] = -vc->cam_pos[1];
    out->pos[2] = -vc->cam_pos[2];
    out->yaw = -vc->ang[1];
    out->pitch = vc->ang[0];
    out->fov_deg = SH_VC_FOV_DEG;
}
