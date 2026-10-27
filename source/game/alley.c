#include "sh3ds/alley.h"
#include "sh3ds/alley_progress.h"
#include "sh3ds/controls.h"
#include "sh3ds/snd.h"
#include "sh3ds/sound.h"
#include "sh3ds/vc.h"

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PI_F ((float)M_PI)
#define DEG(d) ((d) * (PI_F / 180.0f))
#define ANIM_ST(idx) (((idx) << 1) | 1)

#include "alley_camera_data.h"
#include "alley_npc_data.h"
#include "overlay_data.h"

/* MAP_POINTS / cutscene anchors (PS1 → 3DS). */
#define SH_INTRO_SPAWN_X (-6.2f)
#define SH_INTRO_SPAWN_Z (-160.5f)
#define SH_CHERYL_X (-29.5f)
#define SH_CHERYL_Z (-128.7f)
#define SH_ALLEY_GATE_X (-56.0f)
#define SH_ALLEY_GATE_Z (-126.0f)
#define SH_ALLEY_X (-62.0f)
#define SH_ALLEY_END_Z (-49.0f)
#define SH_FOOT_Z (-144.0f)

/* Spotted cam — stay on Bachman Rd (PS1 −22, −2.4, 130.1). */
#define SH_SPOT_CAM_X (-22.0f)
#define SH_SPOT_CAM_Y (2.4f)
#define SH_SPOT_CAM_Z (-130.1f)

/* MapMsgAudioCmds for msgs 21..23 / nightmare 27..28. */
#define SH_VO_CHERYL_SPOT 4109
#define SH_VO_WHERE_GOING 4131
#define SH_VO_HEY_WAIT 4132
#define SH_VO_WHAT_IS_THIS 4126
#define SH_VO_WHATS_GOING 4119
#define SH_VO_STOP 19
#define SH_VO_STOP_ALL 22

static void ps1_to_3ds(const float ps1[3], float out[3])
{
    out[0] = ps1[0];
    out[1] = -ps1[1];
    out[2] = -ps1[2];
}

static float dist2_xz(float ax, float az, float bx, float bz)
{
    const float dx = ax - bx;
    const float dz = az - bz;
    return dx * dx + dz * dz;
}

static void fade_to(ShAlley *a, float target, float speed)
{
    a->fade_target = target;
    a->fade_speed = speed > 0.05f ? speed : 0.05f;
}

static void fade_update(ShAlley *a, float dt)
{
    if (fabsf(a->fade - a->fade_target) < 0.001f) {
        a->fade = a->fade_target;
        return;
    }
    const float step = a->fade_speed * dt;
    if (a->fade < a->fade_target) {
        a->fade = fminf(a->fade + step, a->fade_target);
    } else {
        a->fade = fmaxf(a->fade - step, a->fade_target);
    }
}

static bool fade_done(const ShAlley *a)
{
    return fabsf(a->fade - a->fade_target) < 0.01f;
}

static void cam_look_at(ShCamera *cam, float tx, float ty, float tz)
{
    const float dx = tx - cam->pos[0];
    const float dy = ty - cam->pos[1];
    const float dz = tz - cam->pos[2];
    const float horiz = sqrtf(dx * dx + dz * dz);
    cam->yaw = atan2f(-dx, -dz);
    cam->pitch = atan2f(dy, horiz > 0.01f ? horiz : 0.01f);
}

static bool load_dms(ShAlley *alley)
{
    FILE *f = fopen("romfs:/game/anim/OPEN.DMS", "rb");
    if (!f) {
        alley->dms_ok = false;
        alley->dms_size = 0;
        return false;
    }
    alley->dms_size = fread(alley->dms_data, 1, sizeof(alley->dms_data), f);
    fclose(f);
    alley->dms_ok = alley->dms_size > 0 && sh_dms_parse(&alley->dms, alley->dms_data, alley->dms_size);
    return alley->dms_ok;
}

static void sample_open_cam(ShAlley *alley, float frame)
{
    float pos[3], look[3], proj = 0.0f;
    if (!alley->dms_ok || !sh_dms_camera_sample(&alley->dms, frame, pos, look, &proj)) {
        alley->cam_active = false;
        return;
    }
    float p3[3], l3[3];
    ps1_to_3ds(pos, p3);
    ps1_to_3ds(look, l3);
    alley->cam.pos[0] = p3[0];
    alley->cam.pos[1] = p3[1];
    alley->cam.pos[2] = p3[2];
    cam_look_at(&alley->cam, l3[0], l3[1], l3[2]);
    alley->cam.fov_deg = proj > 20.0f ? 2.0f * atan2f(120.0f, proj) * (180.0f / PI_F) : SH_VC_FOV_DEG;
    alley->cam_active = true;
}

static void sample_hero(ShAlley *alley, float *px, float *py, float *pz, float *yaw, float frame)
{
    float pos[3], rot[3];
    const int idx = sh_dms_find_char(&alley->dms, "HERO");
    if (idx < 0 || !sh_dms_char_sample(&alley->dms, idx, frame, pos, rot)) {
        return;
    }
    float p3[3];
    ps1_to_3ds(pos, p3);
    *px = p3[0];
    *py = p3[1];
    *pz = p3[2];
    *yaw = -rot[1];
}

static void set_msg(ShAlley *alley, const char *sub, const char *dlg, float hold)
{
    alley->subtitle = sub;
    alley->dialogue = dlg;
    alley->msg_time = hold;
}

static void msg_tick(ShAlley *alley, float dt)
{
    if (alley->msg_time > 0.0f) {
        alley->msg_time -= dt;
        if (alley->msg_time <= 0.0f) {
            alley->msg_time = 0.0f;
            alley->subtitle = NULL;
        }
    }
}

static bool wait_timer(ShAlley *alley, float seconds, float dt)
{
    alley->wait += dt;
    if (alley->wait >= seconds) {
        alley->wait = 0.0f;
        return true;
    }
    return false;
}

static void step_next(ShAlley *alley)
{
    alley->step++;
    alley->wait = 0.0f;
}

static void harry_set_anim(ShAlley *alley, int status)
{
    if (alley->harry_anim_status != status) {
        alley->harry_anim_status = status;
        alley->harry_anim_time = 0.0f;
    }
}

static void voice_play(ShAlley *alley, uint16_t cmd)
{
    if (alley->voice_on) {
        sh_snd_call(SH_VO_STOP);
    }
    alley->voice_cmd = cmd;
    alley->voice_t = 0.0f;
    alley->voice_on = cmd != 0;
    if (cmd != 0) {
        sh_bgm_status_set(SH_BGM_STATUS_VOICE_DIALOG);
        sh_xa_preload(cmd);
        sh_snd_call(cmd);
    }
}

static void voice_stop(ShAlley *alley)
{
    if (alley->voice_on) {
        sh_snd_call(SH_VO_STOP);
        alley->voice_on = false;
        alley->voice_cmd = 0;
    }
}

static void voice_tick(ShAlley *alley, float dt)
{
    if (!alley->voice_on) {
        return;
    }
    alley->voice_t += dt;
    sh_bgm_status_set(SH_BGM_STATUS_VOICE_DIALOG);
    /* Clips are short; stop after a safe upper bound if still flagged. */
    if (alley->voice_t > 6.0f) {
        voice_stop(alley);
    }
}

static void cheryl_place(ShAlley *alley, float x, float z, float yaw)
{
    alley->cheryl_visible = true;
    alley->cheryl_pos[0] = x;
    alley->cheryl_pos[1] = 0.0f;
    alley->cheryl_pos[2] = z;
    alley->cheryl_yaw = yaw;
}

/* Move toward target; returns true when arrived. */
static bool cheryl_move_to(ShAlley *alley, float tx, float tz, float speed, float dt)
{
    float dx = tx - alley->cheryl_pos[0];
    float dz = tz - alley->cheryl_pos[2];
    float d = sqrtf(dx * dx + dz * dz);
    if (d < 0.15f) {
        alley->cheryl_pos[0] = tx;
        alley->cheryl_pos[2] = tz;
        return true;
    }
    const float step = speed * dt;
    if (step >= d) {
        alley->cheryl_pos[0] = tx;
        alley->cheryl_pos[2] = tz;
    } else {
        alley->cheryl_pos[0] += dx / d * step;
        alley->cheryl_pos[2] += dz / d * step;
    }
    alley->cheryl_yaw = atan2f(-dx, -dz);
    alley->cheryl_anim += dt;
    return false;
}

/* Bachman Rd only — never cut south through lots (PS1 z stays ~128..126). */
static const float k_cheryl_street_wp[][2] = {
    {-35.0f, -128.0f},
    {-45.0f, -127.0f},
    {-56.0f, -126.0f},
};

static const float k_cheryl_alley_wp[][2] = {
    {-62.0f, -110.0f},
    {-62.0f, -90.0f},
    {-62.0f, -70.0f},
    {-62.0f, -49.0f},
};

static void foot_sfx_tick(ShAlley *alley, float px, float pz, float dt)
{
    /* Cheryl_DistantFootstepSfxPlay — only while far from her spawn. */
    const float d2 = dist2_xz(px, pz, SH_CHERYL_X, SH_CHERYL_Z);
    if (d2 < 16.0f * 16.0f) {
        return;
    }
    alley->foot_sfx_t += dt;
    if (alley->foot_sfx_t < 0.8f) {
        return;
    }
    alley->foot_sfx_t = 0.0f;
    /* SFX ahead of Harry toward Cheryl. */
    float pos[3] = {SH_CHERYL_X, 0.0f, SH_CHERYL_Z};
    const float dx = SH_CHERYL_X - px;
    const float dz = SH_CHERYL_Z - pz;
    const float d = sqrtf(dx * dx + dz * dz);
    if (d > 0.1f) {
        pos[0] = px + dx / d * 8.0f;
        pos[2] = pz + dz / d * 8.0f;
    }
    /* Pitch jitter like Rng_GenerateInt(-16, 15). */
    const int pitch = ((int)(alley->timer * 37.0f) & 31) - 16;
    sh_snd_at_pitch(SH_SFX_CHERYL_FOOTSTEP, pos, 0.35f, pitch);
}

static void place_harry(ShAlley *alley, float *px, float *py, float *pz, float *yaw,
                        float x, float z, float yaw_rad)
{
    if (px) {
        *px = x;
    }
    if (pz) {
        *pz = z;
    }
    if (py) {
        *py = 0.0f;
    }
    if (yaw) {
        *yaw = yaw_rad;
    }
    alley->need_ground_snap = true;
    sh_vc_warp(&alley->vc);
}

static void spawn_street(ShAlley *alley, float *px, float *py, float *pz, float *yaw)
{
    alley->timer = -1.0f;
    alley->cam_active = false;
    alley->letterbox = false;
    alley->controls_locked = false;
    alley->harry_anim_status = 0;
    alley->harry_anim_time = 0.0f;
    alley->stage = SH_ALLEY_STREET;
    alley->step = 0;
    place_harry(alley, px, py, pz, yaw, SH_INTRO_SPAWN_X, SH_INTRO_SPAWN_Z, PI_F);
}

void sh_alley_reset(ShAlley *alley)
{
    if (!alley) {
        return;
    }
    voice_stop(alley);
    memset(alley, 0, sizeof(*alley));
    alley->darkness = -1;
    sh_alley_flicker_init(&alley->flicker);
    alley->stage = SH_ALLEY_OFF;
    alley->timer = -1.0f;
    sh_vc_init(&alley->vc, s_alley_camera_roads,
               sizeof(s_alley_camera_roads) / sizeof(s_alley_camera_roads[0]));
    sh_env_set(&alley->environment, s_overlay_info[0].presets[0], s_overlay_info[0].presets[1]);
}

void sh_alley_begin(ShAlley *alley, ShSavegame *save)
{
    if (!alley) {
        return;
    }
    sh_alley_reset(alley);
    alley->stage = SH_ALLEY_OPENING;
    alley->step = 0;
    alley->controls_locked = true;
    alley->letterbox = true;
    alley->fade = 1.0f;
    fade_to(alley, 1.0f, 0.1f);
    load_dms(alley);
    sh_bgm_crossfade(3); /* BgmCmd_Track3 */
    sh_snd_play(SH_SFX_OPENING_AMB, 0, 144); /* Q8(9/16) atten like retail */
    if (save) {
        save->map_idx = 0;
        save->location_id = 0;
    }
}

bool sh_alley_active(const ShAlley *alley)
{
    return alley && alley->stage != SH_ALLEY_OFF && alley->stage != SH_ALLEY_TO_CAFE;
}

bool sh_alley_go_to_cafe(const ShAlley *alley)
{
    return alley && (alley->stage == SH_ALLEY_TO_CAFE || alley->go_to_cafe);
}

static void finish_to_cafe(ShAlley *alley, ShSavegame *save)
{
    if (save) {
        sh_save_event_set(save, SH_EV_OPENING);
        sh_save_event_set(save, SH_EV_FOOTSTEPS);
        sh_save_event_set(save, SH_EV_ALLEY_FMV);
        sh_save_event_set(save, SH_EV_SPOTTED);
        sh_save_event_set(save, SH_EV_INTO_ALLEY);
        sh_save_event_set(save, SH_EV_ALLEY_DARK);
        sh_save_event_set(save, SH_EV_NIGHTMARE);
        sh_save_event_set(save, SH_EV_INTRO_DONE);
    }
    voice_stop(alley);
    sh_snd_stop(SH_SFX_OPENING_AMB);
    sh_snd_stop(SH_SFX_NIGHTMARE_AMB);
    sh_snd_stop(SH_SFX_ALLEY_TENSION);
    sh_snd_stop(SH_SFX_ALLEY_WHEEL);
    sh_snd_call(SH_VO_STOP_ALL); /* SD_Call(22) */
    alley->go_to_cafe = true;
    alley->stage = SH_ALLEY_TO_CAFE;
    alley->controls_locked = true;
    alley->cheryl_visible = false;
    alley->cam_active = false;
    alley->letterbox = false;
    alley->harry_anim_status = 0;
    alley->fade = 1.0f;
    alley->fade_target = 1.0f;
}

static void opening_update(ShAlley *alley, float *px, float *py, float *pz, float *yaw,
                           const ShInputState *input, float dt)
{
    alley->controls_locked = true;
    alley->letterbox = true;

    if (sh_input_down(input, KEY_START) && alley->step > 1) {
        voice_stop(alley);
        fade_to(alley, 0.0f, 2.0f);
        set_msg(alley, NULL, NULL, 0.0f);
        spawn_street(alley, px, py, pz, yaw);
        return;
    }

    switch (alley->step) {
    case 0:
        alley->timer = 0.0f;
        harry_set_anim(alley, ANIM_ST(2));
        fade_to(alley, 0.0f, 0.35f);
        step_next(alley);
        /* fallthrough */
    case 1:
        if (fade_done(alley)) {
            step_next(alley);
        }
        break;
    case 2:
        alley->timer = fminf(alley->timer + dt * 10.0f, 22.0f);
        if (alley->timer >= 22.0f) {
            harry_set_anim(alley, ANIM_ST(52));
            step_next(alley);
        }
        break;
    case 3:
        alley->timer = fminf(alley->timer + dt * 8.0f, 26.0f);
        if (alley->timer >= 26.0f) {
            step_next(alley);
        }
        break;
    case 4:
        alley->timer = fminf(alley->timer + dt * 4.0f, 72.0f);
        set_msg(alley, "Cheryl. Where could you be?", NULL, 2.5f);
        if (wait_timer(alley, 2.5f, dt)) {
            step_next(alley);
        }
        break;
    case 5:
        alley->timer = fminf(alley->timer + dt * 4.0f, 72.0f);
        if (wait_timer(alley, 0.8f, dt)) {
            step_next(alley);
        }
        break;
    case 6:
        alley->timer = fminf(alley->timer + dt * 4.0f, 72.0f);
        set_msg(alley, "It's strange...", NULL, 1.5f);
        if (wait_timer(alley, 1.5f, dt)) {
            step_next(alley);
        }
        break;
    case 7:
        alley->timer = fminf(alley->timer + dt * 4.0f, 72.0f);
        set_msg(alley, "It's quiet. Too quiet.", NULL, 2.0f);
        if (wait_timer(alley, 2.0f, dt)) {
            step_next(alley);
        }
        break;
    case 8:
        alley->timer = fminf(alley->timer + dt * 4.0f, 72.0f);
        set_msg(alley, "This place is like a ghost town.", NULL, 3.0f);
        if (wait_timer(alley, 3.0f, dt) || alley->timer >= 72.0f) {
            step_next(alley);
        }
        break;
    case 9:
        if (wait_timer(alley, 1.5f, dt)) {
            step_next(alley);
        }
        break;
    case 10:
        set_msg(alley, "Cheryl.", NULL, 3.0f);
        if (wait_timer(alley, 3.0f, dt)) {
            step_next(alley);
        }
        break;
    case 11:
        if (wait_timer(alley, 1.0f, dt)) {
            step_next(alley);
        }
        break;
    default:
        set_msg(alley, NULL, NULL, 0.0f);
        spawn_street(alley, px, py, pz, yaw);
        alley->cheryl_visible = false;
        break;
    }

    if (alley->harry_anim_status != 0) {
        alley->harry_anim_time += dt;
    }
    if (alley->timer >= 0.0f) {
        sample_open_cam(alley, alley->timer);
        if (px && py && pz && yaw && alley->dms_ok) {
            sample_hero(alley, px, py, pz, yaw, alley->timer);
        }
    }
}

static bool in_footsteps_zone(float px, float pz)
{
    return pz > SH_FOOT_Z - 6.0f && pz < SH_FOOT_Z + 4.0f && px > -14.0f && px < 24.0f;
}

/* MapEvent_GreyChildrenSpawn poses (PS1 → 3DS). */
static void spawn_grey_children(ShAlley *alley)
{
    alley->grey_count = SH_ALLEY_MAX_GREY;
    alley->need_grey_assets = !alley->grey_assets_loaded;
    alley->cheryl_visible = false;
    for (int i = 0; i < SH_ALLEY_MAX_GREY; i++) {
        ShAlleyGrey *g = &alley->grey[i];
        memset(g, 0, sizeof(*g));
        g->alive = true;
        g->pos[0] = (float)s_alley_grey_spawns[i].x / 4096.0f;
        g->pos[1] = 0.0f;
        g->pos[2] = -(float)s_alley_grey_spawns[i].z / 4096.0f;
        g->yaw = -(float)s_alley_grey_spawns[i].yaw * (2.0f * PI_F / 4096.0f);
        g->hp = 40.0f;
    }
}

static void grey_update(ShAlley *alley, float px, float pz, float dt)
{
    for (int i = 0; i < alley->grey_count; i++) {
        ShAlleyGrey *g = &alley->grey[i];
        if (!g->alive) {
            continue;
        }
        g->anim += dt;
        if (g->cd > 0.0f) {
            g->cd -= dt;
        }
        const float dx = px - g->pos[0];
        const float dz = pz - g->pos[2];
        const float d = sqrtf(dx * dx + dz * dz);
        if (d > 0.35f) {
            const float sp = 1.6f * dt;
            g->pos[0] += dx / d * sp;
            g->pos[2] += dz / d * sp;
            g->yaw = atan2f(-dx, -dz);
        }
    }
}

static int grey_alive_count(const ShAlley *alley)
{
    int n = 0;
    for (int i = 0; i < alley->grey_count; i++) {
        if (alley->grey[i].alive) {
            n++;
        }
    }
    return n;
}

static bool in_redirect_zone(float px, float pz)
{
    /* East back toward Midwich, or south off Bachman before the alley. */
    if (px > -18.0f && pz > -132.0f && pz < -110.0f) {
        return true;
    }
    if (pz < -100.0f && pz > -124.0f && px > -48.0f && px < -20.0f) {
        return true;
    }
    return false;
}

static void set_spot_cam(ShAlley *alley, float t)
{
    /* Dolly ~5s: start (-22,2.4,-130.1) → nudge toward Cheryl, stay on street. */
    const float u = fminf(fmaxf(t / 5.0f, 0.0f), 1.0f);
    alley->cam.pos[0] = SH_SPOT_CAM_X - 4.0f * u;
    alley->cam.pos[1] = SH_SPOT_CAM_Y + 1.5f * u;
    alley->cam.pos[2] = SH_SPOT_CAM_Z + 4.0f * u;
    cam_look_at(&alley->cam, alley->cheryl_pos[0], 1.0f, alley->cheryl_pos[2]);
    alley->cam.fov_deg = SH_VC_FOV_DEG;
    alley->cam_active = true;
}

static void set_alley_cam_behind(ShAlley *alley, float px, float py, float pz)
{
    /*
     * Over-shoulder down the alley (Harry faces +Z).
     * Retail PS1 offsets put the cam along −Z of facing; after axis flip that is
     * behind Harry in 3DS — look past him toward Cheryl.
     */
    alley->cam.pos[0] = px + 0.24f;
    alley->cam.pos[1] = py + 2.24f;
    alley->cam.pos[2] = pz - 5.0f;
    cam_look_at(&alley->cam, px - 0.3f, py + 0.7f, pz + 8.0f);
    alley->cam.fov_deg = SH_VC_FOV_DEG;
    alley->cam_active = true;
}

bool sh_alley_update(ShAlley *alley, ShSavegame *save, float *player_x, float *player_y, float *player_z,
                     float *player_yaw, const ShInputState *input, float dt)
{
    if (!alley || alley->stage == SH_ALLEY_OFF || !save || !input) {
        return false;
    }

    fade_update(alley, dt);
    sh_border_update(&alley->border, SH_Q12(dt));
    msg_tick(alley, dt);
    voice_tick(alley, dt);

    const float px = player_x ? *player_x : 0.0f;
    const float pz = player_z ? *player_z : 0.0f;
    sh_alley_progress_update(alley, save, px, pz, dt);

    switch (alley->stage) {
    case SH_ALLEY_OPENING:
        opening_update(alley, player_x, player_y, player_z, player_yaw, input, dt);
        break;

    case SH_ALLEY_STREET:
        alley->controls_locked = false;
        alley->cam_active = false;
        alley->letterbox = false;
        alley->harry_anim_status = 0;
        if (!sh_save_event_get(save, SH_EV_OPENING)) {
            sh_save_event_set(save, SH_EV_OPENING);
        }
        foot_sfx_tick(alley, px, pz, dt);
        if (in_footsteps_zone(px, pz) && !sh_save_event_get(save, SH_EV_FOOTSTEPS)) {
            alley->stage = SH_ALLEY_FOOTSTEPS;
            alley->step = 0;
            alley->wait = 0.0f;
            alley->controls_locked = true;
            alley->letterbox = true;
            alley->foot_sfx_t = 0.0f;
        }
        break;

    case SH_ALLEY_FOOTSTEPS:
        /* Freeze in place — no auto-walk through buildings. */
        alley->controls_locked = true;
        alley->letterbox = true;
        alley->cam_active = false;
        foot_sfx_tick(alley, px, pz, dt);
        switch (alley->step) {
        case 0:
            fade_to(alley, 0.35f, 1.2f);
            step_next(alley);
            /* fallthrough */
        case 1:
            if (wait_timer(alley, 3.0f, dt)) {
                step_next(alley);
            }
            break;
        case 2:
            set_msg(alley, "Footsteps?", NULL, 1.5f);
            if (wait_timer(alley, 1.5f, dt)) {
                step_next(alley);
            }
            break;
        default:
            sh_save_event_set(save, SH_EV_FOOTSTEPS);
            fade_to(alley, 1.0f, 1.8f);
            alley->stage = SH_ALLEY_FMV;
            alley->step = 0;
            alley->wait = 0.0f;
            break;
        }
        break;

    case SH_ALLEY_FMV:
        /* MAP_EVENTS[5], SysState_Fmv_Update: completion follows playback or retail skip. */
        alley->controls_locked = true;
        alley->letterbox = true;
        alley->cam_active = false;
        alley->cheryl_visible = false;
        if (!alley->movie_finished) {
            if (fade_done(alley)) { alley->movie_requested = true; }
            break;
        }
        if (fade_done(alley)) {
            alley->movie_finished = false;
            sh_save_event_set(save, SH_EV_ALLEY_FMV);
            /* MAP_POINTS-ish: Bachman facing Cheryl (PS1 −26, 127). */
            place_harry(alley, player_x, player_y, player_z, player_yaw, -26.0f, -127.0f, DEG(-135.0f));
            cheryl_place(alley, SH_CHERYL_X, SH_CHERYL_Z, DEG(-90.0f));
            alley->cheryl_wp = 0;
            alley->stage = SH_ALLEY_SPOTTED;
            alley->step = 0;
            alley->wait = 0.0f;
            alley->timer = 0.0f;
            fade_to(alley, 0.0f, 1.5f);
            set_spot_cam(alley, 0.0f);
        }
        break;

    case SH_ALLEY_SPOTTED:
        alley->controls_locked = true;
        alley->letterbox = true;
        alley->timer += dt;
        set_spot_cam(alley, alley->timer);
        /* Cheryl runs west along Bachman only. */
        if (alley->cheryl_wp < 3) {
            const float *wp = k_cheryl_street_wp[alley->cheryl_wp];
            const float spd = alley->step < 3 ? 1.8f : 0.55f;
            if (cheryl_move_to(alley, wp[0], wp[1], spd, dt)) {
                alley->cheryl_wp++;
            }
        }
        switch (alley->step) {
        case 0:
            if (fade_done(alley)) {
                voice_play(alley, SH_VO_CHERYL_SPOT);
                set_msg(alley, "Cheryl? Is that Cheryl!?", NULL, 2.8f);
                step_next(alley);
            }
            break;
        case 1:
            if (wait_timer(alley, 2.8f, dt)) {
                voice_stop(alley);
                step_next(alley);
            }
            break;
        case 2:
            /* Stay planted on Bachman — no lerp through geometry. */
            if (player_yaw) {
                *player_yaw = DEG(-135.0f);
            }
            harry_set_anim(alley, ANIM_ST(2));
            if (wait_timer(alley, 1.2f, dt)) {
                voice_play(alley, SH_VO_WHERE_GOING);
                set_msg(alley, "Where are you going?", NULL, 1.2f);
                step_next(alley);
            }
            break;
        case 3:
            if (wait_timer(alley, 1.2f, dt)) {
                voice_stop(alley);
                voice_play(alley, SH_VO_HEY_WAIT);
                set_msg(alley, "Hey wait... stop!", NULL, 1.8f);
                step_next(alley);
            }
            break;
        case 4:
            if (wait_timer(alley, 1.8f, dt)) {
                voice_stop(alley);
                step_next(alley);
            }
            break;
        default:
            sh_save_event_set(save, SH_EV_SPOTTED);
            alley->cam_active = false;
            alley->letterbox = false;
            alley->controls_locked = false;
            alley->harry_anim_status = 0;
            /* Re-assert street spawn so post-CS tank control is valid. */
            place_harry(alley, player_x, player_y, player_z, player_yaw, -26.0f, -127.0f, DEG(-135.0f));
            alley->stage = SH_ALLEY_CHASE;
            alley->step = 0;
            alley->cheryl_wp = (alley->cheryl_wp < 2) ? alley->cheryl_wp : 2;
            set_msg(alley, NULL, NULL, 0.0f);
            break;
        }
        if (alley->harry_anim_status != 0) {
            alley->harry_anim_time += dt;
        }
        break;

    case SH_ALLEY_CHASE:
        alley->controls_locked = false;
        alley->cam_active = false;
        alley->letterbox = false;
        alley->harry_anim_status = 0;
        if (alley->cheryl_wp < 3) {
            const float *wp = k_cheryl_street_wp[alley->cheryl_wp];
            if (cheryl_move_to(alley, wp[0], wp[1], 3.0f, dt)) {
                alley->cheryl_wp++;
            }
        } else {
            alley->cheryl_visible = true;
            cheryl_move_to(alley, SH_ALLEY_GATE_X, SH_ALLEY_GATE_Z, 2.5f, dt);
        }

        if (!alley->redirected && in_redirect_zone(px, pz)) {
            alley->stage = SH_ALLEY_REDIRECT;
            alley->step = 0;
            alley->wait = 0.0f;
            alley->controls_locked = true;
            break;
        }

        alley->skip_collision = sh_alley_gate_trigger(px, pz);
        if (sh_alley_gate_trigger(px, pz)) {
            alley->stage = SH_ALLEY_INTO;
            alley->step = 0;
            alley->wait = 0.0f;
            sh_save_event_set(save, SH_EV_INTO_ALLEY);
            sh_save_event_set(save, SH_EV_CHASE_GATE);
            alley->controls_locked = true;
            alley->letterbox = true;
            alley->skip_collision = true;
            fade_to(alley, 0.4f, 1.5f);
            cheryl_place(alley, SH_ALLEY_X, SH_ALLEY_GATE_Z + 12.0f, PI_F);
            alley->cheryl_wp = 0;
            /* Past the gate mesh, facing down the alley (+Z). */
            place_harry(alley, player_x, player_y, player_z, player_yaw, SH_ALLEY_X, SH_ALLEY_GATE_Z + 4.0f, PI_F);
        }
        break;

    case SH_ALLEY_REDIRECT:
        alley->controls_locked = true;
        alley->letterbox = true;
        switch (alley->step) {
        case 0:
            set_msg(alley, "I'd better follow Cheryl...", NULL, 2.5f);
            harry_set_anim(alley, ANIM_ST(2));
            step_next(alley);
            /* fallthrough */
        case 1:
            if (player_x && player_z && player_yaw) {
                const float tx = SH_ALLEY_GATE_X + 10.0f;
                const float tz = SH_ALLEY_GATE_Z;
                const float dx = tx - *player_x;
                const float dz = tz - *player_z;
                const float d = sqrtf(dx * dx + dz * dz);
                if (d > 0.4f) {
                    const float t = fminf(1.0f, 2.2f * dt / d);
                    *player_x += dx * t;
                    *player_z += dz * t;
                    *player_yaw = atan2f(-dx, -dz);
                }
            }
            if (wait_timer(alley, 2.5f, dt)) {
                step_next(alley);
            }
            break;
        default:
            alley->redirected = true;
            alley->controls_locked = false;
            alley->letterbox = false;
            alley->harry_anim_status = 0;
            alley->stage = SH_ALLEY_CHASE;
            alley->step = 0;
            break;
        }
        if (alley->harry_anim_status != 0) {
            alley->harry_anim_time += dt;
        }
        break;

    case SH_ALLEY_INTO:
        alley->controls_locked = true;
        alley->letterbox = true;
        alley->skip_collision = true;
        if (player_x && player_z) {
            set_alley_cam_behind(alley, *player_x, player_y ? *player_y : 0.0f, *player_z);
        }
        cheryl_move_to(alley, SH_ALLEY_X, -110.0f, 2.2f, dt);
        switch (alley->step) {
        case 0:
            if (fade_done(alley) && wait_timer(alley, 0.8f, dt)) {
                fade_to(alley, 0.0f, 1.2f);
                step_next(alley);
            }
            break;
        case 1:
            harry_set_anim(alley, ANIM_ST(2));
            if (player_x && player_z && player_yaw) {
                /* Walk a few metres down the alley (+Z). */
                const float tz = SH_ALLEY_GATE_Z + 10.0f;
                if (*player_z < tz) {
                    *player_z += 2.4f * dt;
                    *player_x = SH_ALLEY_X;
                    *player_yaw = PI_F;
                } else if (wait_timer(alley, 1.0f, dt)) {
                    step_next(alley);
                }
            } else if (wait_timer(alley, 1.5f, dt)) {
                step_next(alley);
            }
            break;
        default:
            alley->cam_active = false;
            alley->letterbox = false;
            alley->controls_locked = false;
            alley->skip_collision = false;
            alley->harry_anim_status = 0;
            alley->cheryl_wp = 0;
            place_harry(alley, player_x, player_y, player_z, player_yaw, SH_ALLEY_X, SH_ALLEY_GATE_Z + 8.0f, PI_F);
            alley->stage = SH_ALLEY_ROAM;
            alley->step = 0;
            set_msg(alley, NULL, NULL, 0.0f);
            break;
        }
        if (alley->harry_anim_status != 0) {
            alley->harry_anim_time += dt;
        }
        break;

    case SH_ALLEY_ROAM:
        alley->controls_locked = false;
        alley->cam_active = false;
        alley->letterbox = false;
        if (sh_alley_room_begin(alley, save, px, pz, player_yaw ? *player_yaw : 0.0f, input)) {
            break;
        }
        /* Keep soft pass near the mouth until Harry is clear of the fence. */
        alley->skip_collision = pz < SH_ALLEY_GATE_Z + 12.0f;
        if (alley->cheryl_wp < 4) {
            const float *wp = k_cheryl_alley_wp[alley->cheryl_wp];
            if (cheryl_move_to(alley, wp[0], wp[1], 2.8f, dt)) {
                alley->cheryl_wp++;
            }
        }
        /* MAP_EVENTS[23]: tick, required EventFlag_15, complete EventFlag_17. */
        if (sh_save_event_get(save, 15) && !sh_save_event_get(save, SH_EV_ALLEY_DARK)) {
            alley->stage = SH_ALLEY_DARKER;
            alley->step = 0;
            alley->wait = 0.0f;
            alley->controls_locked = true;
            alley->letterbox = true;
            alley->skip_collision = false;
        } else if (sh_alley_nightmare_trigger(save, px, pz)) {
            alley->stage = SH_ALLEY_NIGHTMARE;
            /* CutsceneAlleyNightmare case 0: WorldMap_ChunkSet. */
            alley->nightmare_world = true;
            alley->nightmare_world_requested = true;
            alley->step = 0;
            alley->wait = 0.0f;
            alley->controls_locked = true;
            alley->letterbox = true;
            fade_to(alley, 1.0f, 3.0f);
        }
        break;

    case SH_ALLEY_DARKER:
        alley->controls_locked = true;
        alley->letterbox = false;
        alley->cam_active = false;
        switch (alley->step) {
        case 0:
            /* MapEven_CutsceneAlleyGetsDarker. */
            alley->torch_environment_time = 0;
            alley->flame_requested = true;
            alley->grey_assets_loaded = false;
            alley->need_grey_assets = true;
            alley->border_step = 0;
            step_next(alley);
            /* fallthrough */
        case 1:
            if (alley->border_step == 0) {
                alley->border.state = SH_BORDER_FADEINSTART;
                alley->border_step++;
            } else if (alley->border.state == SH_BORDER_SHOWN) {
                step_next(alley);
            }
            break;
        case 2:
            /* Free movement is stopped when controls_locked is applied. */
            step_next(alley);
            break;
        case 3:
            step_next(alley);
            /* fallthrough */
        case 4:
            set_msg(alley, "That's strange,\nit's getting darker.", NULL, 0.0f);
            if (sh_input_down(input, SH_BTN_ACTION)) {
                set_msg(alley, NULL, NULL, 0.0f);
                step_next(alley);
            }
            break;
        case 5:
            if (sh_save_event_get(save, 16)) { alley->step = 6; }
            break;
        case 6:
            if (alley->grey_assets_loaded) { step_next(alley); }
            break;
        case 7:
            save->equipped_weapon = SH_ITEM_UNEQUIPPED;
            sh_path_alley_animation_start(&alley->torch_player, 84);
            alley->torch_active = true;
            step_next(alley);
            /* fallthrough */
        case 8:
            if (sh_path_school_animation_done(&alley->torch_player)) { step_next(alley); }
            break;
        case 9:
            sh_env_transition_begin(&alley->environment, 10, 10);
            step_next(alley);
            break;
        case 10:
            alley->torch_environment_time += SH_Q12(dt);
            sh_env_transition_update(&alley->environment, alley->torch_environment_time, 0, SH_Q12(0.5f));
            if (alley->torch_environment_time > SH_Q12(0.5f)) { step_next(alley); }
            break;
        case 11:
            sh_save_event_set(save, SH_EV_ALLEY_DARK);
            set_msg(alley, "......better than nothing,\nI guess.", NULL, 0.0f);
            if (sh_input_down(input, SH_BTN_ACTION)) {
                set_msg(alley, NULL, NULL, 0.0f);
                step_next(alley);
            }
            break;
        default:
            alley->border.state = SH_BORDER_FADEOUTSTART;
            alley->torch_active = false;
            sh_vc_warp(&alley->vc);
            /* MapEven_CutsceneAlleyGetsDarker default: Chara_Spawn Grey Children. */
            spawn_grey_children(alley);
            alley->stage = SH_ALLEY_ROAM;
            alley->step = 0;
            alley->wait = 0.0f;
            alley->cheryl_visible = false;
            alley->controls_locked = false;
            alley->letterbox = false;
            break;
        }
        break;

    case SH_ALLEY_DEAD_END:
        alley->controls_locked = true;
        alley->letterbox = true;
        alley->cam_active = false;
        set_msg(alley, "A dead end?\nWhat the hell!?", NULL, 0.0f);
        if (sh_input_down(input, SH_BTN_ACTION)) {
            alley->stage = SH_ALLEY_ROAM;
            alley->step = 0;
            alley->wait = 0.0f;
            alley->controls_locked = false;
            alley->letterbox = false;
            alley->skip_collision = false;
            set_msg(alley, NULL, NULL, 0.0f);
            sh_snd_stop(SH_SFX_OPENING_AMB);
            sh_snd_play(SH_SFX_NIGHTMARE_AMB, 0, 40);
            sh_snd_play(SH_SFX_ALLEY_TENSION, 0, 50);
        }
        break;

    case SH_ALLEY_FIGHT:
        /* MapEvent_CutsceneAlleyNightmare default enables the aggressive phase. */
        alley->controls_locked = false;
        alley->letterbox = false;
        alley->cam_active = false;
        alley->skip_collision = false;
        grey_update(alley, px, pz, dt);
        if (sh_input_down(input, SH_BTN_ACTION)) {
            for (int i = 0; i < alley->grey_count; i++) {
                ShAlleyGrey *g = &alley->grey[i];
                if (!g->alive) {
                    continue;
                }
                if (dist2_xz(px, pz, g->pos[0], g->pos[2]) < 1.4f * 1.4f) {
                    g->hp -= 20.0f;
                    g->cd = 0.4f;
                    if (g->hp <= 0.0f) {
                        g->alive = false;
                    }
                }
            }
        }
        if (grey_alive_count(alley) == 0 || wait_timer(alley, 45.0f, dt)) {
            alley->stage = SH_ALLEY_BLACKOUT;
            alley->step = 0;
            alley->wait = 0.0f;
            alley->controls_locked = true;
            alley->letterbox = true;
            fade_to(alley, 1.0f, 1.0f);
        }
        break;

    case SH_ALLEY_NIGHTMARE:
        alley->controls_locked = true;
        alley->letterbox = true;
        alley->cam_active = false;
        alley->cheryl_visible = false;
        switch (alley->step) {
        case 0:
            /* Don't stall forever if fade never quite hits target. */
            if (fade_done(alley) || wait_timer(alley, 1.5f, dt)) {
                alley->fade = 1.0f;
                alley->fade_target = 1.0f;
                alley->wait = 0.0f;
                voice_play(alley, SH_VO_WHAT_IS_THIS);
                set_msg(alley, "What is this?", NULL, 1.6f);
                step_next(alley);
            }
            break;
        case 1:
            if (wait_timer(alley, 1.6f, dt)) {
                voice_stop(alley);
                voice_play(alley, SH_VO_WHATS_GOING);
                set_msg(alley, "What's going on here!?", NULL, 2.5f);
                step_next(alley);
            }
            break;
        case 2:
            if (wait_timer(alley, 2.5f, dt)) {
                voice_stop(alley);
                /* Ambush sting — grey children stub. */
                sh_snd_play(SH_SFX_ALLEY_TENSION, 0, 50);
                step_next(alley);
            }
            break;
        case 3:
            if (wait_timer(alley, 1.8f, dt)) {
                step_next(alley);
            }
            break;
        default:
            sh_save_event_set(save, SH_EV_NIGHTMARE);
            alley->stage = SH_ALLEY_FIGHT;
            alley->step = 0;
            alley->wait = 0.0f;
            alley->controls_locked = false;
            alley->letterbox = false;
            fade_to(alley, 0.0f, 3.0f);
            break;
        }
        break;

    case SH_ALLEY_BLACKOUT:
        /* Retail: fight ends → EventFlag_25/26 → LoadOverlay cafe. Hold black. */
        alley->controls_locked = true;
        alley->letterbox = true;
        alley->cam_active = false;
        alley->cheryl_visible = false;
        alley->fade = 1.0f;
        alley->fade_target = 1.0f;
        voice_stop(alley);
        if (wait_timer(alley, 1.8f, dt)) {
            finish_to_cafe(alley, save);
        }
        break;

    case SH_ALLEY_ROOM:
        alley->controls_locked = true;
        alley->cam_active = false;
        if (fade_done(alley)) {
            if (alley->room_complete_flag) { sh_save_event_set(save, alley->room_complete_flag); }
            alley->room_ready = true;
        }
        break;

    case SH_ALLEY_TO_CAFE:
        alley->controls_locked = true;
        alley->fade = 1.0f;
        break;

    default:
        break;
    }

    if (alley->torch_active) {
        if (sh_path_school_animation_done(&alley->torch_player) && !alley->torch_match) {
            const float position[3] = {px, player_y ? *player_y : 0.0f, pz};
            sh_snd_at(1356, position, 0.5f);
            alley->torch_match = true;
        }
        ShAnimPoseTiming pose;
        sh_path_animation_timing(&alley->torch_player, SH_Q12(dt), 0, 0, &pose);
    }
    if (sh_save_event_get(save, SH_EV_ALLEY_DARK)) {
        sh_alley_flicker_update(&alley->flicker, &alley->environment, SH_Q12(dt));
    }
    sh_env_update(&alley->environment, sh_save_flashlight_on(save), dt);
    return alley->controls_locked;
}

bool sh_alley_harry_keyframe(const ShAlley *alley, int *keyframe)
{
    if (!alley || !alley->torch_active) { return false; }
    *keyframe = alley->torch_player.model.keyframe;
    return true;
}

const char *sh_alley_subtitle(const ShAlley *alley)
{
    return alley && alley->subtitle ? alley->subtitle : "";
}

const char *sh_alley_dialogue(const ShAlley *alley)
{
    return alley && alley->dialogue ? alley->dialogue : "";
}

const char *sh_alley_hint(const ShAlley *alley)
{
    (void)alley;
    return "";
}

bool sh_alley_letterbox(const ShAlley *alley)
{
    return alley && alley->letterbox;
}

float sh_alley_fade(const ShAlley *alley)
{
    return alley ? alley->fade : 0.0f;
}

bool sh_alley_camera(const ShAlley *alley, ShCamera *out)
{
    if (!alley || !alley->cam_active || alley->stage == SH_ALLEY_OFF) {
        return false;
    }
    if (out) {
        *out = alley->cam;
    }
    return true;
}

bool sh_alley_gameplay_camera(ShAlley *alley, const float position[3], float yaw, float dt, ShCamera *out)
{
    if (!sh_alley_active(alley)) { return false; }
    if (alley->cam_active) {
        alley->vc_scripted = true;
        return false;
    }
    if (alley->vc_scripted || alley->room_ready) { sh_vc_warp(&alley->vc); }
    alley->vc_scripted = false;
    bool near_enemy = false;
    /* vcSetNearestEnemyDataInVC_WORK: nearest living enemy strictly within 15 m. */
    for (int i = 0; i < alley->grey_count; i++) {
        const ShAlleyGrey *enemy = &alley->grey[i];
        if (alley->stage == SH_ALLEY_FIGHT && enemy->alive && enemy->hp >= 0.0f &&
            dist2_xz(position[0], position[2], enemy->pos[0], enemy->pos[2]) < 15.0f * 15.0f) {
            near_enemy = true;
            break;
        }
    }
    const float ps1_position[3] = {position[0], -position[1], -position[2]};
    sh_vc_update(&alley->vc, ps1_position, -yaw, near_enemy, dt, out);
    return true;
}

bool sh_alley_actor_pose(const ShAlley *alley, int slot, ShActorPose *out)
{
    if (!alley || !out || slot < 0) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (alley->grey_count > 0 && slot < alley->grey_count) {
        const ShAlleyGrey *g = &alley->grey[slot];
        if (!g->alive) {
            return false;
        }
        out->slot = slot;
        out->visible = true;
        out->pos[0] = g->pos[0];
        out->pos[1] = g->pos[1];
        out->pos[2] = g->pos[2];
        out->yaw = g->yaw;
        return true;
    }
    if (slot == 0 && alley->cheryl_visible) {
        out->slot = 0;
        out->visible = true;
        out->pos[0] = alley->cheryl_pos[0];
        out->pos[1] = alley->cheryl_pos[1];
        out->pos[2] = alley->cheryl_pos[2];
        out->yaw = alley->cheryl_yaw;
        return true;
    }
    return false;
}

int sh_alley_actor_keyframe(const ShAlley *alley, int slot, int kf_count)
{
    if (!alley || kf_count <= 0 || slot < 0) {
        return 0;
    }
    float t = alley->cheryl_anim;
    if (alley->grey_count > 0 && slot < alley->grey_count) {
        /* StalkerControl_1 skips animation updates until Nightmare activates it. */
        if (alley->stage != SH_ALLEY_FIGHT && alley->stage != SH_ALLEY_BLACKOUT) {
            return ALLEY_GREY_FROZEN_KEYFRAME < kf_count ? ALLEY_GREY_FROZEN_KEYFRAME : 0;
        }
        t = alley->grey[slot].anim;
    }
    const int span = kf_count < 20 ? kf_count : 20;
    const int kf = (int)(t * 12.0f) % span;
    return kf < 0 ? 0 : kf;
}

bool sh_alley_skip_collision(const ShAlley *alley)
{
    /* Scripted paths may cross a gate; free gameplay must use world collision. */
    return alley && alley->controls_locked && alley->skip_collision;
}

bool sh_alley_needs_grey_assets(const ShAlley *alley)
{
    return alley && alley->need_grey_assets;
}

void sh_alley_clear_grey_assets_flag(ShAlley *alley)
{
    if (alley) {
        alley->need_grey_assets = false;
        alley->grey_assets_loaded = true;
    }
}

bool sh_alley_harry_cutscene_anim(const ShAlley *alley, int *status_out, float *time_out)
{
    if (!alley || alley->harry_anim_status == 0) {
        return false;
    }
    if (alley->stage != SH_ALLEY_OPENING && alley->stage != SH_ALLEY_SPOTTED &&
        alley->stage != SH_ALLEY_REDIRECT && alley->stage != SH_ALLEY_INTO) {
        return false;
    }
    if (status_out) {
        *status_out = alley->harry_anim_status;
    }
    if (time_out) {
        *time_out = alley->harry_anim_time;
    }
    return true;
}

bool sh_alley_take_ground_snap(ShAlley *alley)
{
    if (!alley || !alley->need_ground_snap) {
        return false;
    }
    alley->need_ground_snap = false;
    return true;
}
