#include "sh3ds/cafe.h"
#include "sh3ds/controls.h"
#include "sh3ds/fixed.h"
#include "sh3ds/snd.h"
#include "sh3ds/sound.h"

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI_F 3.14159265f
#define DEG(x) ((x) * (PI_F / 180.0f))

#ifndef SH_CAFE_ANIM_DIR
#define SH_CAFE_ANIM_DIR "romfs:/game/anim"
#endif
#define SH_CAFE_DMS_PATH SH_CAFE_ANIM_DIR "/CAFE.DMS"
#define SH_CAFE2_DMS_PATH SH_CAFE_ANIM_DIR "/CAFE2.DMS"

/* Air Screamer (air_screamer.c) */
#define SH_BIRD_BASE_HEALTH 380.0f
#define SH_BIRD_BITE_DAMAGE 8.0f
#define SH_BIRD_FLY_HEIGHT 1.55f
#define SH_BIRD_SPEED 3.2f
#define SH_BIRD_DIVE_SPEED 5.5f
#define SH_HANDGUN_DAMAGE 95.0f
#define SH_KNIFE_DAMAGE 30.0f
#define SH_HANDGUN_RANGE 9.0f
#define SH_KNIFE_RANGE 1.45f
#define SH_HANDGUN_AMMO 15

/* ---- MAP_MESSAGES (map0_s01_events.c), index 15.. ------------------------- */

typedef struct CafeMsg {
    const char *text;
    float secs; /* ~J0 duration, 0 = wait for input */
    bool end;   /* ~E terminates the chain */
} CafeMsg;

#define MSG_BASE 0
#include "cafe_messages.h"
#define MSG_COUNT ((int)(sizeof(s_msgs) / sizeof(s_msgs[0])))

static const CafeMsg *msg_get(int idx)
{
    idx -= MSG_BASE;
    if (idx < 0 || idx >= MSG_COUNT) {
        return NULL;
    }
    return &s_msgs[idx];
}

/* ---- helpers ------------------------------------------------------------- */

static float wrap_angle(float a)
{
    while (a > PI_F) {
        a -= 2.0f * PI_F;
    }
    while (a < -PI_F) {
        a += 2.0f * PI_F;
    }
    return a;
}

/* PS1 (x, y down, z) → 3DS (x, y up, -z). */
static void ps1_to_3ds(const float in[3], float out[3])
{
    out[0] = in[0];
    out[1] = -in[1];
    out[2] = -in[2];
}

/* Sfx_WithFlagsPlay at a PS1-space position. */
static void snd_at_ps1(uint16_t sfx, float x, float y, float z, float vol)
{
    const float in[3] = {x, y, z};
    float pos[3];
    ps1_to_3ds(in, pos);
    sh_snd_at(sfx, pos, vol);
}

/* PS1 yaw (0 faces +Z) → 3DS yaw (0 faces -Z). */
static float ps1_yaw_to_3ds(float yaw_ps1)
{
    return wrap_angle(-yaw_ps1);
}

/*
 * Cafe vc_road_data.h entries (map0_s01), PS1 metres: lim boxes are {min_x, max_x, min_z, max_z}.
 * Camera heights are metres above the floor (-lim_rd_*_hy).
 */
typedef enum CafeRdType { CAFE_RD_ROAD = 0, CAFE_RD_EFFECT = 1, CAFE_RD_EVENT = 2 } CafeRdType; /* = use priority */
typedef enum CafeCamMv { CAFE_CAM_FIX = 0, CAFE_CAM_CHASE = 1 } CafeCamMv;

#define CAFE_RD_NO_ENEMY (1u << 0)   /* VC_RD_USE_NO_ENEMY_F */
#define CAFE_RD_NEAR_ENEMY (1u << 1) /* VC_RD_USE_NEAR_ENEMY_F */
#define CAFE_RD_WARP_IN (1u << 2)
#define CAFE_RD_WARP_OUT (1u << 3)

typedef struct CafeRoad {
    float sw[4];
    float rd[4];
    float cam_y_lo, cam_y_hi;
    float fix_pitch_deg; /* fix_ang_x, FIX only */
    float fix_yaw_deg;   /* fix_ang_y, FIX only */
    CafeRdType type;
    CafeCamMv mv;
    unsigned flags;
} CafeRoad;

static const CafeRoad s_cafe_roads[] = {
    /* booth by the counter */
    {{4.1f, 4.8f, 265.0f, 267.44f}, {4.0f, 4.0f, 265.8f, 269.2f}, 2.1f, 2.1f, 323.0f, 141.0f, CAFE_RD_EVENT,
     CAFE_CAM_FIX, CAFE_RD_NO_ENEMY | CAFE_RD_WARP_IN | CAFE_RD_WARP_OUT},
    /* middle of the room */
    {{0.0f, 5.0f, 266.1f, 271.8f}, {3.6f, 3.6f, 268.2f, 274.0f}, 1.82f, 1.82f, 0.0f, 0.0f, CAFE_RD_ROAD,
     CAFE_CAM_CHASE, CAFE_RD_NO_ENEMY},
    /* back of the cafe, radio table */
    {{0.0f, 5.0f, 271.8f, 274.5f}, {6.1f, 6.1f, 265.0f, 274.94f}, 2.1f, 2.1f, 330.0f, 280.0f, CAFE_RD_EVENT,
     CAFE_CAM_FIX, CAFE_RD_NO_ENEMY | CAFE_RD_WARP_IN | CAFE_RD_WARP_OUT},
    /* window side while the Air Screamer is around */
    {{1.3f, 6.2f, 265.0f, 268.2f}, {1.9f, 1.9f, 273.2f, 273.2f}, 1.2f, 1.2f, 0.0f, 0.0f, CAFE_RD_EFFECT,
     CAFE_CAM_CHASE, CAFE_RD_NEAR_ENEMY},
    /* main floor while the Air Screamer is around */
    {{1.3f, 6.2f, 268.2f, 275.0f}, {2.94f, 2.94f, 266.5f, 266.5f}, 2.9f, 6.0f, 0.0f, 0.0f, CAFE_RD_ROAD,
     CAFE_CAM_CHASE, CAFE_RD_NEAR_ENEMY | CAFE_RD_WARP_OUT},
};
#define CAFE_ROAD_COUNT ((int)(sizeof(s_cafe_roads) / sizeof(s_cafe_roads[0])))

static float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/* vcGetXZSumDistFromLimArea without VC_RD_MARGE_ROAD_F: 0 inside, Manhattan distance outside. */
static float road_sw_dist(const CafeRoad *r, float x, float z)
{
    const float ox = x < r->sw[0] ? r->sw[0] - x : (x > r->sw[1] ? x - r->sw[1] : 0.0f);
    const float oz = z < r->sw[2] ? r->sw[2] - z : (z > r->sw[3] ? z - r->sw[3] : 0.0f);
    return ox + oz;
}

static bool road_usable(const CafeRoad *r, bool near_enemy)
{
    return !(r->flags & (near_enemy ? CAFE_RD_NO_ENEMY : CAFE_RD_NEAR_ENEMY));
}

/* vcGetBestNewCurNearRoad + vcSetCurNearRoadInVC_WORK. x/z in PS1 metres. */
static int cafe_road_select(ShCafe *cafe, float x, float z, bool near_enemy, bool *warp)
{
    int nearest[3] = {-1, -1, -1};
    float nearest_d[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < CAFE_ROAD_COUNT; i++) {
        const CafeRoad *r = &s_cafe_roads[i];
        if (!road_usable(r, near_enemy)) {
            continue;
        }
        const float d = road_sw_dist(r, x, z);
        if (nearest[r->type] < 0 || d < nearest_d[r->type]) {
            nearest[r->type] = i;
            nearest_d[r->type] = d;
        }
    }

    static const CafeRdType order[3] = {CAFE_RD_EVENT, CAFE_RD_ROAD, CAFE_RD_EFFECT};
    int new_i = -1;
    float new_d = 1e9f;
    int new_p = 0;
    for (int k = 0; k < 3; k++) {
        const int t = order[k];
        if (nearest[t] < 0) {
            continue;
        }
        const float d = nearest_d[t];
        bool renew;
        if (new_p < t) {
            renew = d <= 0.0f || d < new_d;
        } else if (t >= new_p) {
            renew = d < new_d;
        } else {
            renew = new_d > 0.0f && d < new_d;
        }
        if (renew) {
            new_i = nearest[t];
            new_d = d;
            new_p = t;
        }
    }

    *warp = false;
    if (new_i < 0) {
        return cafe->cam_road_idx;
    }
    const int old_i = cafe->cam_road_idx;
    if (old_i < 0 || old_i >= CAFE_ROAD_COUNT || !road_usable(&s_cafe_roads[old_i], near_enemy)) {
        if (old_i >= 0 && old_i < CAFE_ROAD_COUNT && old_i != new_i) {
            *warp = (s_cafe_roads[new_i].flags & CAFE_RD_WARP_IN) || (s_cafe_roads[old_i].flags & CAFE_RD_WARP_OUT);
        }
        cafe->cam_road_idx = new_i;
        return new_i;
    }

    const CafeRoad *old = &s_cafe_roads[old_i];
    const float adv = old->type == CAFE_RD_EVENT ? 0.15f : 0.35f; /* vcAdvantageDistOfOldCurRoad */
    const float old_d = road_sw_dist(old, x, z);
    const int old_p = old->type;
    bool keep;
    if (new_p < old_p && old_d < adv * 2.0f) {
        keep = true;
    } else if (old_p < new_p && new_d <= 0.0f) {
        keep = false;
    } else {
        keep = new_d >= old_d - adv;
    }
    if (keep || new_i == old_i) {
        return old_i;
    }
    *warp = (s_cafe_roads[new_i].flags & CAFE_RD_WARP_IN) || (old->flags & CAFE_RD_WARP_OUT);
    cafe->cam_road_idx = new_i;
    return new_i;
}

/* Look-at helper shared by road cameras. */
static void cam_look_at(ShCamera *cam, float lx, float ly, float lz)
{
    const float dx = lx - cam->pos[0];
    const float dy = ly - cam->pos[1];
    const float dz = lz - cam->pos[2];
    const float horiz = sqrtf(dx * dx + dz * dz);
    cam->yaw = atan2f(-dx, -dz);
    cam->pitch = horiz > 1e-4f ? atan2f(dy, horiz) : 0.0f;
}

/* vcAdjustXzInLimAreaUsingMIN_IN_ROAD_DIST: shrink by 1 m, collapse thin rails to their centre. */
static void road_clamp_in(const float lim[4], float *x, float *z)
{
    float min_x = lim[0] + 1.0f, max_x = lim[1] - 1.0f;
    float min_z = lim[2] + 1.0f, max_z = lim[3] - 1.0f;
    if (max_x < min_x) {
        min_x = max_x = 0.5f * (lim[0] + lim[1]);
    }
    if (max_z < min_z) {
        min_z = max_z = 0.5f * (lim[2] + lim[3]);
    }
    *x = clampf(*x, min_x, max_x);
    *z = clampf(*z, min_z, max_z);
}

static bool cafe_near_enemy(const ShCafe *cafe, float px, float pz)
{
    if (cafe->stage != SH_CAFE_BIRD_FIGHT || cafe->bird_state == SH_BIRD_DEAD ||
        cafe->bird_state == SH_BIRD_HIDDEN) {
        return false;
    }
    /* vcSetNearestEnemyDataInVC_WORK: ENEMY_DIST_MAX 15 m on each axis. */
    return fabsf(cafe->bird_pos[0] - px) < 15.0f && fabsf(cafe->bird_pos[2] - pz) < 15.0f;
}

/*
 * vcMakeIdealCamPosForFixAngCam / vcMakeIdealCamPosUseVC_ROAD_DATA + vcRetMaxTgtMvXzLen,
 * evaluated in PS1 XZ (x, z = -3DS z). Gameplay cameras for explore / fight / done.
 */
static void update_gameplay_camera(ShCafe *cafe, float px, float py, float pz, float yaw, float dt)
{
    if (!cafe || cafe->stage == SH_CAFE_CUTSCENE || cafe->stage == SH_CAFE_BIRD_INTRO ||
        cafe->stage == SH_CAFE_LEAVING) {
        cafe->cam_snap = true;
        return;
    }

    /* While CAFE2 perch/peek uses DMS, do not override. */
    if (cafe->cam_active && cafe->dms_file == 2 && cafe->stage == SH_CAFE_EXPLORE &&
        (cafe->bird_state == SH_BIRD_PERCH || cafe->bird_state == SH_BIRD_SCRIPTED)) {
        cafe->cam_snap = true;
        return;
    }

    const bool snap = cafe->cam_snap || !cafe->cam_active;
    bool warp = false;
    const int ri = cafe_road_select(cafe, px, -pz, cafe_near_enemy(cafe, px, pz), &warp);
    if (ri < 0) {
        return;
    }
    const CafeRoad *road = &s_cafe_roads[ri];

    const float cx = px, cz = -pz;       /* PS1 XZ */
    const float face = -yaw;             /* PS1 facing angle */
    float cam_x = cafe->cam.pos[0], cam_z = -cafe->cam.pos[2];
    float ix, iz, iy;

    if (road->mv == CAFE_CAM_FIX) {
        float vx = 0.0f, vz = 0.0f;
        if (cx < road->rd[0]) {
            vx = road->rd[0] - cx;
        } else if (cx > road->rd[1]) {
            vx = road->rd[1] - cx;
        }
        if (cz < road->rd[2]) {
            vz = road->rd[2] - cz;
        } else if (cz > road->rd[3]) {
            vz = road->rd[3] - cz;
        }
        const float maxd = fmaxf(fabsf(vx), fabsf(vz));
        const float off = maxd * 0.5f + 1.5f;
        const float d = snap ? 7.0f : sqrtf((cx - cam_x) * (cx - cam_x) + (cz - cam_z) * (cz - cam_z));
        float fwd = 0.0f;
        if (d < 7.0f) {
            fwd = d > 1.5f ? (7.0f - d) / 5.5f * 0.7f : 0.7f;
        }
        const float fy = DEG(road->fix_yaw_deg) + PI_F;
        ix = cx + fwd * sinf(face) + off * sinf(fy);
        iz = cz + fwd * cosf(face) + off * cosf(fy);
        road_clamp_in(road->rd, &ix, &iz);
        iy = road->cam_y_lo;
    } else {
        const float base = face + PI_F;
        float delta = wrap_angle(cafe->cam_ideal_ang - base);
        delta = delta >= 0.0f ? DEG(12.0f) : -DEG(12.0f);
        cafe->cam_ideal_ang = wrap_angle(base + delta);

        float tx = cx, tz = cz;
        road_clamp_in(road->rd, &tx, &tz);
        const float h = sqrtf((tx - cx) * (tx - cx) + (tz - cz) * (tz - cz));
        float dist = 2.0f; /* VC_AREA_SMALL */
        if (h > 3.0f) {
            dist = 0.4f;
        } else if (h > 0.7f) {
            dist = 2.0f + (0.4f - 2.0f) * (h - 0.7f) / 2.3f;
        }
        ix = cx + dist * sinf(cafe->cam_ideal_ang);
        iz = cz + dist * cosf(cafe->cam_ideal_ang);
        road_clamp_in(road->rd, &ix, &iz);

        /* Rail passes over Harry: slide along it so the lens never sits inside him. */
        const float hx = ix - cx, hz = iz - cz;
        const float hd = sqrtf(hx * hx + hz * hz);
        if (hd < 1.3f) {
            const float rx = road->rd[1] - road->rd[0];
            const float rz = road->rd[3] - road->rd[2];
            const float rl = sqrtf(rx * rx + rz * rz);
            if (rl > 0.05f) {
                const float ux = rx / rl, uz = rz / rl;
                float side = (cam_x - cx) * ux + (cam_z - cz) * uz;
                if (fabsf(side) < 0.05f) {
                    side = (hx * ux + hz * uz) >= 0.0f ? 1.0f : -1.0f;
                }
                const float push = sqrtf(fmaxf(1.3f * 1.3f - hd * hd, 0.0f)) * (side >= 0.0f ? 1.0f : -1.0f);
                ix += ux * push;
                iz += uz * push;
                ix = clampf(ix, road->rd[0], road->rd[1]);
                iz = clampf(iz, road->rd[2], road->rd[3]);
            }
        }
        iy = clampf(1.35f, road->cam_y_lo, road->cam_y_hi);
    }

    if (snap || warp) {
        cam_x = ix;
        cam_z = iz;
        cafe->cam.pos[1] = py + iy;
    } else {
        const float mvx = cx - (cafe->cam_prev_px);
        const float mvz = cz - (-cafe->cam_prev_pz);
        const float spd = dt > 0.0f ? sqrtf(mvx * mvx + mvz * mvz) / dt : 0.0f;
        float vmax = spd + 1.0f;
        if (vmax < 2.2f) {
            vmax = 2.2f;
        }
        const float step = vmax * dt;
        const float ex = ix - cam_x, ez = iz - cam_z;
        const float el = sqrtf(ex * ex + ez * ez);
        if (el <= step || el < 1e-4f) {
            cam_x = ix;
            cam_z = iz;
        } else {
            cam_x += ex / el * step;
            cam_z += ez / el * step;
        }
        const float ty = py + iy;
        const float dy = ty - cafe->cam.pos[1];
        const float ystep = 2.2f * dt;
        cafe->cam.pos[1] += fabsf(dy) <= ystep ? dy : (dy > 0.0f ? ystep : -ystep);
    }
    cafe->cam.pos[0] = cam_x;
    cafe->cam.pos[2] = -cam_z;
    cafe->cam_prev_px = px;
    cafe->cam_prev_pz = pz;

    if (road->mv == CAFE_CAM_FIX) {
        cafe->cam.yaw = ps1_yaw_to_3ds(DEG(road->fix_yaw_deg));
        cafe->cam.pitch = DEG(road->fix_pitch_deg - 360.0f);
    } else {
        const float wy = py + 1.15f;
        if (snap || warp) {
            cafe->cam_watch[0] = px;
            cafe->cam_watch[1] = wy;
            cafe->cam_watch[2] = pz;
        } else {
            const float k = 1.0f - expf(-10.0f * dt);
            cafe->cam_watch[0] += (px - cafe->cam_watch[0]) * k;
            cafe->cam_watch[1] += (wy - cafe->cam_watch[1]) * k;
            cafe->cam_watch[2] += (pz - cafe->cam_watch[2]) * k;
        }
        cam_look_at(&cafe->cam, cafe->cam_watch[0], cafe->cam_watch[1], cafe->cam_watch[2]);
        cafe->cam.pitch = clampf(cafe->cam.pitch, DEG(-55.0f), DEG(18.0f));
    }
    /* Mild aim zoom: PS1 mostly tightens camera, not a telephoto FOV. 40° ≈ slight pull-in. */
    cafe->cam.fov_deg = cafe->aiming ? 40.0f : 48.0f;

    cafe->walk_yaw = cafe->cam.yaw;
    cafe->move_yaw = cafe->cam.yaw;
    cafe->cam_active = true;
    cafe->cam_snap = false;
}

static bool load_file(const char *path, uint8_t *buf, size_t cap, size_t *size_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    const size_t n = fread(buf, 1, cap, f);
    fclose(f);
    if (size_out) {
        *size_out = n;
    }
    return n > 0;
}

static bool dms_load(ShCafe *cafe, int which)
{
    if (cafe->dms_file == which && cafe->dms_size > 0) {
        return true;
    }
    cafe->dms_file = 0;
    cafe->dms_size = 0;
    if (!load_file(which == 1 ? SH_CAFE_DMS_PATH : SH_CAFE2_DMS_PATH, cafe->dms_data, sizeof(cafe->dms_data),
                   &cafe->dms_size)) {
        return false;
    }
    if (!sh_dms_parse(&cafe->dms, cafe->dms_data, cafe->dms_size)) {
        cafe->dms_size = 0;
        return false;
    }
    cafe->dms_file = which;
    return true;
}

static bool dms_char(ShCafe *cafe, const char *name, float frame, float out_pos[3], float *out_yaw)
{
    if (cafe->dms_size == 0) {
        return false;
    }
    const int idx = sh_dms_find_char(&cafe->dms, name);
    float pos[3], rot[3];
    if (idx < 0 || !sh_dms_char_sample(&cafe->dms, idx, frame, pos, rot)) {
        return false;
    }
    ps1_to_3ds(pos, out_pos);
    if (out_yaw) {
        *out_yaw = ps1_yaw_to_3ds(rot[1]);
    }
    return true;
}

/* vcUserCamTarget / vcUserWatchTarget / vcChangeProjectionValue */
static void dms_camera(ShCafe *cafe, float frame)
{
    if (cafe->dms_size == 0) {
        return;
    }
    float pos[3], look[3], pd = 0.0f;
    if (!sh_dms_camera_sample(&cafe->dms, frame, pos, look, &pd)) {
        return;
    }
    float p[3], l[3];
    ps1_to_3ds(pos, p);
    ps1_to_3ds(look, l);
    cafe->cam.pos[0] = p[0];
    cafe->cam.pos[1] = p[1];
    cafe->cam.pos[2] = p[2];
    const float dx = l[0] - p[0];
    const float dy = l[1] - p[1];
    const float dz = l[2] - p[2];
    const float horiz = sqrtf(dx * dx + dz * dz);
    cafe->cam.yaw = atan2f(-dx, -dz);
    cafe->cam.pitch = horiz > 1e-4f ? atan2f(dy, horiz) : 0.0f;
    /* PS1 projection distance over a 240-line screen → vertical FOV. */
    cafe->cam.fov_deg = pd > 20.0f ? 2.0f * atan2f(120.0f, pd) * (180.0f / PI_F) : 48.0f;
    cafe->cam_active = true;
    cafe->walk_yaw = cafe->cam.yaw;
    cafe->cam_road_idx = -1; /* DMS owns the view until gameplay picks a road again. */
}

/* Event_CutsceneTimerAdvance */
static bool timer_advance(ShCafe *cafe, float rate, float start, float end, bool clamp, float dt)
{
    if (cafe->timer < start) {
        cafe->timer = start;
        return false;
    }
    cafe->timer += dt * rate;
    if (cafe->timer > end) {
        if (clamp) {
            cafe->timer = end;
        }
        return true;
    }
    return false;
}

/* Event_WaitTimer: true when elapsed. */
static bool wait_timer(ShCafe *cafe, float secs, float dt)
{
    cafe->wait += dt;
    if (cafe->wait >= secs) {
        cafe->wait = 0.0f;
        return true;
    }
    return false;
}

static void fade_to(ShCafe *cafe, float target, float secs)
{
    cafe->fade_target = target;
    cafe->fade_speed = secs > 0.01f ? 1.0f / secs : 100.0f;
}

static bool fade_done(const ShCafe *cafe)
{
    return fabsf(cafe->fade - cafe->fade_target) < 0.01f;
}

static void fade_update(ShCafe *cafe, float dt)
{
    const float d = cafe->fade_target - cafe->fade;
    const float step = cafe->fade_speed * dt;
    if (fabsf(d) <= step) {
        cafe->fade = cafe->fade_target;
    } else {
        cafe->fade += d > 0.0f ? step : -step;
    }
}

/* g_Cutscene_MapMsgAudioCmds (pages 15..58 except 43), D_800DE124 (62, 63), D_800DE128 (68). */
static uint16_t msg_audio_cmd(int idx)
{
    static const uint16_t k_cutscene[43] = {
        4127, 4130, 4097, 4136, 4099, 4112, 4105, 4125, 4123, 4144, 4121, 4104, 4143, 4147, 4117,
        4129, 4120, 4103, 4135, 4134, 4142, 4148, 4140, 4141, 4111, 4106, 4118, 4146, 4114, 4139,
        4122, 4102, 4116, 4137, 4128, 4107, 4133, 4110, 4115, 4113, 4145, 4124, 4098,
    };
    if (idx >= 15 && idx <= 58 && idx != 43) {
        return k_cutscene[idx - 15 - (idx > 43)];
    }
    switch (idx) {
    case 62:
        return 4138;
    case 63:
        return 4100;
    case 68:
        return 4101;
    default:
        return 0;
    }
}

/* Gfx_MapMsg_Draw returning MapMsgState_Finish → SD_Call(audioCmds[(*audioIdx)++]). */
static void msg_page_begin(ShCafe *cafe, int idx)
{
    const uint16_t cmd = msg_audio_cmd(idx);
    cafe->msg_idx = idx;
    cafe->msg_time = 0.0f;
    cafe->msg_voice = cmd != 0;
    cafe->msg_load = cmd != 0 ? 1 : 0;
    cafe->msg_load_time = 0.0f;
    if (cmd != 0) {
        sh_snd_call(cmd);
    }
}

/* The page stays hidden until the XA request went through AudioStreamingState_XaLoadPending. */
static bool msg_voice_ready(ShCafe *cafe, float dt)
{
    if (cafe->msg_load == 0) {
        return true;
    }
    cafe->msg_load_time += dt;
    if (sh_sound_streaming() == 4) {
        cafe->msg_load = 2;
    } else if (cafe->msg_load == 2) {
        cafe->msg_load = 0;
    }
    if (cafe->msg_load_time > 3.0f) {
        cafe->msg_load = 0; /* clip missing: do not hold the cutscene forever */
    }
    return cafe->msg_load == 0;
}

/* Event_DisplayMapMsgWithAudio: plays a chain starting at `idx` for this step; true once finished. */
static bool msg_run(ShCafe *cafe, int idx, const ShInputState *input, float dt)
{
    if (cafe->msg_step != cafe->step || cafe->msg_idx < 0) {
        cafe->msg_step = cafe->step;
        msg_page_begin(cafe, idx);
    }
    const CafeMsg *m = msg_get(cafe->msg_idx);
    if (!m) {
        cafe->msg_idx = -1;
        return true;
    }
    if (cafe->msg_voice) {
        sh_bgm_status_set(SH_BGM_STATUS_VOICE_DIALOG);
        if (!msg_voice_ready(cafe, dt)) {
            return false;
        }
    }
    cafe->msg_time += dt;
    /* ~J0(secs) pages are MapMsgAudioType_VoiceClipUnskippable. */
    const bool skip = m->secs <= 0.0f && input && (sh_input_down(input, KEY_A) || sh_input_down(input, KEY_B));
    const float dur = m->secs > 0.0f ? m->secs : 1e9f;
    if (cafe->msg_time >= dur || skip) {
        if (cafe->msg_voice) {
            sh_snd_call(19);
        }
        if (m->end) {
            cafe->msg_idx = -1;
            return true;
        }
        msg_page_begin(cafe, cafe->msg_idx + 1);
    }
    return false;
}

static void msg_clear(ShCafe *cafe)
{
    cafe->msg_idx = -1;
    cafe->msg_step = -1;
    cafe->msg_time = 0.0f;
    cafe->msg_voice = false;
    cafe->msg_load = 0;
}

static void step_set(ShCafe *cafe, int step)
{
    cafe->step = step;
    cafe->wait = 0.0f;
}

static void step_next(ShCafe *cafe)
{
    step_set(cafe, cafe->step + 1);
}

/* ---- lifecycle ----------------------------------------------------------- */

void sh_cafe_reset(ShCafe *cafe)
{
    if (!cafe) {
        return;
    }
    memset(cafe, 0, sizeof(*cafe));
    cafe->radio_vol = 224;
    cafe->cam_road_idx = -1;
    cafe->timer = -1.0f;
    cafe->radio = -1.0f;
    cafe->msg_idx = -1;
    cafe->msg_step = -1;
    cafe->bird_pos[1] = -10.0f;
    cafe->bird_pos[2] = -280.0f;
}

static void enter_explore(ShCafe *cafe);

void sh_cafe_begin(ShCafe *cafe, ShSavegame *save)
{
    if (!cafe) {
        return;
    }
    sh_cafe_reset(cafe);
    cafe->stage = SH_CAFE_CUTSCENE;
    cafe->step = 0;
    cafe->controls_locked = true;
    cafe->fade = 1.0f;
    cafe->fade_target = 1.0f;
    if (save) {
        sh_save_event_set(save, SH_EV_FMV);
        sh_save_event_set(save, SH_EV_CAFE_CS);
        save->map_idx = 1;     /* MapIdx_MAP0_S01 */
        save->location_id = 1; /* SaveLocationId_Cafe */
    }
    dms_load(cafe, 1);
}

void sh_cafe_begin_explore(ShCafe *cafe, ShSavegame *save)
{
    if (!cafe) {
        return;
    }
    sh_cafe_reset(cafe);
    if (save) {
        sh_save_event_set(save, SH_EV_FMV);
        sh_save_event_set(save, SH_EV_CAFE_CS);
        save->map_idx = 1;
        save->location_id = 1;
    }
    enter_explore(cafe);
    cafe->fade = 0.0f;
    cafe->fade_target = 0.0f;
    cafe->got_text = "Skip: Cafe explore\nPick up knife, map, flashlight...";
    cafe->got_time = 3.0f;
}

void sh_cafe_begin_bird_fight(ShCafe *cafe, ShSavegame *save)
{
    if (!cafe) {
        return;
    }
    sh_cafe_reset(cafe);
    if (save) {
        sh_save_event_set(save, SH_EV_FMV);
        sh_save_event_set(save, SH_EV_CAFE_CS);
        sh_save_event_set(save, SH_EV_KNIFE);
        sh_save_event_set(save, SH_EV_FLASHLIGHT);
        sh_save_event_set(save, SH_EV_MAP);
        sh_save_event_set(save, SH_EV_KNIFE_AND_MAP);
        sh_save_event_set(save, SH_EV_BIRD_INTRO);
        sh_save_event_set(save, SH_EV_47);
        save->has_map = true;
        save->map_idx = 1;
        save->location_id = 1;
        sh_save_item_add(save, SH_ITEM_POCKET_KNIFE, 1);
        sh_save_item_add(save, SH_ITEM_FLASHLIGHT, 1);
        sh_save_item_add(save, SH_ITEM_HANDGUN, 1);
        sh_save_item_add(save, SH_ITEM_HANDGUN_BULLETS, 15);
        sh_save_item_add(save, SH_ITEM_HEALTH_DRINK, 2);
        save->equipped_weapon = SH_ITEM_HANDGUN;
        sh_save_flashlight_set(save, true);
    }
    cafe->stage = SH_CAFE_BIRD_FIGHT;
    cafe->step = 0;
    cafe->controls_locked = false;
    cafe->fade = 0.0f;
    cafe->fade_target = 0.0f;
    cafe->bird_state = SH_BIRD_RETREAT;
    cafe->bird_timer = 0.5f;
    cafe->bird_cd = 1.0f;
    cafe->bird_hp = SH_BIRD_BASE_HEALTH;
    cafe->bird_anim = 0.0f;
    cafe->bird_pos[0] = 4.0f;
    cafe->bird_pos[1] = 2.5f;
    cafe->bird_pos[2] = -275.0f;
    cafe->bird_yaw = 0.0f;
    cafe->ammo = save ? sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS) : 15;
    cafe->radio = 0.95f;
    cafe->got_text = "Skip: Air Screamer fight\nR : aim    A : shoot";
    cafe->got_time = 3.0f;
}

static void enter_explore(ShCafe *cafe)
{
    cafe->stage = SH_CAFE_EXPLORE;
    cafe->step = 0;
    cafe->controls_locked = false;
    cafe->cam_active = false;
    cafe->prompt_kind = 0;
    cafe->prompt_open = false;
    cafe->timer = -1.0f;
    cafe->harry_anim_status = 0;
    cafe->harry_anim_time = 0.0f;
    msg_clear(cafe);
}

/* ---- MapEvent_CafeCutscene ---------------------------------------------- */

/* PlayerState → packed ANIM_STATUS for HB_M0S01 (via field_38 / base anims). */
static int cafe_player_state_to_status(int state)
{
    switch (state) {
    case 51: /* PlayerState_Unk51 → anim 56 (cutscene stand loop) */
        return (56 << 1) | 1;
    case 53: /* PlayerState_WalkForward → HarryAnim_WalkForward */
        return (2 << 1) | 1;
    case 71: /* PlayerState_Unk71 → anim 46 */
        return (46 << 1) | 1;
    case 76: /* 0x4C — ANIM_STATUS(38, false) wake blend */
    case 77: /* PlayerState_Unk77 → anim 38 */
        return (38 << 1) | 1;
    case 78: /* PlayerState_Unk78 → anim 39 */
        return (39 << 1) | 1;
    case 90: /* PlayerState_Unk90 → anim 57 */
        return (57 << 1) | 1;
    case 93: /* PlayerState_Unk93 → anim 46 */
        return (46 << 1) | 1;
    case 110: /* PlayerState_Unk110 → anim 55 */
        return (55 << 1) | 1;
    default:
        return state;
    }
}

static void harry_set_anim(ShCafe *cafe, int state)
{
    const int status = cafe_player_state_to_status(state);
    if (cafe->harry_anim_status != status) {
        cafe->harry_anim_status = status;
        cafe->harry_anim_time = 0.0f;
    }
}

static void cutscene_update(ShCafe *cafe, ShSavegame *save, float *px, float *pz, float *yaw,
                            const ShInputState *input, float dt)
{
    cafe->controls_locked = true;
    cafe->radio = 0.0f;

    /* Skip (START) between steps 6 and 46, like the original. */
    if (sh_input_down(input, KEY_START) && cafe->step > 5 && cafe->step < 47) {
        msg_clear(cafe);
        step_set(cafe, 52);
    }

    switch (cafe->step) {
    case 0:
        cafe->timer = 0.0f;
        cafe->sub0 = 0;
        cafe->sub1 = 0;
        cafe->cybil_visible = false;
        cafe->cybil_pos[0] = 4.4f;
        cafe->cybil_pos[1] = 0.0f;
        cafe->cybil_pos[2] = -269.9f;
        cafe->cybil_yaw = 0.0f;
        cafe->cybil_clip = 1; /* CybilAnim_1 idle stand */
        cafe->cybil_anim = 0.0f;
        harry_set_anim(cafe, 77); /* wake / sit-up (HB_M0S01) */
        cafe->fade = 1.0f;
        fade_to(cafe, 1.0f, 0.1f);
        step_next(cafe);
        /* fallthrough */
    case 1:
        /* Timeout: alley XA / streaming must not leave us on a permanent black screen. */
        if (sh_bgm_crossfade(9) || wait_timer(cafe, 2.0f, dt)) {
            sh_bgm_crossfade_reset();
            step_next(cafe);
        }
        break;
    case 2:
        harry_set_anim(cafe, 77);
        fade_to(cafe, 0.0f, 1.2f);
        step_next(cafe);
        /* fallthrough */
    case 3:
        step_next(cafe);
        /* fallthrough */
    case 4:
        if (fade_done(cafe)) {
            step_next(cafe);
        }
        break;
    case 5:
        step_next(cafe);
        /* fallthrough */
    case 6:
        if (timer_advance(cafe, 7.8f, 0.0f, 5.0f, false, dt)) {
            step_next(cafe);
        }
        break;
    case 7:
        timer_advance(cafe, 7.8f, 0.0f, 26.0f, true, dt);
        if (msg_run(cafe, 15, input, dt)) {
            step_next(cafe);
        }
        break;
    case 8:
        if (timer_advance(cafe, 7.8f, 0.0f, 26.0f, true, dt)) {
            step_next(cafe);
        }
        break;
    case 9:
        cafe->sub0 = 1; /* Cybil appears */
        cafe->cybil_visible = true;
        harry_set_anim(cafe, 78);
        step_next(cafe);
        /* fallthrough */
    case 10:
    case 11:
    case 12:
    case 13:
        switch (cafe->step) {
        case 10:
            if (msg_run(cafe, 16, input, dt)) {
                step_next(cafe);
            }
            break;
        case 11:
            if (msg_run(cafe, 17, input, dt)) {
                step_next(cafe);
            }
            break;
        case 12:
            if (cafe->timer >= 48.0f && cafe->sub1 == 2) {
                step_set(cafe, 13);
            }
            break;
        case 13:
            if (msg_run(cafe, 18, input, dt)) {
                step_next(cafe);
            }
            break;
        default:
            break;
        }
        timer_advance(cafe, 4.0f, 27.0f, 48.0f, true, dt);
        if (cafe->sub1 == 0 && cafe->timer > 38.0f) {
            cafe->sub1 = 1;
            cafe->sub_time = 0.0f;
            cafe->cybil_clip = 10; /* walk-up / greet */
            cafe->cybil_anim = 0.0f;
            harry_set_anim(cafe, 110);
        }
        if (cafe->sub1 == 1) {
            cafe->sub_time += dt;
            if (cafe->sub_time > 1.0f) {
                cafe->sub1 = 2;
                cafe->cybil_clip = 11;
                cafe->cybil_anim = 0.0f;
            }
        }
        break;
    case 14:
        timer_advance(cafe, 3.0f, 49.0f, 139.0f, true, dt);
        if (msg_run(cafe, 25, input, dt)) {
            step_next(cafe);
        }
        break;
    case 15:
        cafe->timer = 140.0f;
        step_next(cafe);
        /* fallthrough */
    case 16:
        if (msg_run(cafe, 35, input, dt)) {
            step_next(cafe);
        }
        break;
    case 17:
        if (wait_timer(cafe, 1.2f, dt)) {
            step_next(cafe);
        }
        break;
    case 18:
        harry_set_anim(cafe, 93);
        step_next(cafe);
        /* fallthrough */
    case 19:
        if (timer_advance(cafe, 10.0f, 141.0f, 152.0f, false, dt)) {
            step_next(cafe);
        }
        break;
    case 20:
        harry_set_anim(cafe, 53); /* WalkForward (listen / shift) */
        step_next(cafe);
        /* fallthrough */
    case 21:
        if (timer_advance(cafe, 10.0f, 141.0f, 159.0f, true, dt)) {
            step_next(cafe);
        }
        sh_xa_preload(4108); /* "Hold it!" */
        break;
    case 22:
        sh_snd_call(4108);
        cafe->cybil_clip = 7;
        cafe->cybil_anim = 0.0f;
        step_next(cafe);
        /* fallthrough */
    case 23:
        timer_advance(cafe, 10.0f, 160.0f, 173.0f, true, dt);
        if (msg_run(cafe, 43, input, dt)) {
            step_next(cafe);
        }
        break;
    case 24:
        harry_set_anim(cafe, 71);
        step_next(cafe);
        /* fallthrough */
    case 25:
        timer_advance(cafe, 10.0f, 174.0f, 184.0f, true, dt);
        if (msg_run(cafe, 44, input, dt)) {
            step_next(cafe);
        }
        break;
    case 26:
        harry_set_anim(cafe, 51);
        cafe->cybil_clip = 8;
        cafe->cybil_anim = 0.0f;
        step_next(cafe);
        /* fallthrough */
    case 27:
        timer_advance(cafe, 10.0f, 185.0f, 194.0f, true, dt);
        if (msg_run(cafe, 45, input, dt)) {
            step_next(cafe);
        }
        break;
    case 28:
        cafe->timer = 195.0f;
        cafe->cybil_clip = 5;
        cafe->cybil_anim = 0.0f;
        step_next(cafe);
        /* fallthrough */
    case 29:
        if (msg_run(cafe, 46, input, dt)) {
            step_next(cafe);
        }
        break;
    case 30:
        if (wait_timer(cafe, 1.5f, dt)) {
            step_next(cafe);
        }
        break;
    case 31:
        if (msg_run(cafe, 49, input, dt)) {
            step_next(cafe);
        }
        break;
    case 32:
        cafe->cybil_clip = 1;
        cafe->cybil_anim = 0.0f;
        step_next(cafe);
        /* fallthrough */
    case 33:
        if (timer_advance(cafe, 20.0f, 207.0f, 222.0f, true, dt)) {
            step_next(cafe);
        }
        break;
    case 34:
        cafe->cybil_clip = 6;
        cafe->cybil_anim = 0.0f;
        step_next(cafe);
        /* fallthrough */
    case 35:
        timer_advance(cafe, 5.0f, 223.0f, 251.0f, true, dt);
        if (msg_run(cafe, 51, input, dt)) {
            step_next(cafe);
        }
        break;
    case 36:
        timer_advance(cafe, 5.0f, 223.0f, 251.0f, true, dt);
        if (wait_timer(cafe, 0.6f, dt)) {
            step_next(cafe);
        }
        break;
    case 37:
        timer_advance(cafe, 5.0f, 223.0f, 251.0f, true, dt);
        if (msg_run(cafe, 55, input, dt)) {
            step_next(cafe);
        }
        break;
    case 38:
        cafe->cybil_clip = 9;
        cafe->cybil_anim = 0.0f;
        harry_set_anim(cafe, 0x4C);
        step_next(cafe);
        /* fallthrough */
    case 39:
        timer_advance(cafe, 10.0f, 252.0f, 280.0f, true, dt);
        if (msg_run(cafe, 57, input, dt)) {
            step_next(cafe);
        }
        break;
    case 40:
        if (timer_advance(cafe, 10.0f, 252.0f, 280.0f, true, dt)) {
            step_next(cafe);
        }
        break;
    case 41:
        cafe->cybil_clip = 5;
        cafe->cybil_anim = 0.0f;
        step_next(cafe);
        /* fallthrough */
    case 42:
        if (timer_advance(cafe, 10.0f, 281.0f, 296.0f, true, dt)) {
            step_next(cafe);
        }
        break;
    case 43:
        cafe->timer = 297.0f;
        harry_set_anim(cafe, 51);
        step_next(cafe);
        /* fallthrough */
    case 44:
        if (msg_run(cafe, 58, input, dt)) {
            step_next(cafe);
        }
        break;
    case 45:
        harry_set_anim(cafe, 90);
        cafe->cybil_clip = 1;
        cafe->cybil_anim = 0.0f;
        step_next(cafe);
        /* fallthrough */
    case 46:
        if (timer_advance(cafe, 10.0f, 298.0f, 308.0f, true, dt)) {
            step_next(cafe);
        }
        break;
    case 47:
        cafe->sub0 = 0; /* Cybil leaves */
        cafe->cybil_visible = false;
        snd_at_ps1(SH_SFX_CAFE_DOOR, 0.8f, -1.2f, 269.5f, 0.5f);
        step_next(cafe);
        /* fallthrough */
    case 48:
        sh_snd_call(19);
        /* Cybil hands Harry the handgun (+15 rounds). */
        sh_save_item_add(save, SH_ITEM_HANDGUN, 1);
        sh_save_item_add(save, SH_ITEM_HANDGUN_BULLETS, SH_HANDGUN_AMMO);
        cafe->ammo = sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS);
        save->equipped_weapon = SH_ITEM_HANDGUN;
        step_next(cafe);
        /* fallthrough */
    case 49:
        fade_to(cafe, 1.0f, 0.8f);
        if (fade_done(cafe)) {
            step_next(cafe);
        }
        break;
    case 50:
        cafe->timer = 308.0f;
        if (wait_timer(cafe, 0.5f, dt)) {
            step_next(cafe);
        }
        break;
    case 52: /* skip */
        fade_to(cafe, 1.0f, 0.6f);
        if (fade_done(cafe)) {
            step_set(cafe, 53);
        }
        break;
    case 53:
        step_set(cafe, 48);
        break;
    default: {
        /* Player_ControlUnfreeze + SysState_Gameplay */
        float pos[3], y = 0.0f;
        if (dms_char(cafe, "HERO", 308.0f, pos, &y)) {
            *px = pos[0];
            *pz = pos[2];
            *yaw = y;
        }
        sh_snd_call(23);
        sh_save_event_set(save, SH_EV_CAFE_DONE);
        cafe->timer = -1.0f;
        enter_explore(cafe);
        fade_to(cafe, 0.0f, 0.8f);
        return;
    }
    }

    if (cafe->timer >= 0.0f) {
        dms_camera(cafe, cafe->timer);
        float pos[3], y = 0.0f;
        if (dms_char(cafe, "HERO", cafe->timer, pos, &y)) {
            *px = pos[0];
            *pz = pos[2];
            *yaw = y;
        }
        if (cafe->sub0 != 0 && dms_char(cafe, "SIBY", cafe->timer, pos, &y)) {
            cafe->cybil_visible = true;
            cafe->cybil_pos[0] = pos[0];
            cafe->cybil_pos[1] = pos[1];
            cafe->cybil_pos[2] = pos[2];
            cafe->cybil_yaw = y;
        } else {
            cafe->cybil_visible = false;
        }
    }
}

/* ---- MapEvent_MapItemTake tail: bird perched outside (CAFE2 frame 0) ------ */

static void map_peek_begin(ShCafe *cafe, float *px, float *pz, float *yaw)
{
    /* Original warps Harry to face the window. */
    *px = 4.586f;
    *pz = -267.2852f;
    *yaw = ps1_yaw_to_3ds(DEG(90.0f));
    cafe->player_warp[0] = *px;
    cafe->player_warp[1] = *pz;
    cafe->perch_time = 0.0f;
    cafe->timer = 0.0f;
    if (dms_load(cafe, 2)) {
        float pos[3], y = 0.0f;
        if (dms_char(cafe, "BIRD", 0.0f, pos, &y)) {
            cafe->bird_pos[0] = pos[0];
            cafe->bird_pos[1] = pos[1];
            cafe->bird_pos[2] = pos[2];
            cafe->bird_yaw = y;
            cafe->bird_state = SH_BIRD_PERCH;
        }
        dms_camera(cafe, 0.0f);
    }
}

/* Map_WorldObjectsUpdate: flags 41/42 */
static void map_peek_update(ShCafe *cafe, ShSavegame *save, float px, float pz, float dt)
{
    if (!sh_save_event_get(save, SH_EV_MAP) || sh_save_event_get(save, SH_EV_BIRD_PERCH_DONE)) {
        return;
    }
    const bool moved = fabsf(px - cafe->player_warp[0]) > 0.02f || fabsf(pz - cafe->player_warp[1]) > 0.02f;
    if (sh_save_event_get(save, SH_EV_BIRD_PERCH_FLY)) {
        cafe->timer += dt * 12.0f;
        if (cafe->timer > 25.0f) {
            cafe->timer = 25.0f;
        }
        float pos[3], y = 0.0f;
        if (dms_char(cafe, "BIRD", cafe->timer, pos, &y)) {
            cafe->bird_pos[0] = pos[0];
            cafe->bird_pos[1] = pos[1];
            cafe->bird_pos[2] = pos[2];
            cafe->bird_yaw = y;
        }
        const bool far = fabsf(px - cafe->player_warp[0]) > 0.7f || fabsf(pz - cafe->player_warp[1]) > 0.7f;
        if (cafe->timer >= 25.0f || far) {
            sh_save_event_set(save, SH_EV_BIRD_PERCH_DONE);
            cafe->cam_active = false;
            cafe->map_open = false;
            cafe->bird_state = SH_BIRD_HIDDEN;
            cafe->bird_pos[1] = -10.0f;
            cafe->timer = -1.0f;
        }
    } else if (moved || cafe->perch_time > 7.5f) {
        sh_save_event_set(save, SH_EV_BIRD_PERCH_FLY);
        cafe->bird_state = SH_BIRD_SCRIPTED;
        cafe->map_open = false; /* close paper map so Harry can watch the bird */
    } else {
        cafe->perch_time += dt;
    }
}

/* ---- MapEvent_AirScreamerIntroCutscene ---------------------------------- */

static void bird_intro_begin(ShCafe *cafe, ShSavegame *save, float *px, float *pz, float *yaw)
{
    cafe->stage = SH_CAFE_BIRD_INTRO;
    cafe->step = 0;
    cafe->controls_locked = true;
    cafe->prompt_kind = 0;
    cafe->prompt_open = false;
    msg_clear(cafe);
    sh_save_event_set(save, SH_EV_RADIO_ON_50);
    sh_save_event_set(save, SH_EV_RADIO_STATIC);
    *px = 1.3f;
    *pz = -269.7f;
    *yaw = ps1_yaw_to_3ds(DEG(5.0f));
    cafe->timer = 26.0f;
    cafe->fade = 1.0f;
    fade_to(cafe, 0.0f, 0.5f);
    cafe->radio = 0.5f;
    cafe->bird_state = SH_BIRD_HIDDEN;
    cafe->bird_hp = SH_BIRD_BASE_HEALTH;
    dms_load(cafe, 2);
}

static bool timer_to(ShCafe *cafe, float rate, float end, float dt)
{
    cafe->timer += dt * rate;
    if (cafe->timer >= end) {
        cafe->timer = end;
        return true;
    }
    return false;
}

static void bird_intro_update(ShCafe *cafe, ShSavegame *save, float *px, float *pz, float *yaw,
                              const ShInputState *input, float dt)
{
    cafe->controls_locked = true;

    switch (cafe->step) {
    case 0:
        sh_snd_call(SH_SFX_RADIO_INTERFERENCE);
        sh_snd_call(SH_SFX_RADIO_STATIC);
        step_next(cafe);
        /* fallthrough */
    case 1:
        if (sh_bgm_crossfade(30)) {
            step_next(cafe);
        }
        break;
    case 2:
        step_next(cafe);
        /* fallthrough */
    case 3:
        step_next(cafe);
        /* fallthrough */
    case 4:
        if (timer_to(cafe, 15.0f, 37.0f, dt)) {
            step_next(cafe);
        }
        break;
    case 5:
        if (msg_run(cafe, 62, input, dt)) {
            step_next(cafe);
        }
        break;
    case 6:
        if (wait_timer(cafe, 0.5f, dt)) {
            step_next(cafe);
        }
        break;
    case 7:
        if (msg_run(cafe, 63, input, dt)) {
            step_next(cafe);
        }
        break;
    case 8:
        step_next(cafe);
        /* fallthrough */
    case 9:
        if (timer_to(cafe, 15.0f, 52.0f, dt)) {
            step_next(cafe);
        }
        break;
    case 10:
        if (cafe->timer < 53.0f) {
            cafe->timer = 53.0f;
        }
        cafe->radio = 0.5f;
        cafe->radio_vol = 0x80;
        sh_save_event_set(save, SH_EV_BIRD_INTRO);
        if (wait_timer(cafe, 1.5f, dt)) {
            step_next(cafe);
        }
        break;
    case 11:
        step_next(cafe);
        /* fallthrough */
    case 12:
        if (timer_to(cafe, 15.0f, 75.0f, dt)) {
            step_next(cafe);
        }
        break;
    case 13:
        cafe->radio = 0.88f;
        cafe->radio_vol = 0xE0;
        sh_save_event_set(save, SH_EV_46);
        cafe->timer = 76.0f;
        step_next(cafe);
        /* fallthrough */
    case 14:
        if (timer_to(cafe, 18.0f, 115.0f, dt)) {
            step_next(cafe);
        }
        break;
    case 15:
        snd_at_ps1(SH_SFX_WINDOW_1373, 0.8f, -1.8f, 267.7f, 0.5f);
        step_next(cafe); /* glass shards FX */
        /* fallthrough */
    case 16:
        if (timer_to(cafe, 18.0f, 146.0f, dt)) {
            step_next(cafe);
        }
        break;
    case 17:
        snd_at_ps1(SH_SFX_WINDOW_1374, 0.8f, -1.8f, 264.1f, 0.5f);
        snd_at_ps1(SH_SFX_WINDOW_1375, 0.8f, -1.8f, 264.1f, 0.5f);
        step_next(cafe);
        /* fallthrough */
    case 18:
        if (timer_to(cafe, 18.0f, 147.0f, dt)) {
            step_next(cafe);
        }
        break;
    case 19:
        cafe->bird_state = SH_BIRD_SCRIPTED;
        step_next(cafe);
        /* fallthrough */
    case 20:
        if (timer_to(cafe, 18.0f, 154.0f, dt)) {
            step_next(cafe);
        }
        break;
    case 21:
        sh_save_event_set(save, SH_EV_47);
        cafe->timer = -1.0f;
        cafe->cam_active = false;
        /* fallthrough */
    default:
        /* Player_FallBackward → gameplay; bird becomes an active enemy. */
        cafe->stage = SH_CAFE_BIRD_FIGHT;
        cafe->step = 0;
        cafe->controls_locked = false;
        cafe->bird_state = SH_BIRD_RETREAT;
        cafe->bird_timer = 0.8f;
        cafe->bird_cd = 1.5f;
        cafe->bird_hp = SH_BIRD_BASE_HEALTH;
        cafe->bird_anim = 0.0f;
        cafe->fire_anim = 0.0f;
        cafe->aiming = false;
        cafe->radio = 0.95f;
        /* Cybil already handed over the gun — keep it equipped for the fight. */
        if (sh_save_item_count(save, SH_ITEM_HANDGUN) > 0) {
            save->equipped_weapon = SH_ITEM_HANDGUN;
            cafe->ammo = sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS);
        }
        msg_clear(cafe);
        return;
    }

    cafe->radio = cafe->step >= 13 ? 0.88f : 0.5f;

    if (cafe->timer >= 0.0f) {
        float pos[3], y = 0.0f;
        if (dms_char(cafe, "HERO", cafe->timer, pos, &y)) {
            *px = pos[0];
            *pz = pos[2];
            *yaw = y;
        }
        if (cafe->step >= 20 && dms_char(cafe, "BIRD", cafe->timer, pos, &y)) {
            /* +0.2 m along facing (decomp) — PS1 forward is -Z here. */
            cafe->bird_pos[0] = pos[0] - sinf(y) * 0.2f;
            cafe->bird_pos[1] = pos[1];
            cafe->bird_pos[2] = pos[2] - cosf(y) * 0.2f;
            cafe->bird_yaw = y;
            cafe->bird_state = SH_BIRD_SCRIPTED;
        }
        dms_camera(cafe, cafe->timer);
    }
}

/* ---- Air Screamer AI (air_screamer.c, simplified) ------------------------ */

static void bird_face(ShCafe *cafe, float tx, float tz, float turn_rate, float dt)
{
    const float dx = tx - cafe->bird_pos[0];
    const float dz = tz - cafe->bird_pos[2];
    const float want = atan2f(-dx, -dz);
    const float d = wrap_angle(want - cafe->bird_yaw);
    const float maxd = turn_rate * dt;
    cafe->bird_yaw += d > maxd ? maxd : (d < -maxd ? -maxd : d);
    cafe->bird_yaw = wrap_angle(cafe->bird_yaw);
}

static void bird_move_toward(ShCafe *cafe, float tx, float ty, float tz, float speed, float dt)
{
    const float dx = tx - cafe->bird_pos[0];
    const float dy = ty - cafe->bird_pos[1];
    const float dz = tz - cafe->bird_pos[2];
    const float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < 1e-3f) {
        return;
    }
    const float step = speed * dt;
    const float k = step >= len ? 1.0f : step / len;
    cafe->bird_pos[0] += dx * k;
    cafe->bird_pos[1] += dy * k;
    cafe->bird_pos[2] += dz * k;
}

/* Keep the bird inside the cafe room (walls are not collidable for NPCs here). */
static bool bird_clamp_room(ShCafe *cafe, float floor_y)
{
    static const float lo[3] = {1.3f, 0.0f, -274.6f};
    static const float hi[3] = {7.2f, 0.0f, -265.6f};
    bool clamped = false;
    for (int i = 0; i < 3; i += 2) {
        if (cafe->bird_pos[i] < lo[i]) {
            cafe->bird_pos[i] = lo[i];
            clamped = true;
        }
        if (cafe->bird_pos[i] > hi[i]) {
            cafe->bird_pos[i] = hi[i];
            clamped = true;
        }
    }
    if (cafe->bird_state != SH_BIRD_DYING && cafe->bird_state != SH_BIRD_DEAD) {
        if (cafe->bird_pos[1] < floor_y + 0.5f) {
            cafe->bird_pos[1] = floor_y + 0.5f;
        }
        if (cafe->bird_pos[1] > floor_y + 2.3f) {
            cafe->bird_pos[1] = floor_y + 2.3f;
        }
    }
    return clamped;
}

static void bird_fight_update(ShCafe *cafe, ShSavegame *save, float px, float py, float pz, float dt)
{
    const float dx = px - cafe->bird_pos[0];
    const float dz = pz - cafe->bird_pos[2];
    const float dist = sqrtf(dx * dx + dz * dz);
    cafe->bird_phase += dt;
    cafe->bird_cd -= dt;
    if (cafe->hit_flash > 0.0f) {
        cafe->hit_flash -= dt;
    }

    switch (cafe->bird_state) {
    case SH_BIRD_HOVER: {
        /* Circle Harry at ~2.5 m. */
        const float ang = cafe->bird_phase * 1.1f;
        const float tx = px + sinf(ang) * 2.5f;
        const float tz = pz + cosf(ang) * 2.5f;
        const float ty = py + SH_BIRD_FLY_HEIGHT + 0.25f * sinf(cafe->bird_phase * 5.0f);
        bird_move_toward(cafe, tx, ty, tz, SH_BIRD_SPEED, dt);
        bird_face(cafe, px, pz, 3.5f, dt);
        cafe->bird_timer -= dt;
        if (cafe->bird_timer <= 0.0f) {
            cafe->bird_state = SH_BIRD_APPROACH;
            cafe->bird_timer = 3.0f;
        }
        break;
    }
    case SH_BIRD_APPROACH: {
        const float ty = py + SH_BIRD_FLY_HEIGHT;
        bird_move_toward(cafe, px, ty, pz, SH_BIRD_SPEED, dt);
        bird_face(cafe, px, pz, 4.0f, dt);
        cafe->bird_timer -= dt;
        if (dist < 1.6f) {
            cafe->bird_state = SH_BIRD_DIVE;
            cafe->bird_timer = 0.7f;
            cafe->bird_bit = false;
            sh_snd_at(SH_SFX_BIRD_ATTACK, cafe->bird_pos, 0.5f); /* AirScreamerFlag_4 */
        } else if (cafe->bird_timer <= 0.0f) {
            cafe->bird_state = SH_BIRD_HOVER;
            cafe->bird_timer = 1.5f;
        }
        break;
    }
    case SH_BIRD_DIVE: {
        bird_move_toward(cafe, px, py + 1.0f, pz, SH_BIRD_DIVE_SPEED, dt);
        bird_face(cafe, px, pz, 6.0f, dt);
        if (!cafe->bird_bit && dist < 0.9f && cafe->bird_cd <= 0.0f) {
            save->health = sh_q12_clamp(save->health - SH_Q12(SH_BIRD_BITE_DAMAGE), 0, SH_Q12(100.0f));
            cafe->bird_bit = true;
            cafe->bird_cd = 3.0f;
            const float harry[3] = {px, py, pz};
            sh_snd_at(SH_SFX_BIRD_BITE, cafe->bird_pos, 0.5f); /* AirScreamerFlag_5 */
            sh_snd_at(SH_SFX_HIT_1313, cafe->bird_pos, 0.5f);  /* AirScreamerFlag_0: bite connects */
            sh_snd_at(SH_SFX_HARRY_DAMAGE, harry, 0.125f);
            if (save->health <= 0) {
                cafe->harry_dying = true;
                cafe->die_timer = 0.0f;
                cafe->aiming = false;
                cafe->fire_anim = 0.0f;
                cafe->controls_locked = true;
                fade_to(cafe, 1.0f, 1.8f);
            }
        }
        cafe->bird_timer -= dt;
        if (cafe->bird_timer <= 0.0f || cafe->bird_bit) {
            cafe->bird_state = SH_BIRD_RETREAT;
            cafe->bird_timer = 1.1f;
        }
        break;
    }
    case SH_BIRD_RETREAT: {
        /* Fly away from Harry and up. */
        const float len = dist > 0.05f ? dist : 0.05f;
        const float tx = cafe->bird_pos[0] - dx / len * 3.0f;
        const float tz = cafe->bird_pos[2] - dz / len * 3.0f;
        bird_move_toward(cafe, tx, py + SH_BIRD_FLY_HEIGHT + 0.6f, tz, SH_BIRD_SPEED, dt);
        bird_face(cafe, px, pz, 2.5f, dt);
        cafe->bird_timer -= dt;
        if (cafe->bird_timer <= 0.0f) {
            cafe->bird_state = SH_BIRD_HOVER;
            cafe->bird_timer = 2.0f + 1.0f * (0.5f + 0.5f * sinf(cafe->bird_phase * 7.3f));
        }
        break;
    }
    case SH_BIRD_RECOIL: {
        /* Knocked back by the hit. */
        const float len = dist > 0.05f ? dist : 0.05f;
        cafe->bird_pos[0] -= dx / len * 2.2f * dt;
        cafe->bird_pos[2] -= dz / len * 2.2f * dt;
        cafe->bird_pos[1] += 0.6f * dt;
        cafe->bird_timer -= dt;
        if (cafe->bird_timer <= 0.0f) {
            cafe->bird_state = SH_BIRD_RETREAT;
            cafe->bird_timer = 0.8f;
        }
        break;
    }
    case SH_BIRD_DYING:
        cafe->aiming = false;
        cafe->fire_anim = 0.0f;
        cafe->bird_pos[1] -= 2.4f * dt;
        cafe->bird_yaw += 2.0f * dt;
        if (cafe->bird_pos[1] < py + 0.08f) {
            cafe->bird_pos[1] = py + 0.08f;
        }
        if (cafe->bird_pos[1] <= py + 0.12f) {
            cafe->bird_timer -= dt;
            if (cafe->bird_timer <= 0.0f) {
                cafe->bird_state = SH_BIRD_DEAD;
                cafe->bird_pos[1] = py + 0.08f;
            }
        }
        break;
    default:
        break;
    }
    if (bird_clamp_room(cafe, py) && (cafe->bird_state == SH_BIRD_RETREAT || cafe->bird_state == SH_BIRD_RECOIL)) {
        /* Against a wall: stop backing off, swing around Harry instead. */
        cafe->bird_state = SH_BIRD_HOVER;
        cafe->bird_timer = 1.0f;
    }
}

/* Weapon ready (R) + attack (A while aimed). */
static void combat_update(ShCafe *cafe, ShSavegame *save, float px, float pz, float yaw, const ShInputState *input,
                          float dt)
{
    cafe->attack_cd -= dt;
    if (cafe->fire_anim > 0.0f) {
        cafe->fire_anim -= dt;
        if (cafe->fire_anim < 0.0f) {
            cafe->fire_anim = 0.0f;
        }
    }
    if (cafe->combat_text_time > 0.0f) {
        cafe->combat_text_time -= dt;
        if (cafe->combat_text_time <= 0.0f) {
            cafe->combat_text = NULL;
        }
    }

    /* No aiming / firing once the bird is dying — avoids WEPAL bake during death. */
    if (cafe->bird_state == SH_BIRD_DYING || cafe->bird_state == SH_BIRD_DEAD) {
        cafe->aiming = false;
        cafe->fire_anim = 0.0f;
        return;
    }

    /* Ensure a usable weapon is ready when Cybil's gun is in the inventory. */
    if (save->equipped_weapon != SH_ITEM_HANDGUN && save->equipped_weapon != SH_ITEM_POCKET_KNIFE) {
        if (sh_save_item_count(save, SH_ITEM_HANDGUN) > 0) {
            save->equipped_weapon = SH_ITEM_HANDGUN;
        } else if (sh_save_item_count(save, SH_ITEM_POCKET_KNIFE) > 0) {
            save->equipped_weapon = SH_ITEM_POCKET_KNIFE;
        }
    }

    const uint8_t weapon = save->equipped_weapon;
    const bool has_weapon = weapon == SH_ITEM_HANDGUN || weapon == SH_ITEM_POCKET_KNIFE;
    /* Type A: Aim = R2→R, Attack = Cross→A while aimed. */
    const bool want_aim = !cafe->controls_locked && sh_input_held(input, SH_BTN_AIM);
    cafe->aiming = has_weapon && want_aim;
    if (want_aim && !has_weapon) {
        cafe->combat_text = "SELECT : status → Equip weapon";
        cafe->combat_text_time = 1.2f;
        return;
    }
    if (!cafe->aiming || cafe->attack_cd > 0.0f || !sh_input_down(input, SH_BTN_ACTION)) {
        return;
    }

    const bool gun = weapon == SH_ITEM_HANDGUN;
    const float harry[3] = {px, 0.0f, pz};
    if (gun) {
        cafe->ammo = sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS);
    }
    if (gun && cafe->ammo <= 0) {
        sh_snd_at(SH_SFX_HANDGUN_EMPTY, harry, 0.5f);
        cafe->attack_cd = 0.5f;
        cafe->fire_anim = 0.25f; /* dry-fire pose */
        return;
    }
    cafe->attack_cd = gun ? 0.42f : 0.45f;
    cafe->fire_anim = gun ? 0.28f : 0.48f; /* handgun Unk29 ≈ 10 kf @ 36/s */
    if (gun) {
        if (sh_save_item_remove_one(save, SH_ITEM_HANDGUN_BULLETS)) {
            cafe->ammo = sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS);
        } else {
            cafe->ammo = 0;
        }
        cafe->gun_flash = 0.18f;
        sh_snd_at(SH_SFX_HANDGUN_SHOT, harry, 0.5f);
    } else {
        sh_snd_at(SH_SFX_KNIFE_SWING, harry, 0.5f);
    }

    const bool alive = cafe->bird_state >= SH_BIRD_HOVER && cafe->bird_state <= SH_BIRD_RECOIL;
    if (!alive) {
        return;
    }
    const float dx = cafe->bird_pos[0] - px;
    const float dz = cafe->bird_pos[2] - pz;
    const float dist = sqrtf(dx * dx + dz * dz);
    const float to_bird = atan2f(-dx, -dz);
    const float off = fabsf(wrap_angle(to_bird - yaw));
    const float range = gun ? SH_HANDGUN_RANGE : SH_KNIFE_RANGE;
    const float cone = gun ? DEG(22.0f) : DEG(45.0f);
    if (dist > range || off > cone) {
        return;
    }

    /* AirScreamer_DamageTake */
    const float dmg = gun ? SH_HANDGUN_DAMAGE : SH_KNIFE_DAMAGE;
    cafe->bird_hp -= dmg;
    cafe->hit_flash = 0.15f;
    if (cafe->bird_hp <= 0.0f) {
        cafe->bird_hp = 0.0f;
        cafe->bird_state = SH_BIRD_DYING;
        cafe->bird_timer = 1.6f;
        cafe->bird_anim = 0.0f;
        cafe->aiming = false;
        cafe->fire_anim = 0.0f;
        cafe->combat_text = NULL;
        /* AirScreamerFlag_6: stop sfxVolumes_CE8[2], play [6] */
        sh_snd_stop(SH_SFX_BIRD_1376);
        sh_snd_at(SH_SFX_BIRD_DEATH, cafe->bird_pos, 196.0f / 256.0f);
    } else {
        /* Recoil on heavy hits or when weak (<=120). */
        if (dmg > 45.0f || cafe->bird_hp <= 120.0f) {
            cafe->bird_state = SH_BIRD_RECOIL;
            cafe->bird_timer = 0.6f;
        }
        sh_snd_at(SH_SFX_BIRD_HURT, cafe->bird_pos, 196.0f / 256.0f); /* AirScreamerFlag_3 */
    }
}

/* ---- explore ------------------------------------------------------------- */

/* ---- MAP_POINTS / MAP_EVENTS (map0_s01_header.c, map0_s01_events_data.c) -- */

typedef struct CafePoint {
    float x, z;   /* PS1 metres */
    int ang_q8;   /* triggerParam0 (OBB facing) */
    int len;      /* triggerParam1 (OBB facing, half length = len / 16 m) */
} CafePoint;

static const CafePoint s_points[15] = {
    {1.0f, 260.0f, 0, 0},   {5.1f, 274.1f, 0, 0},    {5.0f, 267.3f, 0, 0},    {5.0f, 267.3f, 0, 0},
    {2.3f, 273.9f, 0, 0},   {3.0f, 270.5f, 0, 32},   {0.8f, 269.5f, 64, 8},   {22.2f, 56.5f, 192, 8},
    {3.6f, 266.5f, 0, 4},   {4.8f, 274.5f, 192, 3},  {4.9f, 265.9f, 192, 4},  {3.9f, 265.7f, 64, 12},
    {3.3f, 265.7f, 192, 12}, {5.1f, 273.7f, 0, 0},   {4.95f, 267.5f, 0, 0},
};

#define EV_HEALTH_DRINK0 SH_EV_HEALTH_DRINK0
#define EV_HEALTH_DRINK1 SH_EV_HEALTH_DRINK1

enum {
    PROMPT_NONE = 0,
    PROMPT_KNIFE = 1,
    PROMPT_LIGHT = 2,
    PROMPT_MAP = 3,
    PROMPT_RADIO = 4,
    PROMPT_DRINK0 = 7,
    PROMPT_DRINK1 = 8,
    PROMPT_READ = 9, /* SysState_ReadMessage */
};

static bool prompt_is_item(int kind)
{
    return (kind >= PROMPT_KNIFE && kind <= PROMPT_RADIO) || kind == PROMPT_DRINK0 || kind == PROMPT_DRINK1;
}

/* Event_CollideFacingCheck. Harry pose in 3DS space.
 * Reach is slightly larger than PS1's 0.8 m so counter-edge pickups (drink next to knife) stay hittable. */
static bool trig_facing(int pt, float px, float pz, float yaw)
{
    const CafePoint *p = &s_points[pt];
    const float a = -yaw;
    const float bx = px - sinf(a) * 0.125f;
    const float bz = -pz - cosf(a) * 0.125f;
    const float dx = p->x - bx;
    const float dz = p->z - bz;
    const float reach = 1.05f;
    if (fabsf(dx) > reach || fabsf(dz) > reach || dx * dx + dz * dz > reach * reach) {
        return false;
    }
    return fabsf(wrap_angle(a - atan2f(dx, dz))) <= DEG(35.0f);
}

static float cross2(float ax, float az, float bx, float bz)
{
    return ax * bz - az * bx;
}

/* Event_CollideObbFacingCheck: Harry's 0.5 m reach must cross the point's segment. */
static bool trig_obb_facing(int pt, float px, float pz, float yaw)
{
    const CafePoint *p = &s_points[pt];
    const float a = -yaw;
    const float ang = (float)p->ang_q8 * (2.0f * PI_F / 256.0f);
    const float hx = -cosf(ang) * (float)p->len / 16.0f;
    const float hz = sinf(ang) * (float)p->len / 16.0f;
    const float x0 = px, z0 = -pz;
    const float rx = sinf(a) * 0.5f, rz = cosf(a) * 0.5f;
    const float s0x = p->x - hx - x0, s0z = p->z - hz - z0;
    const float sdx = 2.0f * hx, sdz = 2.0f * hz;
    const float den = cross2(rx, rz, sdx, sdz);
    if (fabsf(den) < 1e-6f) {
        return false;
    }
    const float t = cross2(s0x, s0z, sdx, sdz) / den;
    const float u = cross2(s0x, s0z, rx, rz) / den;
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}

static void take_item(ShCafe *cafe, ShSavegame *save, int kind, float *px, float *pz, float *yaw)
{
    switch (kind) {
    case PROMPT_KNIFE:
        sh_save_item_add(save, SH_ITEM_POCKET_KNIFE, 1);
        sh_save_event_set(save, SH_EV_KNIFE);
        /* Equip immediately so R+A works without opening the status screen first. */
        if (save->equipped_weapon == SH_ITEM_UNEQUIPPED) {
            save->equipped_weapon = SH_ITEM_POCKET_KNIFE;
        }
        break;
    case PROMPT_LIGHT:
        sh_save_item_add(save, SH_ITEM_FLASHLIGHT, 1);
        sh_save_event_set(save, SH_EV_FLASHLIGHT);
        sh_save_flashlight_set(save, false); /* Game_TurnFlashlightOff */
        break;
    case PROMPT_MAP:
        /* MapEvent_MapItemTake steps 6..11: close the paper map, fade back in on CAFE2 frame 0. */
        sh_save_event_set(save, SH_EV_MAP);
        save->has_map = true;
        cafe->map_open = false;
        cafe->fade = 1.0f;
        fade_to(cafe, 0.0f, 0.5f);
        map_peek_begin(cafe, px, pz, yaw);
        break;
    case PROMPT_RADIO:
        sh_save_item_add(save, SH_ITEM_RADIO, 1);
        sh_save_event_set(save, SH_EV_RADIO);
        sh_save_radio_set(save, true); /* itemToggleFlags |= RadioOn */
        break;
    case PROMPT_DRINK0:
    case PROMPT_DRINK1:
        sh_save_item_add(save, SH_ITEM_HEALTH_DRINK, 1);
        sh_save_event_set(save, kind == PROMPT_DRINK0 ? EV_HEALTH_DRINK0 : EV_HEALTH_DRINK1);
        break;
    default:
        break;
    }
    if (sh_save_event_get(save, SH_EV_KNIFE) && sh_save_event_get(save, SH_EV_MAP)) {
        sh_save_event_set(save, SH_EV_KNIFE_AND_MAP);
    }
}

static void begin_leave_town(ShCafe *cafe)
{
    cafe->stage = SH_CAFE_LEAVING;
    cafe->step = 0;
    cafe->controls_locked = true;
    cafe->cam_active = false;
    cafe->aiming = false;
    cafe->map_open = false;
    cafe->prompt_open = false;
    cafe->prompt_kind = 0;
    msg_clear(cafe);
    fade_to(cafe, 1.0f, 1.2f);
}

static void prompt_close(ShCafe *cafe)
{
    cafe->prompt_open = false;
    cafe->pickup_active = false;
    cafe->prompt_kind = PROMPT_NONE;
    cafe->map_open = false;
}

static void open_read(ShCafe *cafe, int msg)
{
    cafe->prompt_kind = PROMPT_READ;
    cafe->prompt_msg = msg;
    cafe->prompt_open = true;
}

static void open_item(ShCafe *cafe, int kind, int msg)
{
    cafe->prompt_kind = kind;
    cafe->prompt_msg = msg;
    if (kind == PROMPT_MAP) {
        /* Paper map close-up replaces the 3D item (Screen_BackgroundImgDraw). */
        cafe->map_open = true;
        cafe->prompt_open = true;
        return;
    }
    cafe->pickup_active = true;
    cafe->pickup_scale = 0.0f;
    cafe->pickup_spin = 0.0f;
    cafe->prompt_open = false;
}

/* Among overlapping facing pickups (knife ↔ drink0, light ↔ map ↔ drink1), take the nearest. */
static void consider_facing_item(int pt, int kind, int msg, float px, float pz, float yaw, int *best_kind,
                                 int *best_msg, float *best_d2)
{
    if (!trig_facing(pt, px, pz, yaw)) {
        return;
    }
    const CafePoint *p = &s_points[pt];
    const float dx = p->x - px;
    const float dz = p->z - (-pz);
    const float d2 = dx * dx + dz * dz;
    if (d2 < *best_d2) {
        *best_d2 = d2;
        *best_kind = kind;
        *best_msg = msg;
    }
}

/* Walks MAP_EVENTS in table order; returns true when an event fired. */
static bool cafe_events_button(ShCafe *cafe, ShSavegame *save, float *px, float *pz, float *yaw)
{
    const float x = *px, z = *pz, a = *yaw;
    const bool ev49 = sh_save_event_get(save, SH_EV_CAFE_DONE);
    const bool ev50 = sh_save_event_get(save, SH_EV_RADIO_ON_50);
    const bool bird_dead = sh_save_event_get(save, SH_EV_BIRD_DIED);

    /* [4] door: Air Screamer intro once knife + map are held */
    if (sh_save_event_get(save, SH_EV_KNIFE_AND_MAP) && !sh_save_event_get(save, SH_EV_BIRD_INTRO) &&
        !sh_save_event_get(save, SH_EV_47) && trig_obb_facing(6, x, z, a)) {
        bird_intro_begin(cafe, save, px, pz, yaw);
        return true;
    }
    if (trig_obb_facing(6, x, z, a)) {
        if (sh_save_event_get(save, SH_EV_RADIO)) {
            begin_leave_town(cafe); /* [6] LoadOverlay MAP2_S00 */
        } else if (bird_dead) {
            open_read(cafe, 71); /* [7] */
        } else if (ev50) {
            open_read(cafe, 70); /* [8] */
        } else {
            open_read(cafe, 67); /* [9] */
        }
        return true;
    }
    /* Facing pickups: nearest wins so the drink next to the knife is reachable. */
    {
        int best_kind = 0, best_msg = 0;
        float best_d2 = 1e9f;
        if (!sh_save_event_get(save, EV_HEALTH_DRINK0)) {
            consider_facing_item(13, PROMPT_DRINK0, 6, x, z, a, &best_kind, &best_msg, &best_d2);
        }
        if (!sh_save_event_get(save, EV_HEALTH_DRINK1)) {
            consider_facing_item(14, PROMPT_DRINK1, 6, x, z, a, &best_kind, &best_msg, &best_d2);
        }
        if (!sh_save_event_get(save, SH_EV_KNIFE)) {
            consider_facing_item(1, PROMPT_KNIFE, 60, x, z, a, &best_kind, &best_msg, &best_d2);
        }
        if (!sh_save_event_get(save, SH_EV_FLASHLIGHT)) {
            consider_facing_item(2, PROMPT_LIGHT, 61, x, z, a, &best_kind, &best_msg, &best_d2);
        }
        if (!sh_save_event_get(save, SH_EV_MAP)) {
            consider_facing_item(3, PROMPT_MAP, 59, x, z, a, &best_kind, &best_msg, &best_d2);
        }
        if (best_kind != 0) {
            open_item(cafe, best_kind, best_msg);
            return true;
        }
    }
    /* [13]..[15] radio */
    if (trig_facing(4, x, z, a)) {
        if (bird_dead && !sh_save_event_get(save, SH_EV_RADIO)) {
            open_item(cafe, PROMPT_RADIO, 64);
            return true;
        }
        if (ev50) {
            open_read(cafe, 69);
            return true;
        }
        if (ev49) {
            open_read(cafe, 65);
            return true;
        }
    }
    /* [16] notebook on the counter (save point [17] not ported) */
    if (ev50 && trig_obb_facing(10, x, z, a)) {
        open_read(cafe, 75);
        return true;
    }
    /* [18]..[21] pinball machines, TV */
    if (trig_obb_facing(8, x, z, a) || trig_obb_facing(11, x, z, a) || trig_obb_facing(12, x, z, a)) {
        open_read(cafe, 73);
        return true;
    }
    if (trig_obb_facing(9, x, z, a)) {
        open_read(cafe, 74);
        return true;
    }
    return false;
}

static bool explore_update(ShCafe *cafe, ShSavegame *save, float *px, float *py, float *pz, float *yaw,
                           const ShInputState *input, float dt)
{
    (void)py;
    const bool press = sh_input_down(input, KEY_A);
    const bool back = sh_input_down(input, KEY_B);

    map_peek_update(cafe, save, *px, *pz, dt);

    if (cafe->pickup_active) {
        /* Gfx_PickupItemAnimate: grow for 0.25 s without spin, then 180 deg/s with Yes/No. */
        cafe->controls_locked = true;
        if (cafe->pickup_scale < 1.0f) {
            cafe->pickup_scale += dt * 4.0f;
            if (cafe->pickup_scale >= 1.0f) {
                cafe->pickup_scale = 1.0f;
                cafe->prompt_open = true;
            }
        } else {
            cafe->pickup_spin = wrap_angle(cafe->pickup_spin + PI_F * dt);
        }
    }

    if (cafe->prompt_open) {
        cafe->controls_locked = true;
        if (cafe->prompt_kind == PROMPT_READ) {
            if (press || back) {
                const CafeMsg *m = msg_get(cafe->prompt_msg);
                if (m && !m->end && msg_get(cafe->prompt_msg + 1)) {
                    cafe->prompt_msg++;
                } else {
                    prompt_close(cafe);
                }
            }
        } else if (prompt_is_item(cafe->prompt_kind)) {
            if (press) {
                const int kind = cafe->prompt_kind;
                sh_snd_play(SH_SFX_MENU_CONFIRM, 0, 64);
                prompt_close(cafe);
                take_item(cafe, save, kind, px, pz, yaw);
            } else if (back) {
                sh_snd_play(SH_SFX_MENU_CANCEL, 0, 64);
                prompt_close(cafe);
            }
        } else if (press || back) {
            prompt_close(cafe);
        }
        return true;
    }
    if (cafe->pickup_active) {
        return true;
    }
    cafe->controls_locked = false;

    if (press && cafe_events_button(cafe, save, px, pz, yaw)) {
        cafe->controls_locked = cafe->stage == SH_CAFE_EXPLORE || cafe->stage == SH_CAFE_DONE;
        return true;
    }
    return false;
}

/* ---- public -------------------------------------------------------------- */

bool sh_cafe_update(ShCafe *cafe, ShSavegame *save, float *player_x, float *player_y, float *player_z,
                    float *player_yaw, float *radio_intensity, const ShInputState *input, float dt)
{
    if (!cafe || cafe->stage == SH_CAFE_OFF || !save || !input) {
        return false;
    }

    /* Game_Main / SysState_MapScreen: gameplay time is frozen in the paper map. */
    if (cafe->map_open && !cafe->prompt_open && !cafe->pickup_active &&
        (cafe->stage == SH_CAFE_EXPLORE || cafe->stage == SH_CAFE_DONE ||
         cafe->stage == SH_CAFE_BIRD_FIGHT)) {
        cafe->controls_locked = true;
        cafe->aiming = false;
        if (radio_intensity) {
            *radio_intensity = 0.0f;
        }
        return true;
    }

    fade_update(cafe, dt);

    /* SysState_GameOver stub: lock everything, fade out, then hand control back to the shell. */
    if (cafe->harry_dying) {
        cafe->die_timer += dt;
        cafe->controls_locked = true;
        cafe->aiming = false;
        cafe->fire_anim = 0.0f;
        cafe->combat_text = "Harry collapsed...";
        cafe->combat_text_time = 1.0f;
        if (cafe->die_timer >= 2.5f && fade_done(cafe) && cafe->fade >= 0.99f) {
            cafe->game_over = true;
        }
        if (radio_intensity) {
            *radio_intensity = 0.0f;
        }
        return true;
    }
    if (cafe->gun_flash > 0.0f) {
        cafe->gun_flash -= dt;
        if (cafe->gun_flash < 0.0f) {
            cafe->gun_flash = 0.0f;
        }
    }
    cafe->radio = -1.0f;
    cafe->cybil_anim += dt;
    if (cafe->harry_anim_status != 0) {
        cafe->harry_anim_time += dt;
    }
    if ((int)cafe->bird_state != cafe->bird_anim_state) {
        cafe->bird_anim_state = (int)cafe->bird_state;
        cafe->bird_anim = 0.0f;
    }
    cafe->bird_anim += dt;
    bool lock = false;

    switch (cafe->stage) {
    case SH_CAFE_CUTSCENE:
        cutscene_update(cafe, save, player_x, player_z, player_yaw, input, dt);
        lock = true;
        break;

    case SH_CAFE_EXPLORE:
    case SH_CAFE_DONE:
        lock = explore_update(cafe, save, player_x, player_y, player_z, player_yaw, input, dt);
        if (!lock && !cafe->map_open) {
            /* Allow aim / fire pose (and dry-fire) outside the Air Screamer fight. */
            combat_update(cafe, save, *player_x, *player_z, *player_yaw, input, dt);
        } else {
            cafe->aiming = false;
        }
        if (sh_save_event_get(save, SH_EV_RADIO_STATIC)) {
            cafe->radio = 0.0f;
        }
        break;

    case SH_CAFE_LEAVING:
        cafe->controls_locked = true;
        cafe->aiming = false;
        cafe->cam_active = false;
        lock = true;
        if (fade_done(cafe) && cafe->fade >= 0.99f) {
            cafe->leave_town = true;
            cafe->stage = SH_CAFE_OFF;
        }
        break;

    case SH_CAFE_BIRD_INTRO:
        bird_intro_update(cafe, save, player_x, player_z, player_yaw, input, dt);
        lock = true;
        break;

    case SH_CAFE_BIRD_FIGHT:
        cafe->controls_locked = false;
        cafe->radio = 0.95f;
        bird_fight_update(cafe, save, *player_x, *player_y, *player_z, dt);
        if (cafe->bird_state != SH_BIRD_DYING && cafe->bird_state != SH_BIRD_DEAD) {
            combat_update(cafe, save, *player_x, *player_z, *player_yaw, input, dt);
        } else {
            cafe->aiming = false;
            cafe->fire_anim = 0.0f;
        }
        if (cafe->bird_state == SH_BIRD_DEAD) {
            /* Map_WorldObjectsUpdate: health == NO_VALUE → flag 48, clear 49/50 */
            sh_save_event_set(save, SH_EV_BIRD_DIED);
            sh_save_event_clear(save, SH_EV_CAFE_DONE);
            sh_save_event_clear(save, SH_EV_RADIO_ON_50);
            cafe->stage = SH_CAFE_BIRD_DEATH;
            cafe->step = 0;
            cafe->aiming = false;
            cafe->fire_anim = 0.0f;
            cafe->map_open = false;
            msg_clear(cafe);
        }
        lock = false; /* aiming (sh_cafe_aiming) only blocks walking in game.c */
        break;

    case SH_CAFE_BIRD_DEATH:
        cafe->controls_locked = true;
        cafe->aiming = false;
        cafe->fire_anim = 0.0f;
        cafe->radio = 0.0f;
        switch (cafe->step) {
        case 0:
            sh_snd_stop(SH_SFX_RADIO_INTERFERENCE);
            sh_snd_stop(SH_SFX_RADIO_STATIC);
            sh_save_event_clear(save, SH_EV_RADIO_STATIC);
            step_next(cafe);
            /* fallthrough */
        case 1:
            if (msg_run(cafe, 68, input, dt)) {
                step_next(cafe);
            }
            break;
        default:
            sh_save_event_set(save, SH_EV_BIRD_DEATH_PLAYED);
            cafe->stage = SH_CAFE_DONE;
            cafe->controls_locked = false;
            break;
        }
        lock = true;
        break;

    default:
        break;
    }

    /* Map_WorldObjectsUpdate: radio on the table keeps hissing while flag 52 is set. */
    if (sh_save_event_get(save, SH_EV_RADIO_STATIC)) {
        static const float k_radio_ps1[3] = {2.3f, -0.6f, 273.9f}; /* D_800DE154 */
        float pos[3];
        ps1_to_3ds(k_radio_ps1, pos);
        sh_snd_falloff(SH_SFX_RADIO_INTERFERENCE, pos, cafe->radio_vol, 16.0f, 0);
        sh_snd_falloff(SH_SFX_RADIO_STATIC, pos, cafe->radio_vol, 16.0f, 0);
    }

    /* Fixed / rail cameras for free roam + fight (cutscenes keep DMS cams). */
    if (player_x && player_y && player_z && player_yaw) {
        update_gameplay_camera(cafe, *player_x, *player_y, *player_z, *player_yaw, dt);
    }

    if (radio_intensity) {
        *radio_intensity = cafe->radio;
    }
    return lock;
}

const char *sh_cafe_subtitle(const ShCafe *cafe)
{
    if (!cafe) {
        return "";
    }
    /* Event_ItemTake Yes/No and SysState_ReadMessage — top screen like MAP_MESSAGES. */
    if (cafe->prompt_open) {
        const CafeMsg *pm = msg_get(cafe->prompt_msg);
        return pm ? pm->text : "";
    }
    if (cafe->pickup_active) {
        return "";
    }
    if (cafe->map_open) {
        return "Residential area map";
    }
    if (cafe->msg_idx < 0 || cafe->msg_load != 0) {
        return "";
    }
    const CafeMsg *m = msg_get(cafe->msg_idx);
    return m ? m->text : "";
}

void sh_cafe_bgm_update(const ShCafe *cafe, const ShSavegame *save, bool player_dead, float dt)
{
    if (!cafe || !save || cafe->stage == SH_CAFE_OFF) {
        sh_bgm_layers_update(SH_BGM_LAYER1, 240.0f, player_dead, dt); /* Bgm_Update default */
        return;
    }
    /* Map_RoomBgmInit_CondFalse */
    const bool bird_died = sh_save_event_get(save, SH_EV_BIRD_DIED);
    float fade = 0.333f;
    unsigned flags;
    if (!(bird_died || sh_save_event_get(save, SH_EV_CAFE_DONE))) {
        fade = 240.0f;
        flags = sh_save_event_get(save, SH_EV_FMV) && sh_bgm_current() == 9 ? SH_BGM_LAYER2
                                                                             : SH_BGM_LAYER1 | SH_BGM_MUTE_ALL;
    } else if (sh_save_event_get(save, SH_EV_47)) {
        if (!bird_died) {
            fade = 240.0f;
            flags = SH_BGM_KEEP_ALIVE;
        } else {
            fade = 0.125f;
            flags = SH_BGM_LAYER1 | SH_BGM_MUTE_ALL;
        }
    } else {
        flags = SH_BGM_LAYER1;
    }
    sh_bgm_layers_update(flags, fade, player_dead, dt);
}

const char *sh_cafe_dialogue(const ShCafe *cafe)
{
    if (!cafe) {
        return "";
    }
    if (cafe->prompt_open) {
        const CafeMsg *m = msg_get(cafe->prompt_msg);
        return m ? m->text : "";
    }
    if (cafe->got_text && cafe->got_time > 0.0f) {
        return cafe->got_text;
    }
    return "";
}

const char *sh_cafe_hint(const ShCafe *cafe)
{
    if (!cafe || cafe->stage == SH_CAFE_OFF) {
        return "";
    }
    switch (cafe->stage) {
    case SH_CAFE_CUTSCENE:
        return "START : passer";
    case SH_CAFE_BIRD_INTRO:
    case SH_CAFE_BIRD_DEATH:
        return "";
    case SH_CAFE_BIRD_FIGHT:
        return "R : viser    A : attaquer\nL : courir";
    default:
        break;
    }
    if (cafe->prompt_open) {
        return cafe->prompt_kind == 9 ? "A / B : OK" : "A : Yes     B : No";
    }
    return "L : courir   R : viser   A : action\nY : carte   SELECT : statut";
}

bool sh_cafe_letterbox(const ShCafe *cafe)
{
    return cafe && (cafe->stage == SH_CAFE_CUTSCENE || cafe->stage == SH_CAFE_BIRD_INTRO);
}

float sh_cafe_fade(const ShCafe *cafe)
{
    return cafe ? cafe->fade : 0.0f;
}

bool sh_cafe_camera(const ShCafe *cafe, ShCamera *out)
{
    if (!cafe || !cafe->cam_active || cafe->stage == SH_CAFE_OFF) {
        return false;
    }
    if (out) {
        *out = cafe->cam;
    }
    return true;
}

bool sh_cafe_actor_pose(const ShCafe *cafe, int actor, ShActorPose *out)
{
    if (!cafe || !out || cafe->stage == SH_CAFE_OFF) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->slot = actor;
    if (actor == SH_CAFE_ACTOR_CYBIL) {
        out->visible = cafe->stage == SH_CAFE_CUTSCENE && cafe->cybil_visible;
        out->pos[0] = cafe->cybil_pos[0];
        out->pos[1] = cafe->cybil_pos[1];
        out->pos[2] = cafe->cybil_pos[2];
        out->yaw = cafe->cybil_yaw;
        return out->visible;
    }
    if (actor == SH_CAFE_ACTOR_BIRD) {
        out->visible = cafe->bird_state != SH_BIRD_HIDDEN && cafe->bird_pos[1] > -5.0f;
        out->pos[0] = cafe->bird_pos[0];
        out->pos[1] = cafe->bird_pos[1];
        out->pos[2] = cafe->bird_pos[2];
        out->yaw = cafe->bird_yaw;
        return out->visible;
    }
    return false;
}

bool sh_cafe_harry_cutscene_anim(const ShCafe *cafe, int *status_out, float *time_out)
{
    if (!cafe || cafe->stage != SH_CAFE_CUTSCENE || cafe->harry_anim_status == 0) {
        return false;
    }
    if (status_out) {
        *status_out = cafe->harry_anim_status;
    }
    if (time_out) {
        *time_out = cafe->harry_anim_time;
    }
    return true;
}

int sh_cafe_actor_keyframe(const ShCafe *cafe, int actor, int kf_count)
{
    if (!cafe || kf_count <= 0) {
        return 0;
    }
    if (actor == SH_CAFE_ACTOR_CYBIL) {
        /* CYBIL_ANIM_INFOS playback ranges (SBL.ANM). */
        int start = 0, end = 15;
        float fps = 5.0f;
        bool loop = true;
        switch (cafe->cybil_clip) {
        case 5:
            start = 63;
            end = 75;
            fps = 15.0f;
            break;
        case 6:
            start = 76;
            end = 79;
            fps = 1.0f;
            break;
        case 7:
            start = 80;
            end = 108;
            fps = 3.0f;
            loop = false;
            break;
        case 8:
            start = 109;
            end = 122;
            fps = 10.0f;
            loop = false;
            break;
        case 9:
            start = 123;
            end = 151;
            fps = 18.0f;
            loop = false;
            break;
        case 10:
            start = 152;
            end = 162;
            fps = 7.0f;
            loop = false;
            break;
        case 11:
            start = 163;
            end = 173;
            fps = 12.0f;
            loop = false;
            break;
        default: /* CybilAnim_1 idle stand */
            start = 0;
            end = 15;
            fps = 5.0f;
            break;
        }
        if (end >= kf_count) {
            end = kf_count > 0 ? kf_count - 1 : 0;
        }
        if (start > end) {
            start = end;
        }
        const float span = (float)(end - start + 1);
        float t = cafe->cybil_anim * fps;
        if (loop) {
            while (t >= span) {
                t -= span;
            }
        } else if (t > span - 1.0f) {
            t = span - 1.0f;
        }
        const int kf = start + (int)t;
        return kf < 0 ? 0 : (kf >= kf_count ? kf_count - 1 : kf);
    }
    if (actor == SH_CAFE_ACTOR_BIRD) {
        /*
         * BIRD.ANM clips (file order follows e_AirScreamerAnim):
         *   Still 0..11, HoverBiteAttack 26..55, Death 115..129,
         *   HoverRecoil 154..171, Glide 377..379, HoverVariable 384..407.
         */
        int start = 384, end = 407;
        float fps = 20.0f;
        bool loop = true;
        switch (cafe->bird_state) {
        case SH_BIRD_PERCH:
            start = 0;
            end = 11;
            fps = 8.0f;
            break;
        case SH_BIRD_DIVE:
            start = 26;
            end = 55;
            fps = 30.0f;
            loop = false;
            break;
        case SH_BIRD_RECOIL:
            start = 154;
            end = 171;
            fps = 24.0f;
            loop = false;
            break;
        case SH_BIRD_APPROACH:
        case SH_BIRD_SCRIPTED:
            start = 377;
            end = 379;
            fps = 12.0f;
            break;
        case SH_BIRD_DYING:
        case SH_BIRD_DEAD:
            start = 115;
            end = 129;
            fps = 15.0f;
            loop = false;
            break;
        default:
            break;
        }
        if (end >= kf_count) {
            end = kf_count - 1;
        }
        if (start > end) {
            start = end;
        }
        const int span = end - start + 1;
        int kf = (int)(cafe->bird_anim * fps);
        kf = loop ? start + (kf % span) : (kf >= span ? end : start + kf);
        return kf;
    }
    return 0;
}

int sh_cafe_world_state(const ShCafe *cafe, const ShSavegame *save)
{
    (void)cafe;
    if (!save) {
        return 0;
    }
    if (sh_save_event_get(save, SH_EV_47)) {
        return 2;
    }
    if (sh_save_event_get(save, SH_EV_BIRD_INTRO) && !sh_save_event_get(save, SH_EV_46)) {
        return 1;
    }
    return 0;
}

unsigned sh_cafe_world_key(const ShCafe *cafe, const ShSavegame *save)
{
    if (!cafe || !save || cafe->stage == SH_CAFE_OFF) {
        return 0;
    }
    unsigned key = 1u | ((unsigned)sh_cafe_world_state(cafe, save) << 1);
    key |= sh_save_event_get(save, SH_EV_KNIFE) ? 1u << 4 : 0u;
    key |= sh_save_event_get(save, SH_EV_FLASHLIGHT) ? 1u << 5 : 0u;
    key |= sh_save_event_get(save, SH_EV_MAP) ? 1u << 6 : 0u;
    key |= sh_save_event_get(save, SH_EV_RADIO) ? 1u << 7 : 0u;
    key |= sh_save_event_get(save, SH_EV_HEALTH_DRINK0) ? 1u << 8 : 0u;
    key |= sh_save_event_get(save, SH_EV_HEALTH_DRINK1) ? 1u << 9 : 0u;
    return key;
}

bool sh_cafe_leave_town(const ShCafe *cafe)
{
    return cafe && cafe->leave_town;
}

bool sh_cafe_game_over(const ShCafe *cafe)
{
    return cafe && cafe->game_over;
}

void sh_cafe_cycle_weapon(ShCafe *cafe, ShSavegame *save)
{
    if (!cafe || !save || cafe->controls_locked) {
        return;
    }
    const bool knife = sh_save_item_count(save, SH_ITEM_POCKET_KNIFE) > 0;
    const bool gun = sh_save_item_count(save, SH_ITEM_HANDGUN) > 0;
    if (knife && gun) {
        save->equipped_weapon =
            save->equipped_weapon == SH_ITEM_HANDGUN ? SH_ITEM_POCKET_KNIFE : SH_ITEM_HANDGUN;
    } else if (gun) {
        save->equipped_weapon = SH_ITEM_HANDGUN;
    } else if (knife) {
        save->equipped_weapon = SH_ITEM_POCKET_KNIFE;
    }
    cafe->got_text = save->equipped_weapon == SH_ITEM_HANDGUN      ? "Equipped: Handgun"
                     : save->equipped_weapon == SH_ITEM_POCKET_KNIFE ? "Equipped: Kitchen knife"
                                                                     : "Unequipped";
    cafe->got_time = 1.4f;
    cafe->combat_text = cafe->got_text;
    cafe->combat_text_time = 1.4f;
}

bool sh_cafe_map_open(const ShCafe *cafe)
{
    return cafe && cafe->map_open;
}

void sh_cafe_toggle_map(ShCafe *cafe, const ShSavegame *save)
{
    if (!cafe || !save || !sh_save_event_get(save, SH_EV_MAP)) {
        return;
    }
    if (cafe->stage != SH_CAFE_EXPLORE && cafe->stage != SH_CAFE_DONE && cafe->stage != SH_CAFE_BIRD_FIGHT) {
        return;
    }
    cafe->map_open = !cafe->map_open;
    if (cafe->map_open) {
        cafe->got_text = "Residential area map\nY / B : close";
        cafe->got_time = 2.0f;
    }
}

bool sh_cafe_aiming(const ShCafe *cafe)
{
    return cafe && cafe->aiming;
}

float sh_cafe_gun_flash(const ShCafe *cafe)
{
    return cafe ? cafe->gun_flash : 0.0f;
}

bool sh_cafe_pickup_active(const ShCafe *cafe)
{
    return cafe && cafe->pickup_active;
}

bool sh_cafe_pickup_item(const ShCafe *cafe, int *item_id, float *scale, float *spin)
{
    if (!cafe || !cafe->pickup_active) {
        return false;
    }
    int id;
    switch (cafe->prompt_kind) {
    case PROMPT_KNIFE: id = 0x80; break;  /* InvItemId_KitchenKnife */
    case PROMPT_LIGHT: id = 0xE0; break;  /* InvItemId_Flashlight */
    case PROMPT_RADIO: id = 0xE1; break;  /* InvItemId_PocketRadio */
    case PROMPT_DRINK0:
    case PROMPT_DRINK1: id = 0x20; break; /* InvItemId_HealthDrink */
    default: return false;
    }
    if (item_id) *item_id = id;
    if (scale) *scale = cafe->pickup_scale;
    if (spin) *spin = cafe->pickup_spin;
    return true;
}
