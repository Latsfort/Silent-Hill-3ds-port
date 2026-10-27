#include "sh3ds/town.h"

#include "sh3ds/coll.h"
#include "sh3ds/npc_load.h"
#include "sh3ds/controls.h"
#include "sh3ds/fixed.h"
#include "sh3ds/mapdata.h"
#include "sh3ds/papermap.h"
#include "sh3ds/snd.h"
#include "sh3ds/sound.h"

#include "mapdata_map0_s02.h"
#include "mapdata_map2_s00.h"
#include "map_enemy_spawns_map2_s00.h"
#include "map_pickups_map2_s00.h"
#include "map_world_objects_map2_s00.h"

#include <3ds.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI_F ((float)M_PI)

#define SH_DOG_HP 80.0f
#define SH_DOG_SPEED 2.4f
#define SH_DOG_BITE 12.0f
#define SH_DOG_RANGE 1.1f

/* Per-frame PS1 counters below assume the 30 fps game loop. */
#define PS1_FPS 30.0f
/* ScreenFade: Q12(3.0) per second, Q12(3.0) / (queue + 1) while a file is queued. */
#define FADE_SPEED 3.0f
#define FADE_SPEED_LOADING 1.5f

#define EV_MAPMARK_COFE_ARROW 870
#define EV_MAPMARK_COFE_SIGN 872
#define EV_MAPMARK_CHERYL_ARROW_SIGN 874
#define EV_MAPMARK_11 875
#define EV_SCHOOL_CIRCLE 930
#define EV_SKETCH_DONE 147
#define EV_M2S00_DOG_NOTE 148
#define EV_M2S00_DOG_KEY 149
#define EV_M2S00_WATER_CS 34
#define EV_STORY_WATER 193
#define EV_M2S00_STEEL 161
#define EV_M2S00_PAPER_MAP 171
#define EV_HOUSE_DOOR 153
#define EV_SCHOOL_CROSS_A 172 /* func_800EB824(0) */
#define EV_SCHOOL_CROSS_B 173
#define EV_SCHOOL_CROSS_C 174

/* DEFAULT_PICKUP_ITEM_COUNT / HANDGUN_AMMO_PICKUP_* (items.h). Normal difficulty, extraBulletAdjust=0. */
#define PICKUP_COUNT_DEFAULT 1
#define PICKUP_COUNT_HANDGUN 15
#define PICKUP_COUNT_SHOTGUN 6
#define PICKUP_COUNT_RIFLE 6

typedef struct ShTownKey {
    int callback;
    int item_id;
    int flag;
    int marker_required;
    int marker_set;
    const char *message;
} ShTownKey;

#include "town_keys.h"
#include "town_story_data.h"
#define SH_TOWN_STORY_MESSAGES
#include "town_story_data.h"
#undef SH_TOWN_STORY_MESSAGES

typedef struct ShVcRoad ShVcRoad;
#include "vc_roads_map2_s00.h"
#include "vc_roads_map0_s02.h"
#define TOWN_ROAD_COUNT ((int)(sizeof(s_vc_roads_map2_s00) / sizeof(s_vc_roads_map2_s00[0])))
#define TOWN_POINT_COUNT ((int)(sizeof(s_map_points_map2_s00) / sizeof(s_map_points_map2_s00[0])))
#define TOWN_EVENT_COUNT ((int)(sizeof(s_map_events_map2_s00) / sizeof(s_map_events_map2_s00[0])))
#define TOWN_PICKUP_COUNT MAP_PICKUP_COUNT_MAP2_S00
#define M0S02_POINT_COUNT ((int)(sizeof(s_map_points_map0_s02) / sizeof(s_map_points_map0_s02[0])))
#define M0S02_EVENT_COUNT ((int)(sizeof(s_map_events_map0_s02) / sizeof(s_map_events_map0_s02[0])))

/* map_msg_common.h NTSC-U (MapMsgIdx_FirstAidSelect .. ShotgunAmmoSelect). */
/* MAP_MESSAGES map2_s00 — sh_lookup.py msg map2_s00 N */
static const char *map_msg_text(int idx)
{
    switch (idx) {
    case 15:
        return "There is a Steel pipe . Take it?";
    case 18:
        return "To school";
    case 19:
        return "Isn't this Cheryl's sketchbook?";
    case 21:
        return "Sketchbook cover. This picture... it's me.";
    case 23:
        return "Doghouse Levin st.";
    case 24:
        return "Nothing unusual.";
    case 25:
        return "Probably a doghouse, though I'm not sure since there's no dog around.";
    case 26:
        return "Nothing in particular around.";
    case 27:
        return "House key was hidden in the dog house. Take it?";
    case 60:
        return "There is a Shopping district map . Take it?";
    case 78:
        return "This must be that water works Lisa was talking about.";
    default:
        return idx >= 0 && (unsigned)idx < sizeof(s_town_story_messages) / sizeof(s_town_story_messages[0])
                   ? s_town_story_messages[idx] : "";
    }
}

static bool trig_aabb(const ShMapPoint *p, float px, float pz);
static void item_take_begin(ShTown *t, const ShMapCommonPickup *pk);
static void item_take_update(ShTown *t, ShSavegame *save, const ShInputState *input, float dt);
static void map_full(ShTown *t);
static void town_spawn_reset(ShTown *t);
static void begin_load_overlay(ShTown *t, const ShMapEvent *ev, uint8_t source_map);
static void map_point_player_spawn(const ShMapPoint *p, float *x, float *z, float *yaw);
static void town_events_map0_s02(ShTown *t, ShSavegame *save, float px, float pz, float yaw, const ShInputState *input);
static void school_cross_flags(ShSavegame *save, int variant); /* func_800EB824 */

static const char *s_pickup_msgs[] = {
    "There is a First aid kit . Take it?",
    "There is a Health drink . Take it?",
    "There is an Ampoule . Take it?",
    "There are some Handgun bullets . Take it?",
    "There are some Rifle shells . Take it?",
    "There are some Shotgun shells . Take it?",
};

static void fade_to(ShTown *t, float target, float speed)
{
    t->fade_target = target;
    t->fade_speed = speed > 0.05f ? speed : 0.05f;
}

static void fade_update(ShTown *t, float dt)
{
    if (fabsf(t->fade - t->fade_target) < 0.001f) {
        t->fade = t->fade_target;
        return;
    }
    const float step = t->fade_speed * dt;
    if (t->fade < t->fade_target) {
        t->fade = fminf(t->fade + step, t->fade_target);
    } else {
        t->fade = fmaxf(t->fade - step, t->fade_target);
    }
}

static bool fade_done(const ShTown *t)
{
    return fabsf(t->fade - t->fade_target) < 0.01f;
}

static void set_msg(ShTown *t, const char *sub, const char *dlg, float hold)
{
    t->subtitle = sub;
    t->dialogue = dlg;
    t->msg_time = hold;
}

static void msg_tick(ShTown *t, float dt)
{
    if (t->msg_time > 0.0f) {
        t->msg_time -= dt;
        if (t->msg_time <= 0.0f) {
            t->msg_time = 0.0f;
            t->subtitle = NULL;
            t->dialogue = NULL;
        }
    }
}

/* Event_WaitTimer */
static bool wait_timer(ShTown *t, float seconds, float dt)
{
    t->wait += dt;
    if (t->wait >= seconds) {
        t->wait = 0.0f;
        return true;
    }
    return false;
}

static void step_set(ShTown *t, int step)
{
    t->step = step;
    t->wait = 0.0f;
    t->script_ticks = 0;
    t->script_wait_ticks = 0;
}

/* Matches cafe/chase: view looks along (-sin yaw, -cos yaw). */
static void cam_look_at(ShCamera *cam, float tx, float ty, float tz)
{
    const float dx = tx - cam->pos[0];
    const float dy = ty - cam->pos[1];
    const float dz = tz - cam->pos[2];
    const float horiz = sqrtf(dx * dx + dz * dz);
    cam->yaw = atan2f(-dx, -dz);
    cam->pitch = atan2f(dy, horiz > 0.01f ? horiz : 0.01f);
}

/* ExitCafe case 1: Event_CameraPositionSet / Event_CameraLookAtSet (PS1 → 3DS). */
static void exit_cam_setup(ShTown *t)
{
    /* PS1 pos (15.54, -1.5, 55.64) look (19.42, -1.75, 56.6) */
    t->cam.pos[0] = 15.54f;
    t->cam.pos[1] = 1.5f;
    t->cam.pos[2] = -55.64f;
    cam_look_at(&t->cam, 19.42f, 1.75f, -56.6f);
    t->cam.fov_deg = SH_VC_FOV_DEG;
    t->cam_active = true;
}

static bool dogs_near(const ShTown *t, float px, float pz)
{
    /* vcSetNearestEnemyDataInVC_WORK: ENEMY_DIST_MAX 15 m on each axis. */
    for (int i = 0; i < t->dog_count; i++) {
        const ShTownDog *d = &t->dogs[i];
        if (d->alive && fabsf(d->pos[0] - px) < 15.0f && fabsf(d->pos[2] - pz) < 15.0f) {
            return true;
        }
    }
    return false;
}

static void update_gameplay_camera(ShTown *t, float px, float py, float pz, float yaw, float dt)
{
    const float pos[3] = {px, -py, -pz};
    sh_vc_update(&t->vc, pos, -yaw, dogs_near(t, px, pz), dt, &t->cam);
    t->cam_active = true;
}

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

/* Event_CollideFacingCheck — PS1 reach 0.8 m, facing ±30°. Harry in 3DS space. */
static bool trig_facing(const ShMapPoint *p, float px, float pz, float yaw)
{
    const float a = -yaw; /* PS1 yaw */
    const float bx = px - sinf(a) * 0.125f;
    const float bz = -pz - cosf(a) * 0.125f;
    const float dx = p->x - bx;
    const float dz = p->z - bz;
    const float reach = 0.8f;
    if (fabsf(dx) > reach || fabsf(dz) > reach || dx * dx + dz * dz > reach * reach) {
        return false;
    }
    return fabsf(wrap_angle(a - atan2f(dx, dz))) <= (30.0f * PI_F / 180.0f);
}

/* Event_CollideObbFacingCheck — map point param0=Q8 angle, param1=segment length. */
/* Event_CollideObbCheck — door volume without facing (events_main.c). */
static bool trig_obb(const ShMapPoint *p, float px, float pz)
{
    const float ps1_z = -pz;
    const float dx = px - p->x;
    const float dz = ps1_z - p->z;
    const int tp1 = (int)p->param1;
    if ((tp1 << 9) < (int)(fabsf(dx) * 4096.0f)) {
        return false;
    }
    if ((tp1 << 8) * 2 < (int)(fabsf(dz) * 4096.0f)) {
        return false;
    }
    const float ang = -(float)p->param0 * (2.0f * PI_F / 256.0f);
    const float sin_a = sinf(ang);
    const float cos_a = cosf(ang);
    const float along = -dx * sin_a + dz * cos_a;
    if (along > 4.0f) {
        return false;
    }
    const float across = dx * cos_a + dz * sin_a;
    const float across_lim = (float)(tp1 << 8) / 4096.0f;
    return fabsf(across) <= across_lim;
}

static bool trig_obb_facing(const ShMapPoint *p, float px, float pz, float yaw)
{
    const float a = -yaw;
    const float ang = (float)p->param0 * (2.0f * PI_F / 256.0f);
    const float hx = -cosf(ang) * (float)p->param1 / 16.0f;
    const float hz = sinf(ang) * (float)p->param1 / 16.0f;
    const float x0 = px, z0 = -pz;
    const float rx = sinf(a) * 0.5f, rz = cosf(a) * 0.5f;
    const float s0x = p->x - hx - x0, s0z = p->z - hz - z0;
    const float sdx = 2.0f * hx, sdz = 2.0f * hz;
    const float den = rx * sdz - rz * sdx;
    if (fabsf(den) < 1e-6f) {
        return false;
    }
    const float t = (s0x * sdz - s0z * sdx) / den;
    const float u = (s0x * rz - s0z * rx) / den;
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}

static bool trig_match(const ShMapPoint *p, int trigger_type, float px, float pz, float yaw)
{
    switch (trigger_type) {
    case SH_TRIG_TOUCH_AABB:
        return trig_aabb(p, px, pz);
    case SH_TRIG_TOUCH_FACING:
        return trig_facing(p, px, pz, yaw);
    case SH_TRIG_TOUCH_OBB:
        return trig_obb(p, px, pz);
    case SH_TRIG_TOUCH_OBB_FACING:
        if (trig_obb_facing(p, px, pz, yaw)) {
            return true;
        }
        if (trig_facing(p, px, pz, yaw)) {
            return true;
        }
        return trig_obb(p, px, pz);
    default:
        return false;
    }
}

static void script_begin(ShTown *t, ShTownScript script, int done_flag)
{
    t->stage = SH_TOWN_SCRIPT;
    t->script = script;
    t->script_done_flag = done_flag;
    t->step = 0;
    t->wait = 0.0f;
    t->controls_locked = true;
    t->script_use_cam = false;
    t->pickup_active = false;
    t->prompt_open = false;
    t->skip_fading = false;
    t->box_mode = 0;
    t->mark_alpha = 0.0f;
    t->harry_state = 0;
    t->harry_time = 0.0f;
    t->script_substep = 0;
    t->script_ticks = 0;
    t->script_wait_ticks = 0;
    set_msg(t, NULL, NULL, 0.0f);
}

static void script_end(ShTown *t, ShSavegame *save)
{
    if (t->script_done_flag > 0) {
        sh_save_event_set(save, (unsigned)t->script_done_flag);
    }
    t->stage = SH_TOWN_EXPLORE;
    t->script = SH_TOWN_SCR_NONE;
    t->map_open = false;
    t->letterbox = false;
    t->script_use_cam = false;
    t->controls_locked = false;
    t->box_mode = 0;
    t->harry_state = 0;
    t->pickup_active = false;
    t->prompt_open = false;
    set_msg(t, NULL, NULL, 0.0f);
    sh_vc_warp(&t->vc);
    fade_to(t, 0.0f, FADE_SPEED);
    t->ammo = sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS);
}

static void map_zoom_school(ShTown *t)
{
    const float d = t->map_d < 0.0f ? 0.0f : (t->map_d > 1.0f ? 1.0f : t->map_d);
    t->map_scroll_x = 116.0f - 116.0f * d;
    t->map_scroll_y = 0.0f;
    t->map_scale = 0.5f * d + 0.5f;
}

#include "town_story.inc"

/* MapEvent_* scripted sequences (map2_s00.c). */
static void script_update(ShTown *t, ShSavegame *save, float *px, float *py, float *pz, float *yaw,
                          const ShInputState *input, float dt)
{
    t->controls_locked = true;
    switch (t->script) {
    case SH_TOWN_SCR_SCHOOL: {
        const bool descent = t->school.overlay == SH_MAP_IDX_MAP1_S02 && t->school.callback == 19;
        const bool phone = t->school.overlay == SH_MAP_IDX_MAP1_S02 && (t->school.callback == 14 || t->school.callback == 5);
        if ((descent || phone) && !t->step) {
            t->school.card_player.position = (ShPathPoint){SH_Q12(*px), -SH_Q12(*py), -SH_Q12(*pz)};
            if (phone) { t->school.card_player.rotation.vy = (int16_t)(-*yaw * (4096.0f / (2.0f * PI_F))); }
        }
        const bool ladder = t->school.overlay == SH_MAP_IDX_MAP1_S00 &&
                            (t->school.callback == 15 || t->school.callback == 16);
        if (ladder && !t->step) {
            t->school.ladder_player.position.vx = SH_Q12(*px);
            t->school.ladder_player.position.vy = SH_Q12(-*py);
            t->school.ladder_player.position.vz = SH_Q12(-*pz);
            t->school.ladder_player.rotation.vy = (int16_t)(-*yaw * (4096.0f / (2.0f * PI_F)));
        }
        sh_school_update(t, save, input, dt);
        sh_school_take_player_warp(t, px, pz, yaw);
        if (descent) { *py = -SH_Q12_TO_FLOAT(t->school.card_player.position.vy); }
        if (ladder) {
            *px = t->school.ladder_player.position.vx / 4096.0f;
            *py = -t->school.ladder_player.position.vy / 4096.0f;
            *pz = -t->school.ladder_player.position.vz / 4096.0f;
            *yaw = -t->school.ladder_player.rotation.vy * (2.0f * PI_F / 4096.0f);
        }
        break;
    }
    case SH_TOWN_SCR_WOODMAN_KEY:
        woodman_update(t, save, input, dt);
        break;
    case SH_TOWN_SCR_ECLIPSE_LOCKS:
        eclipse_locks_update(t, save, input, dt);
        break;
    case SH_TOWN_SCR_ECLIPSE_ENTER:
        eclipse_enter_update(t, save, px, pz, yaw, input, dt);
        break;
    case SH_TOWN_SCR_READ_MSG:
        if (t->step == 0) {
            set_msg(t, map_msg_text(t->read_msg_idx), NULL, 0.0f);
            step_set(t, 1);
        } else if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
            script_end(t, save);
        }
        break;

    case SH_TOWN_SCR_DOG_NOTE: /* MapEvent_DoghouseNote */
        switch (t->step) {
        case 0:
            step_set(t, 3);
            fade_to(t, 1.0f, FADE_SPEED);
            break;
        case 3:
            if (fade_done(t)) {
                fade_to(t, 0.0f, FADE_SPEED);
                step_set(t, 4);
            }
            break;
        case 4:
            if (fade_done(t)) {
                set_msg(t, map_msg_text(23), NULL, 0.0f);
                step_set(t, 6);
            }
            break;
        case 6:
            if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
                set_msg(t, NULL, NULL, 0.0f);
                fade_to(t, 1.0f, FADE_SPEED);
                step_set(t, 7);
            }
            break;
        case 7:
            if (fade_done(t)) {
                fade_to(t, 0.0f, FADE_SPEED);
                step_set(t, 8);
            }
            break;
        case 8:
            if (fade_done(t)) {
                sh_save_item_add(save, SH_ITEM_NOTE_DOGHOUSE, 1);
                script_end(t, save);
            }
            break;
        default:
            script_end(t, save);
            break;
        }
        break;

    case SH_TOWN_SCR_DOG_KEY: /* MapEvent_DoghouseKeyTake */
        switch (t->step) {
        case 0:
            step_set(t, 3);
            fade_to(t, 1.0f, FADE_SPEED);
            break;
        case 3:
            if (fade_done(t)) {
                fade_to(t, 0.0f, FADE_SPEED);
                step_set(t, 4);
            }
            break;
        case 4:
            if (fade_done(t)) {
                step_set(t, 7);
            }
            break;
        case 7:
            if (!t->prompt_open) {
                set_msg(t, map_msg_text(27), NULL, 0.0f);
                t->prompt_open = true;
            }
            if (sh_input_down(input, SH_BTN_ACTION)) {
                sh_save_item_add(save, SH_ITEM_HOUSE_KEY, 1);
                sh_save_item_remove_one(save, SH_ITEM_NOTE_DOGHOUSE);
                set_msg(t, NULL, NULL, 0.0f);
                t->prompt_open = false;
                fade_to(t, 1.0f, FADE_SPEED);
                step_set(t, 10);
            } else if (sh_input_down(input, SH_BTN_CANCEL)) {
                set_msg(t, NULL, NULL, 0.0f);
                t->prompt_open = false;
                fade_to(t, 1.0f, FADE_SPEED);
                step_set(t, 10);
            }
            break;
        case 10:
            if (fade_done(t)) {
                fade_to(t, 0.0f, FADE_SPEED);
                step_set(t, 11);
            }
            break;
        case 11:
            if (fade_done(t)) {
                script_end(t, save);
            }
            break;
        default:
            script_end(t, save);
            break;
        }
        break;

    case SH_TOWN_SCR_STEEL_PIPE: /* MapEvent_SteelPipeTake */
        if (t->step == 0) {
            t->pickup_item_id = SH_ITEM_STEEL_PIPE;
            t->pickup_count = PICKUP_COUNT_DEFAULT;
            t->pickup_flag = EV_M2S00_STEEL;
            t->pickup_msg = map_msg_text(15);
            t->pickup_active = true;
            t->pickup_scale = 0.0f;
            t->pickup_spin = 0.0f;
            step_set(t, 1);
        } else {
            item_take_update(t, save, input, dt);
            if (!t->pickup_active) {
                script_end(t, save);
            }
        }
        break;

    case SH_TOWN_SCR_PAPER_MAP: /* MapEvent_PaperMapTake — Event_PaperMapTake(2, 171, 60) */
        switch (t->step) {
        case 0:
            fade_to(t, 1.0f, FADE_SPEED);
            step_set(t, 1);
            break;
        case 1:
            if (fade_done(t)) {
                t->map_open = true;
                map_full(t);
                fade_to(t, 0.0f, FADE_SPEED);
                step_set(t, 2);
            }
            break;
        case 2:
            if (fade_done(t)) {
                set_msg(t, map_msg_text(60), NULL, 0.0f);
                t->prompt_open = true;
                step_set(t, 3);
            }
            break;
        case 3:
            if (sh_input_down(input, SH_BTN_ACTION)) {
                save->has_map = true;
                set_msg(t, NULL, NULL, 0.0f);
                t->prompt_open = false;
                fade_to(t, 1.0f, FADE_SPEED);
                step_set(t, 4);
            } else if (sh_input_down(input, SH_BTN_CANCEL)) {
                set_msg(t, NULL, NULL, 0.0f);
                t->prompt_open = false;
                fade_to(t, 1.0f, FADE_SPEED);
                step_set(t, 5);
            }
            break;
        case 4:
            if (fade_done(t)) {
                script_end(t, save);
            }
            break;
        case 5:
            if (fade_done(t)) {
                t->map_open = false;
                t->script_done_flag = 0;
                script_end(t, save);
            }
            break;
        default:
            script_end(t, save);
            break;
        }
        break;

    case SH_TOWN_SCR_SKETCH_COVER: /* func_800E9470 */
        switch (t->step) {
        case 0: /* QueueRead SKETCHBK.TIM; anims 59/60 not ported */
        case 1:
        case 2:
            fade_to(t, 1.0f, FADE_SPEED);
            step_set(t, 3);
            break;
        case 3:
            if (fade_done(t)) {
                step_set(t, 5);
            }
            break;
        case 5:
            fade_to(t, 0.0f, FADE_SPEED);
            if (fade_done(t)) {
                step_set(t, 6);
            }
            break;
        case 6:
            set_msg(t, map_msg_text(21), NULL, 0.0f);
            if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
                set_msg(t, NULL, NULL, 0.0f);
                fade_to(t, 1.0f, FADE_SPEED);
                step_set(t, 7);
            }
            break;
        case 7:
            if (fade_done(t)) {
                fade_to(t, 0.0f, FADE_SPEED);
                step_set(t, 8);
            }
            break;
        default:
            script_end(t, save);
            break;
        }
        break;

    case SH_TOWN_SCR_SKETCHBOOK: /* MapEvent_CherylsSketchbook */
        switch (t->step) {
        case 0:
            sh_save_event_set(save, EV_MAPMARK_11);
            step_set(t, 3);
            fade_to(t, 1.0f, FADE_SPEED);
            break;
        case 3:
            if (fade_done(t)) {
                step_set(t, 5);
            }
            break;
        case 5: /* BgTextureCmd_Draw TOSCHOOL.TIM + fade in */
            fade_to(t, 0.0f, FADE_SPEED);
            if (fade_done(t)) {
                step_set(t, 6);
            }
            break;
        case 6:
            set_msg(t, map_msg_text(18), NULL, 0.0f);
            if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
                set_msg(t, NULL, NULL, 0.0f);
                step_set(t, 7);
            }
            break;
        case 7:
            set_msg(t, map_msg_text(19), NULL, 0.0f);
            if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
                set_msg(t, NULL, NULL, 0.0f);
                fade_to(t, 1.0f, FADE_SPEED);
                step_set(t, 9);
            }
            break;
        case 9:
            if (fade_done(t)) {
                sh_save_event_set(save, EV_SKETCH_DONE);
                t->map_open = true;
                map_full(t);
                t->map_d = 0.0f;
                fade_to(t, 0.0f, FADE_SPEED);
                step_set(t, 11);
            }
            break;
        case 11:
            if (fade_done(t)) {
                step_set(t, 12);
            }
            break;
        case 12:
            t->box_mode = 1;
            t->map_d = fminf(t->map_d + (32.0f / 4096.0f) * PS1_FPS * dt, 1.0f);
            map_full(t);
            if (t->map_d >= 1.0f) {
                step_set(t, 13);
            }
            break;
        case 13:
            t->box_mode = 2;
            t->map_d = fmaxf(t->map_d - (64.0f / 4096.0f) * PS1_FPS * dt, 0.0f);
            map_zoom_school(t);
            if (t->map_d <= 0.0f) {
                t->box_mode = 0;
                step_set(t, 14);
            }
            break;
        case 14:
            map_zoom_school(t);
            if (wait_timer(t, 1.0f, dt)) {
                step_set(t, 15);
            }
            break;
        case 15:
            map_zoom_school(t);
            t->mark_alpha += PS1_FPS * dt;
            if (t->mark_alpha >= 128.0f) {
                t->mark_alpha = 128.0f;
                if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL) ||
                    wait_timer(t, 1.5f, dt)) {
                    fade_to(t, 1.0f, FADE_SPEED);
                    step_set(t, 16);
                }
            }
            break;
        case 16:
            map_zoom_school(t);
            if (fade_done(t)) {
                sh_save_event_set(save, EV_SCHOOL_CIRCLE);
                t->map_open = false;
                step_set(t, 18);
            }
            break;
        case 18:
            fade_to(t, 0.0f, FADE_SPEED);
            if (fade_done(t)) {
                sh_save_item_add(save, SH_ITEM_NOTE_SCHOOL, 1);
                script_end(t, save);
            }
            break;
        default:
            script_end(t, save);
            break;
        }
        break;

    case SH_TOWN_SCR_WATER_WORKS: /* MapEvent_AtWaterWorks */
        if (t->step == 0) {
            if (px) {
                *px = -255.78f;
            }
            if (pz) {
                *pz = 107.46f;
            }
            if (yaw) {
                *yaw = 45.0f * (PI_F / 180.0f);
            }
            t->map_open = true;
            map_zoom_school(t);
            t->map_scroll_y = 120.0f;
            t->map_scale = 0.5f;
            t->letterbox = true;
            t->script_use_cam = true;
            t->cam.pos[0] = -254.15f;
            t->cam.pos[1] = 1.95f;
            t->cam.pos[2] = 109.3f;
            cam_look_at(&t->cam, -257.04f, 1.94f, 106.54f);
            t->cam.fov_deg = SH_VC_FOV_DEG;
            t->cam_active = true;
            fade_to(t, 0.0f, 1.0f);
            step_set(t, 1);
        } else if (t->step == 1) {
            if (fade_done(t)) {
                step_set(t, 2);
            }
        } else if (t->step == 2) {
            if (wait_timer(t, 3.5f, dt) ||
                sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
                fade_to(t, 1.0f, FADE_SPEED);
                step_set(t, 3);
            }
        } else if (t->step == 3) {
            if (fade_done(t)) {
                t->map_open = false;
                set_msg(t, map_msg_text(78), NULL, 0.0f);
                step_set(t, 6);
            }
        } else if (t->step == 6) {
            if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
                set_msg(t, NULL, NULL, 0.0f);
                script_end(t, save);
            }
        } else {
            script_end(t, save);
        }
        break;

    default:
        script_end(t, save);
        break;
    }
}

/* TriggerType_TouchAabb: |dx| <= param0*0.25, |dz| <= param1*0.25 (events_main.c). */
static bool trig_aabb(const ShMapPoint *p, float px, float pz)
{
    const float rx = (float)p->param0 * 0.25f;
    const float rz = (float)p->param1 * 0.25f;
    return fabsf(px - p->x) <= rx && fabsf((-pz) - p->z) <= rz;
}

static const ShMapCommonPickup *pickup_for_point(unsigned point_idx)
{
    for (int i = 0; i < TOWN_PICKUP_COUNT; i++) {
        if (s_map_pickups_map2_s00[i].point_idx == point_idx) {
            return &s_map_pickups_map2_s00[i];
        }
    }
    return NULL;
}

static void common_pickup_resolve(uint8_t pickup_type, int *item_id, int *count, const char **msg)
{
    switch (pickup_type) {
    case SH_PICKUP_FIRST_AID:
        *item_id = SH_ITEM_FIRST_AID;
        *count = PICKUP_COUNT_DEFAULT;
        *msg = s_pickup_msgs[0];
        break;
    case SH_PICKUP_HEALTH_DRINK:
        *item_id = SH_ITEM_HEALTH_DRINK;
        *count = PICKUP_COUNT_DEFAULT;
        *msg = s_pickup_msgs[1];
        break;
    case SH_PICKUP_AMPOULE:
        *item_id = SH_ITEM_AMPOULE;
        *count = PICKUP_COUNT_DEFAULT;
        *msg = s_pickup_msgs[2];
        break;
    case SH_PICKUP_HANDGUN_BULLETS:
        *item_id = SH_ITEM_HANDGUN_BULLETS;
        *count = PICKUP_COUNT_HANDGUN;
        *msg = s_pickup_msgs[3];
        break;
    case SH_PICKUP_RIFLE_SHELLS:
        *item_id = SH_ITEM_RIFLE_SHELLS;
        *count = PICKUP_COUNT_RIFLE;
        *msg = s_pickup_msgs[4];
        break;
    case SH_PICKUP_SHOTGUN_SHELLS:
        *item_id = SH_ITEM_SHOTGUN_SHELLS;
        *count = PICKUP_COUNT_SHOTGUN;
        *msg = s_pickup_msgs[5];
        break;
    default:
        *item_id = 0;
        *count = 0;
        *msg = "";
        break;
    }
}

/* Event_ItemTake: freeze, animate, Yes/No. Flag set while prompt is shown. */
static void item_take_begin(ShTown *t, const ShMapCommonPickup *pk)
{
    int id = 0, count = 0;
    const char *msg = "";
    common_pickup_resolve(pk->pickup_type, &id, &count, &msg);
    if (id == 0) {
        return;
    }
    t->controls_locked = true;
    t->aiming = false;
    t->pickup_active = true;
    t->prompt_open = false;
    t->pickup_scale = 0.0f;
    t->pickup_spin = 0.0f;
    t->pickup_item_id = id;
    t->pickup_count = count;
    t->pickup_flag = pk->event_flag;
    t->pickup_msg = msg;
    t->subtitle = NULL;
    t->dialogue = NULL;
}

/* func_800EB824 — school district map crossing markers (events 196–198). */
static void school_cross_flags(ShSavegame *save, int variant)
{
    if (!save) {
        return;
    }
    if (variant == 0) {
        sh_save_event_set(save, EV_SCHOOL_CROSS_A);
        sh_save_event_clear(save, EV_SCHOOL_CROSS_B);
        sh_save_event_clear(save, EV_SCHOOL_CROSS_C);
    } else if (variant == 1) {
        sh_save_event_clear(save, EV_SCHOOL_CROSS_A);
        sh_save_event_set(save, EV_SCHOOL_CROSS_B);
        sh_save_event_clear(save, EV_SCHOOL_CROSS_C);
    } else {
        sh_save_event_clear(save, EV_SCHOOL_CROSS_A);
        sh_save_event_clear(save, EV_SCHOOL_CROSS_B);
        sh_save_event_set(save, EV_SCHOOL_CROSS_C);
    }
}

/* func_800EE518: spawn table variant from event flags. */
static int town_spawn_variant(ShSavegame *save)
{
    int ret = 0;
    if (!sh_save_event_get(save, 172)) {
        if (sh_save_event_get(save, 173)) {
            ret = 1;
        } else if (sh_save_event_get(save, 174)) {
            ret = 2;
        } else {
            sh_save_event_set(save, 172);
        }
    }
    if (sh_save_event_get(save, 131) && !sh_save_event_get(save, 181) && ret == 1) {
        ret = 2;
    }
    return ret;
}

static void town_spawn_reset(ShTown *t)
{
    for (int i = 0; i < SH_TOWN_MAX_DOGS; i++) {
        t->dogs[i].alive = false;
        t->dogs[i].state = 0;
        t->dogs[i].spawn = -1;
        t->dogs[i].gfx_slot = 0;
        t->dogs[i].air = false;
    }
    t->dog_count = SH_TOWN_MAX_DOGS;
    t->spawn_used = 0;
    t->spawn_in_game = false;
    /* spawn_killed restored from save->map_enemy_slots in town_begin_explore_common */
}

/* Game_NpcRoomInitSpawn over D_800F1CAC: slots 0..15 Air Screamer, 16..31 Groaner. */
static void town_spawn_npcs(ShTown *t, ShSavegame *save, float px, float pz, bool in_game)
{
    const int v = town_spawn_variant(save);
    for (int k = 0; k < SH_TOWN_MAX_DOGS; k++) {
        ShTownDog *d = &t->dogs[k];
        if (d->spawn < 0) {
            continue;
        }
        /* Game_NpcUpdate: unload past 40 m alive, 24 m dead (exterior). */
        const float lim = d->alive ? 40.0f : 24.0f;
        if ((d->pos[0] - px) * (d->pos[0] - px) + (d->pos[2] - pz) * (d->pos[2] - pz) > lim * lim) {
            t->spawn_used &= ~(1u << d->spawn);
            d->alive = false;
            d->state = 0;
            d->spawn = -1;
        }
    }
    for (int i = 0; i < 32; i++) {
        const int grp = i / 16;
        const int sub = i % 16;
        const ShMapEnemySpawn *s = &s_map_enemy_spawns_map2_s00[v][grp][sub];
        if (s->flags == 0 || s->diff_min > 0 || (t->spawn_used | t->spawn_killed) & (1u << i)) {
            continue;
        }
        const float sx = s->x, sz3 = -s->z;
        const float dx = sx - px, dz = sz3 - pz;
        const float d2 = dx * dx + dz * dz;
        if (d2 > 22.0f * 22.0f || (in_game && d2 <= 20.0f * 20.0f)) {
            continue;
        }
        ShTownDog *d = NULL;
        for (int k = 0; k < SH_TOWN_MAX_DOGS && !d; k++) {
            if (t->dogs[k].spawn < 0) {
                d = &t->dogs[k];
            }
        }
        if (!d) {
            return;
        }
        const bool air = grp == 0;
        int gfx = -1;
        for (int slot = 0; slot < SH_GFX_MAX_ACTORS; slot++) {
            bool used = false;
            for (int k = 0; k < t->dog_count; k++) {
                if (t->dogs[k].alive && t->dogs[k].gfx_slot == slot) {
                    used = true;
                }
            }
            if (!used) {
                gfx = slot;
                break;
            }
        }
        if (gfx < 0) {
            return;
        }
        sh_npc_load_slot(gfx, air);
        d->alive = true;
        d->spawn = i;
        d->air = air;
        d->gfx_slot = gfx;
        d->pos[0] = sx;
        d->pos[2] = sz3;
        d->pos[1] = air ? 3.5f : 0.0f;
        d->yaw = -s->yaw_deg * (PI_F / 180.0f);
        d->hp = SH_DOG_HP;
        d->anim = 0.0f;
        d->cd = 1.0f;
        d->state = 0;
        t->spawn_used |= 1u << i;
    }
}

void sh_town_snap_enemy_heights(ShTown *t, const ShCollWorld *world)
{
    if (!t || !world) {
        return;
    }
    for (int i = 0; i < t->dog_count; i++) {
        ShTownDog *d = &t->dogs[i];
        if (!d->alive) {
            continue;
        }
        ShCollHit hit;
        if (!sh_coll_ground_near(world, d->pos[0], d->pos[2], d->pos[1], &hit) || !hit.valid) {
            continue;
        }
        if (d->air) {
            d->pos[1] = hit.height + 3.5f + sinf(d->anim * 2.0f) * 0.15f;
        } else {
            d->pos[1] = hit.height;
        }
    }
}

/* Chara_PositionSet (player_pos_update.c) */
static void map_point_player_spawn(const ShMapPoint *p, float *x, float *z, float *yaw)
{
    if (!p || !x || !z || !yaw) {
        return;
    }
    const float heading = (float)p->param0 * (2.0f * PI_F / 256.0f);
    float x_ps1 = p->x;
    float z_ps1 = p->z;
    if (p->param1 >= 2) {
        x_ps1 += sinf(heading) * 0.4f;
        z_ps1 += cosf(heading) * 0.4f;
    }
    *x = x_ps1;
    *z = -z_ps1;
    *yaw = -heading;
}

/* SysState_LoadOverlay / SysState_LoadRoom: g_MapPoint = mapPoints[eventParam] of the CURRENT overlay
 * (read before GameBoot_MapLoad), then AreaLoad_UpdatePlayerPosition -> Chara_PositionSet. */
static void begin_load_overlay(ShTown *t, const ShMapEvent *ev, uint8_t source_map)
{
    t->stage = SH_TOWN_LEAVING;
    t->step = 0;
    t->overlay_map_idx = ev->map_idx;
    t->overlay_spawn_pt = ev->event_param;
    t->overlay_sys_state = ev->sys_state;
    t->overlay_complete_flag = ev->complete_flag;
    t->overlay_transition_flags = ev->transition_flags;
    t->leave_overlay = false;
    t->controls_locked = true;
    t->map_open = false;
    t->aiming = false;

    const ShMapPoint *sp = NULL;
    if (sh_school_map_is_school(source_map)) {
        int count;
        const ShMapPoint *points = sh_school_map_points(source_map, &count);
        if (ev->event_param < count) {
            sp = &points[ev->event_param];
        }
    } else if (source_map == SH_MAP_IDX_MAP0_S02) {
        if (ev->event_param < M0S02_POINT_COUNT) {
            sp = &s_map_points_map0_s02[ev->event_param];
        }
    } else if (ev->event_param < TOWN_POINT_COUNT) {
        sp = &s_map_points_map2_s00[ev->event_param];
    }
    t->overlay_room_idx = 0;
    if (sp) {
        map_point_player_spawn(sp, &t->overlay_x, &t->overlay_z, &t->overlay_yaw);
        if (ev->sys_state == SH_SYS_LOAD_ROOM && sp->field_4_5 > 0) {
            t->overlay_room_idx = (int8_t)(sp->field_4_5 - 1);
        }
    } else {
        t->overlay_x = SH_TOWN_SPAWN_X;
        t->overlay_z = SH_TOWN_SPAWN_Z;
        t->overlay_yaw = SH_TOWN_SPAWN_YAW;
    }
    fade_to(t, 1.0f, FADE_SPEED_LOADING);
}

static void item_take_finish(ShTown *t, ShSavegame *save, bool take)
{
    if (take) {
        sh_save_item_add(save, (uint8_t)t->pickup_item_id, (uint8_t)t->pickup_count);
        /* flag already set at SelectionDialog */
    } else if (t->pickup_flag > 0) {
        sh_save_event_clear(save, (unsigned)t->pickup_flag);
    }
    t->pickup_active = false;
    t->prompt_open = false;
    t->controls_locked = false;
    t->pickup_msg = NULL;
    t->subtitle = NULL;
    t->ammo = sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS);
}

static bool key_take_update(ShTown *t, ShSavegame *save, const ShInputState *input, float dt)
{
    const ShTownKey *key = NULL;
    for (unsigned i = 0; i < sizeof(s_town_keys) / sizeof(s_town_keys[0]); i++) {
        if (t->pickup_flag == s_town_keys[i].flag) {
            key = &s_town_keys[i];
            break;
        }
    }
    if (!key) {
        return false;
    }

    /* Event_ItemTake sysStateSteps[1]; models are preloaded by load_pickup_items. */
    switch (t->step) {
    case 0:
        t->step = 1;
        /* fall through */
    case 1:
        t->step = 2;
        break;
    case 2:
        /* Gfx_PickupItemAnimate: scale is doubled, increasing by raw dt * 2. */
        if (t->pickup_scale >= 1.0f) {
            t->prompt_open = true;
            t->subtitle = key->message;
            t->pickup_spin = wrap_angle(t->pickup_spin + PI_F * dt);
            if (sh_input_down(input, SH_BTN_ACTION)) {
                t->step = 3;
            } else if (sh_input_down(input, SH_BTN_CANCEL)) {
                t->step = -1;
            }
        } else {
            t->pickup_scale = fminf(1.0f, t->pickup_scale + dt * 4.0f);
        }
        sh_save_event_set(save, (unsigned)key->flag);
        break;
    case 3:
        item_take_finish(t, save, true);
        break;
    default:
        item_take_finish(t, save, false);
        break;
    }
    /* func_800E9A0C / func_800E9CB4, after Event_ItemTake on each invocation. */
    if (sh_save_event_get(save, (unsigned)key->flag) &&
        sh_save_event_get(save, (unsigned)key->marker_required)) {
        sh_save_event_set(save, (unsigned)key->marker_set);
    }
    return true;
}

static void item_take_update(ShTown *t, ShSavegame *save, const ShInputState *input, float dt)
{
    if (key_take_update(t, save, input, dt)) {
        return;
    }
    /* Gfx_PickupItemAnimate: grow ~0.25 s, then spin + Yes/No. */
    if (t->pickup_scale < 1.0f) {
        t->pickup_scale += dt * 4.0f;
        if (t->pickup_scale >= 1.0f) {
            t->pickup_scale = 1.0f;
            t->prompt_open = true;
            sh_save_event_set(save, (unsigned)t->pickup_flag);
            t->subtitle = t->pickup_msg;
        }
        return;
    }
    t->pickup_spin = wrap_angle(t->pickup_spin + PI_F * dt);
    t->subtitle = t->pickup_msg;
    if (sh_input_down(input, SH_BTN_ACTION)) {
        item_take_finish(t, save, true);
    } else if (sh_input_down(input, SH_BTN_CANCEL)) {
        item_take_finish(t, save, false);
    }
}

/* map0_s02 MAP_EVENTS. */
static void town_events_map0_s02(ShTown *t, ShSavegame *save, float px, float pz, float yaw,
                                 const ShInputState *input)
{
    const bool action = sh_input_down(input, SH_BTN_ACTION);

    for (int i = 0; i < M0S02_EVENT_COUNT; i++) {
        const ShMapEvent *ev = &s_map_events_map0_s02[i];
        if (ev->complete_flag > 0 && sh_save_event_get(save, (unsigned)ev->complete_flag)) {
            continue;
        }
        if (ev->required_flag > 0 && !sh_save_event_get(save, (unsigned)ev->required_flag)) {
            continue;
        }
        if (ev->point_idx >= M0S02_POINT_COUNT) {
            continue;
        }
        const ShMapPoint *pt = &s_map_points_map0_s02[ev->point_idx];

        if (ev->sys_state == SH_SYS_LOAD_OVERLAY || ev->sys_state == SH_SYS_LOAD_ROOM) {
            if (!action || ev->activation_type != SH_ACT_BUTTON) {
                continue;
            }
            if (!trig_match(pt, ev->trigger_type, px, pz, yaw)) {
                continue;
            }
            begin_load_overlay(t, ev, SH_MAP_IDX_MAP0_S02);
            return;
        }

        if (ev->sys_state == SH_SYS_READ_MESSAGE) {
            if (!action || ev->activation_type != SH_ACT_BUTTON) {
                continue;
            }
            if (!trig_match(pt, ev->trigger_type, px, pz, yaw)) {
                continue;
            }
            t->read_msg_idx = ev->event_param;
            script_begin(t, SH_TOWN_SCR_READ_MSG, 0);
            return;
        }
    }
}

static const ShMapEvent *school_item_event(const ShSavegame *save, float x, float z, float yaw, uint8_t item,
                                          uint8_t *items, int max, int *matched)
{
    int event_count, point_count;
    const ShMapEvent *events = sh_school_map_events(save->map_idx, &event_count);
    const ShMapPoint *points = sh_school_map_points(save->map_idx, &point_count);
    *matched = 0;
    for (int i = 0; i < event_count; i++) {
        const ShMapEvent *event = &events[i];
        if (event->activation_type != SH_ACT_ITEM || !sh_school_event_enabled(event, save) ||
            event->point_idx >= point_count ||
            !trig_match(&points[event->point_idx], event->trigger_type, x, z, yaw)) {
            continue;
        }
        if (item && item == event->required_item) {
            return event;
        }
        if (items && *matched < max) {
            items[(*matched)++] = event->required_item;
        }
    }
    return NULL;
}

int sh_town_item_triggers(const ShTown *town, const ShSavegame *save, float x, float z, float yaw,
                          uint8_t *items, int max)
{
    int matched = 0;
    if (town->stage == SH_TOWN_EXPLORE && !town->pickup_active &&
        sh_school_map_is_school(save->map_idx)) {
        school_item_event(save, x, z, yaw, 0, items, max, &matched);
    }
    return matched;
}

bool sh_town_item_use(ShTown *town, ShSavegame *save, float x, float z, float yaw, uint8_t item)
{
    int matched;
    if (town->stage != SH_TOWN_EXPLORE || town->pickup_active ||
        !sh_school_map_is_school(save->map_idx) ||
        !item || sh_save_item_count(save, item) == 0) {
        return false;
    }
    const ShMapEvent *event = school_item_event(save, x, z, yaw, item, NULL, 0, &matched);
    return event && sh_school_begin(town, save, event);
}

static void school_events_update(ShTown *t, ShSavegame *save, float px, float pz, float yaw,
                                 const ShInputState *input)
{
    int event_count, point_count;
    const ShMapEvent *events = sh_school_map_events(save->map_idx, &event_count);
    const ShMapPoint *points = sh_school_map_points(save->map_idx, &point_count);
    for (int i = 0; i < event_count; i++) {
        const ShMapEvent *event = &events[i];
        if (!sh_school_event_enabled(event, save)) {
            continue;
        }
        if (event->trigger_type != SH_TRIG_TICK) {
            if (event->point_idx >= point_count ||
                !trig_match(&points[event->point_idx], event->trigger_type, px, pz, yaw)) {
                continue;
            }
        }
        if (event->activation_type == SH_ACT_ITEM ||
            (event->activation_type == SH_ACT_BUTTON && !sh_input_down(input, SH_BTN_ACTION))) {
            continue;
        }
        if (event->sys_state == SH_SYS_LOAD_OVERLAY || event->sys_state == SH_SYS_LOAD_ROOM) {
            begin_load_overlay(t, event, save->map_idx);
            return;
        }
        if (event->sys_state == SH_SYS_SAVE_MENU0 || event->sys_state == SH_SYS_SAVE_MENU1) {
            t->save_requested = true;
            t->save_location = event->event_param;
            t->controls_locked = true;
            return;
        }
        if (event->sys_state == SH_SYS_EVENT_SET_FLAG) {
            if (event->complete_flag > 0) {
                sh_save_event_set(save, (unsigned)event->complete_flag);
            }
            return;
        }
        if (event->sys_state == SH_SYS_EVENT_CALLBACK && event->event_param == sh_school_common_pickup_callback(save->map_idx)) {
            int count;
            const ShMapCommonPickup *pickups = sh_school_map_pickups(save->map_idx, &count);
            for (int j = 0; j < count; j++) {
                if (pickups[j].point_idx == event->point_idx) {
                    item_take_begin(t, &pickups[j]);
                    return;
                }
            }
        }
        if (event->sys_state == SH_SYS_READ_MESSAGE || event->sys_state == SH_SYS_EVENT_CALLBACK) {
            sh_school_begin(t, save, event);
            return;
        }
    }
}

/* MAP_EVENTS walk (events_main.c) — retail table order. */
static void town_events_update(ShTown *t, ShSavegame *save, float px, float pz, float yaw,
                               const ShInputState *input)
{
    if (t->pickup_active || t->map_open) {
        return;
    }
    if (save && sh_school_map_is_school(save->map_idx)) {
        school_events_update(t, save, px, pz, yaw, input);
        return;
    }
    if (save && save->map_idx == SH_MAP_IDX_MAP0_S02) {
        town_events_map0_s02(t, save, px, pz, yaw, input);
        return;
    }
    if (!save || save->map_idx != SH_MAP_IDX_MAP2_S00) {
        return;
    }
    const bool action = sh_input_down(input, SH_BTN_ACTION);
    for (int i = 0; i < TOWN_EVENT_COUNT; i++) {
        const ShMapEvent *ev = &s_map_events_map2_s00[i];
        const bool is_load = ev->sys_state == SH_SYS_LOAD_OVERLAY || ev->sys_state == SH_SYS_LOAD_ROOM;
        if (ev->complete_flag > 0 && sh_save_event_get(save, (unsigned)ev->complete_flag)) {
            continue;
        }
        if (ev->required_flag > 0 && !sh_save_event_get(save, (unsigned)ev->required_flag)) {
            continue;
        }

        if (ev->trigger_type == SH_TRIG_TICK && ev->sys_state == SH_SYS_EVENT_CALLBACK &&
            ev->event_param == 18) { /* MapEvent_DoorOfEclipseEnter */
            script_begin(t, SH_TOWN_SCR_ECLIPSE_ENTER, 0);
            return;
        }

        /* MapEvent_AtWaterWorks — TriggerType_Tick */
        if (ev->trigger_type == SH_TRIG_TICK && ev->sys_state == SH_SYS_EVENT_CALLBACK &&
            ev->complete_flag == EV_M2S00_WATER_CS && ev->required_flag == EV_STORY_WATER &&
            sh_save_event_get(save, EV_STORY_WATER)) {
            script_begin(t, SH_TOWN_SCR_WATER_WORKS, EV_M2S00_WATER_CS);
            return;
        }

        if (ev->point_idx >= TOWN_POINT_COUNT) {
            continue;
        }
        const ShMapPoint *pt = &s_map_points_map2_s00[ev->point_idx];

        if (ev->sys_state == SH_SYS_EVENT_SET_FLAG && ev->trigger_type == SH_TRIG_TOUCH_AABB) {
            if (trig_aabb(pt, px, pz) && ev->complete_flag > 0) {
                sh_save_event_set(save, (unsigned)ev->complete_flag);
            }
            continue;
        }

        /* func_800EB758 / func_800EB79C / func_800EB7E0 — exclusive AABB, no button. */
        if (ev->sys_state == SH_SYS_EVENT_CALLBACK && ev->activation_type == SH_ACT_EXCLUSIVE &&
            ev->trigger_type == SH_TRIG_TOUCH_AABB) {
            if (!trig_aabb(pt, px, pz)) {
                continue;
            }
            if (ev->event_param == 26) {
                school_cross_flags(save, 0);
            } else if (ev->event_param == 27) {
                school_cross_flags(save, 1);
            } else if (ev->event_param == 28) {
                school_cross_flags(save, 2);
            }
            if (ev->complete_flag > 0) {
                sh_save_event_set(save, (unsigned)ev->complete_flag);
            }
            continue;
        }

        if (ev->sys_state == SH_SYS_READ_MESSAGE) {
            if (!action || ev->activation_type != SH_ACT_BUTTON) {
                continue;
            }
            if (!trig_match(pt, ev->trigger_type, px, pz, yaw)) {
                continue;
            }
            t->read_msg_idx = ev->event_param;
            script_begin(t, SH_TOWN_SCR_READ_MSG, 0);
            return;
        }

        if (!action || ev->activation_type != SH_ACT_BUTTON) {
            continue;
        }
        if (!trig_match(pt, ev->trigger_type, px, pz, yaw)) {
            continue;
        }

        if (ev->sys_state == SH_SYS_EVENT_CALLBACK) {
            if (ev->event_param == 14) { /* func_800E9A74 */
                script_begin(t, SH_TOWN_SCR_WOODMAN_KEY, 0);
                return;
            }
            if (ev->event_param == 17) { /* func_800E9DD8 */
                script_begin(t, SH_TOWN_SCR_ECLIPSE_LOCKS, 0);
                return;
            }
            for (unsigned key_idx = 0; key_idx < sizeof(s_town_keys) / sizeof(s_town_keys[0]); key_idx++) {
                const ShTownKey *key = &s_town_keys[key_idx];
                if (ev->event_param == key->callback) {
                    t->step = 0;
                    t->controls_locked = true;
                    t->aiming = false;
                    t->pickup_active = true;
                    t->prompt_open = false;
                    t->pickup_scale = 0.0f;
                    t->pickup_spin = 0.0f;
                    t->pickup_item_id = key->item_id;
                    t->pickup_count = PICKUP_COUNT_DEFAULT;
                    t->pickup_flag = key->flag;
                    t->pickup_msg = key->message;
                    t->subtitle = NULL;
                    t->dialogue = NULL;
                    return;
                }
            }
            if (ev->event_param == 10) { /* g_MapEventFuncs[10] = func_800E9470 */
                script_begin(t, SH_TOWN_SCR_SKETCH_COVER, 0);
                return;
            }
            if (ev->event_param == 16 || ev->event_param == 1) {
                const float sfx_pos[3] = {pt->x, 1.2f, -pt->z};
                if (ev->event_param == 16) { /* MapEvent_HouseKeyUse */
                    sh_save_item_remove_one(save, SH_ITEM_HOUSE_KEY);
                    sh_snd_at(SH_SFX_USE_KEY, sfx_pos, 1.0f);
                    t->read_msg_idx = 31; /* "Used the House key." */
                    sh_save_event_set(save, 934); /* EventFlag_MapMark_OldTown_DoghouseDotOnly */
                } else { /* MapEvent_DoorLocked */
                    const bool alt = ev->point_idx == 57 || ev->point_idx == 58;
                    sh_snd_at(alt ? SH_SFX_UNK1349 : SH_SFX_DOOR_LOCKED, sfx_pos, 1.0f);
                    t->read_msg_idx = 12; /* MapMsgIdx_DoorLocked */
                }
                script_begin(t, SH_TOWN_SCR_READ_MSG, ev->complete_flag);
                return;
            }
            switch (ev->complete_flag) {
            case EV_M2S00_DOG_NOTE:
                script_begin(t, SH_TOWN_SCR_DOG_NOTE, EV_M2S00_DOG_NOTE);
                return;
            case EV_M2S00_DOG_KEY:
                script_begin(t, SH_TOWN_SCR_DOG_KEY, EV_M2S00_DOG_KEY);
                return;
            case EV_M2S00_STEEL:
                script_begin(t, SH_TOWN_SCR_STEEL_PIPE, EV_M2S00_STEEL);
                return;
            case EV_SKETCH_DONE:
                script_begin(t, SH_TOWN_SCR_SKETCHBOOK, 0);
                return;
            case EV_M2S00_PAPER_MAP:
                script_begin(t, SH_TOWN_SCR_PAPER_MAP, EV_M2S00_PAPER_MAP);
                return;
            case EV_HOUSE_DOOR:
                if (sh_save_event_get(save, EV_M2S00_DOG_KEY)) {
                    sh_save_event_set(save, EV_HOUSE_DOOR);
                }
                return;
            default:
                break;
            }
            const ShMapCommonPickup *pk = pickup_for_point(ev->point_idx);
            if (pk && ev->trigger_type == SH_TRIG_TOUCH_FACING) {
                item_take_begin(t, pk);
                return;
            }
        }

        if (is_load) {
            if (!action || ev->activation_type != SH_ACT_BUTTON) {
                continue;
            }
            if (!trig_match(pt, ev->trigger_type, px, pz, yaw)) {
                continue;
            }
            if (ev->required_flag > 0 && !sh_save_event_get(save, (unsigned)ev->required_flag)) {
                continue;
            }
            begin_load_overlay(t, ev, SH_MAP_IDX_MAP2_S00);
            return;
        }
    }
}

/* Bgm_MapRoomIdxUpdate — exterior only; interior rooms use field_4_5 from LoadRoom. */
static int8_t map2_room_idx_at(float px, float pz)
{
    const float ps1_z = -pz;
    if (px > 108.0f && px < 152.0f && ps1_z > 208.0f && ps1_z < 228.0f) {
        return 2; /* doghouse / Midwich interior bucket (map points 23–24, field_4_5=3) */
    }
    return 0;
}

static void dogs_update(ShTown *t, ShSavegame *save, float px, float py, float pz, float dt)
{
    if (!sh_save_event_get(save, SH_EV_EXIT_CAFE)) {
        return;
    }
    if (save->map_room_idx != 0) {
        for (int i = 0; i < t->dog_count; i++) {
            t->dogs[i].alive = false;
            t->dogs[i].spawn = -1;
        }
        return;
    }
    if (save->map_idx == SH_MAP_IDX_MAP2_S00) {
        town_spawn_npcs(t, save, px, pz, t->spawn_in_game);
        t->spawn_in_game = true;
    }
    for (int i = 0; i < t->dog_count; i++) {
        ShTownDog *d = &t->dogs[i];
        if (!d->alive) {
            continue;
        }
        d->anim += dt;
        if (d->cd > 0.0f) {
            d->cd -= dt;
        }
        if (d->hp <= 0.0f) {
            d->state = 4;
            d->alive = false;
            t->spawn_killed |= 1u << d->spawn;
            if (save && d->spawn >= 0) {
                save->map_enemy_slots |= 1u << (unsigned)d->spawn;
            }
            continue;
        }

        const float dx = px - d->pos[0];
        const float dz = pz - d->pos[2];
        const float dist = sqrtf(dx * dx + dz * dz);
        d->yaw = atan2f(-dx, -dz);

        if (d->air) {
            if (dist > 22.0f) {
                d->state = 0;
                continue;
            }
            d->state = dist > 6.0f ? 1 : 2;
            const float step = SH_DOG_SPEED * 0.65f * dt;
            if (dist > 0.05f) {
                d->pos[0] += dx / dist * step;
                d->pos[2] += dz / dist * step;
            }
            if (d->state == 2 && d->cd <= 0.0f && save->health > 0) {
                save->health = sh_q12_clamp(save->health - SH_Q12(SH_DOG_BITE), 0, SH_Q12(100.0f));
                d->cd = 2.0f;
                t->hit_flash = 0.25f;
                const float harry[3] = {px, py, pz};
                sh_snd_at(SH_SFX_HIT_1313, d->pos, 0.45f);
                sh_snd_at(SH_SFX_HARRY_DAMAGE, harry, 0.125f);
            }
            continue;
        }

        if (dist > 18.0f) {
            d->state = 0;
            continue;
        }
        if (dist > SH_DOG_RANGE) {
            d->state = 1;
            const float step = SH_DOG_SPEED * dt;
            if (dist > 0.05f) {
                d->pos[0] += dx / dist * step;
                d->pos[2] += dz / dist * step;
            }
        } else {
            d->state = 2;
            if (d->cd <= 0.0f && save->health > 0) {
                save->health = sh_q12_clamp(save->health - SH_Q12(SH_DOG_BITE), 0, SH_Q12(100.0f));
                d->cd = 2.2f;
                t->hit_flash = 0.25f;
                const float harry[3] = {px, py, pz};
                sh_snd_at(SH_SFX_HIT_1313, d->pos, 0.45f);
                sh_snd_at(SH_SFX_HARRY_DAMAGE, harry, 0.125f);
            }
        }
    }
}

static bool try_hit_dogs(ShTown *t, ShSavegame *save, float px, float pz, float yaw, bool knife, float range,
                         float cone_deg, float dmg)
{
    bool hit = false;
    const float cs = cosf(cone_deg * (PI_F / 180.0f));
    /* Match tank forward in game.c: (-sin yaw, -cos yaw). */
    const float fx = -sinf(yaw);
    const float fz = -cosf(yaw);
    int target = -1;
    float target_dot = -1.0f;
    if (!knife) {
        /* func_8005CD38 mode 0: select the target closest to the facing angle. */
        for (int i = 0; i < t->dog_count; i++) {
            const ShTownDog *d = &t->dogs[i];
            if (!d->alive || d->hp <= 0.0f) {
                continue;
            }
            const float dx = d->pos[0] - px, dz = d->pos[2] - pz;
            const float dist = sqrtf(dx * dx + dz * dz);
            if (dist > range || dist < 0.05f) {
                continue;
            }
            const float dot = (dx * fx + dz * fz) / dist;
            if (dot >= cs && (target < 0 || dot > target_dot)) {
                target = i;
                target_dot = dot;
            }
        }
    }
    for (int i = 0; i < t->dog_count; i++) {
        ShTownDog *d = &t->dogs[i];
        if (!d->alive || (!knife && i != target)) {
            continue;
        }
        const float dx = d->pos[0] - px;
        const float dz = d->pos[2] - pz;
        const float dist = sqrtf(dx * dx + dz * dz);
        if (dist > range || dist < 0.05f) {
            continue;
        }
        if ((dx / dist) * fx + (dz / dist) * fz < cs) {
            continue;
        }
        d->hp -= dmg;
        d->state = 3;
        d->cd = 0.6f;
        hit = true;
        sh_snd_at(knife ? SH_SFX_HIT_1316 : SH_SFX_HIT_1313, d->pos, 0.5f);
        if (d->hp <= 0.0f) {
            d->alive = false;
            d->state = 4;
            t->spawn_killed |= 1u << d->spawn;
            if (save && d->spawn >= 0) {
                save->map_enemy_slots |= 1u << (unsigned)d->spawn;
            }
        }
    }
    return hit;
}

static void combat_update(ShTown *t, ShSavegame *save, float px, float pz, float yaw, const ShInputState *input,
                          float dt)
{
    if (t->attack_cd > 0.0f) {
        t->attack_cd -= dt;
    }
    if (t->fire_anim > 0.0f) {
        t->fire_anim -= dt;
    }
    if (t->gun_flash > 0.0f) {
        t->gun_flash -= dt;
    }
    if (t->hit_flash > 0.0f) {
        t->hit_flash -= dt;
    }

    const bool gun = save->equipped_weapon == SH_ITEM_HANDGUN;
    const bool knife = save->equipped_weapon == SH_ITEM_POCKET_KNIFE;
    t->aiming = (gun || knife) && !t->controls_locked && sh_input_held(input, SH_BTN_AIM);

    if (gun) {
        t->ammo = sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS);
    }

    /* Player_Controller: firearms use held action; melee uses the attack press. */
    const bool attack = gun ? sh_input_held(input, SH_BTN_ACTION) : sh_input_down(input, SH_BTN_ACTION);
    if (!t->aiming || t->attack_cd > 0.0f || !attack) {
        return;
    }

    if (gun) {
        if (t->ammo <= 0) {
            sh_snd_play(SH_SFX_HANDGUN_EMPTY, 0, 0);
            t->attack_cd = 0.35f;
            return;
        }
        sh_save_item_remove_one(save, SH_ITEM_HANDGUN_BULLETS);
        t->ammo = sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS);
        sh_snd_play(SH_SFX_HANDGUN_SHOT, 0, 0);
        t->fire_anim = 0.35f;
        t->gun_flash = 0.12f;
        t->attack_cd = 0.45f;
        /* Player_UpperBodyUpdate: daylight firearm target cone/range. */
        try_hit_dogs(t, save, px, pz, yaw, false, 10.0f, 50.0f, 55.0f);
    } else if (knife) {
        sh_snd_play(SH_SFX_KNIFE_SWING, 0, 0);
        t->fire_anim = 0.4f;
        t->attack_cd = 0.55f;
        try_hit_dogs(t, save, px, pz, yaw, true, 1.6f, 50.0f, 28.0f);
    }
}

/* ---- public -------------------------------------------------------------- */

void sh_town_reset(ShTown *town)
{
    if (!town) {
        return;
    }
    memset(town, 0, sizeof(*town));
    town->stage = SH_TOWN_OFF;
}

void sh_town_overlay_camera_init(ShTown *town, uint8_t map_idx)
{
    if (map_idx != SH_MAP_IDX_MAP2_S00) {
        town_spawn_reset(town);
    }
    if (sh_school_map_is_school(map_idx)) {
        sh_school_map_camera_init(&town->vc, map_idx);
    } else if (map_idx == SH_MAP_IDX_MAP0_S02) {
        sh_vc_init(&town->vc, s_vc_roads_map0_s02,
                   (int)(sizeof(s_vc_roads_map0_s02) / sizeof(s_vc_roads_map0_s02[0])));
    } else if (map_idx == SH_MAP_IDX_MAP2_S00) {
        sh_vc_init(&town->vc, s_vc_roads_map2_s00, TOWN_ROAD_COUNT);
    }
}

static void town_begin_explore_common(ShTown *town, ShSavegame *save)
{
    sh_vc_init(&town->vc, s_vc_roads_map2_s00, TOWN_ROAD_COUNT);
    town->need_ground_snap = true;
    town->bgm_ready = false;
    town->map_scale = 1.0f;
    town_spawn_reset(town);
    if (save) {
        town->spawn_killed = save->map_enemy_slots & 0xFFFF0000u;
        if (save->map_idx != SH_MAP_IDX_MAP0_S02) {
            save->map_idx = SH_MAP_IDX_MAP2_S00;
        }
        save->location_id = SH_LOC_CHURCH;
        save->has_map = true;
        if (sh_save_item_count(save, SH_ITEM_HANDGUN) > 0) {
            save->equipped_weapon = SH_ITEM_HANDGUN;
            town->ammo = sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS);
        } else if (sh_save_item_count(save, SH_ITEM_POCKET_KNIFE) > 0) {
            save->equipped_weapon = SH_ITEM_POCKET_KNIFE;
        }
    }
}

void sh_town_map_point_spawn(int idx, float *x, float *z, float *yaw)
{
    if (idx < 0 || idx >= TOWN_POINT_COUNT) {
        return;
    }
    map_point_player_spawn(&s_map_points_map2_s00[idx], x, z, yaw);
}

void sh_town_begin_explore_at(ShTown *town, ShSavegame *save, float x, float z, float yaw)
{
    if (!town) {
        return;
    }
    sh_town_reset(town);
    town->stage = SH_TOWN_EXPLORE;
    town->controls_locked = false;
    town->letterbox = false;
    town->fade = 0.0f;
    town->fade_target = 0.0f;
    fade_to(town, 0.0f, FADE_SPEED);
    town_begin_explore_common(town, save);
}

void sh_town_begin(ShTown *town, ShSavegame *save)
{
    if (!town) {
        return;
    }
    sh_town_reset(town);
    town->stage = SH_TOWN_EXIT_CS;
    town->step = 0;
    town->controls_locked = true;
    town->letterbox = true;
    town->fade = 1.0f;
    fade_to(town, 0.0f, FADE_SPEED);
    exit_cam_setup(town);
    town_begin_explore_common(town, save);
    sh_snd_play(SH_SFX_CAFE_DOOR, 0, 40);
}

bool sh_town_active(const ShTown *town)
{
    return town && town->stage != SH_TOWN_OFF;
}

/* ExitCafe default case: Player_ControlUnfreeze, vcReturnPreAutoCamWork, fade in. */
static void exit_cs_finish(ShTown *town, ShSavegame *save)
{
    town->letterbox = false;
    town->controls_locked = false;
    town->map_open = false;
    town->box_mode = 0;
    town->need_ground_snap = true;
    town->stage = SH_TOWN_EXPLORE;
    town->step = 0;
    town_spawn_reset(town); /* func_800EE5D0 */
    if (save) {
        town->spawn_killed = save->map_enemy_slots & 0xFFFF0000u;
    }
    sh_vc_warp(&town->vc);
    set_msg(town, NULL, NULL, 0.0f);
    fade_to(town, 0.0f, FADE_SPEED);
    if (save) {
        sh_save_event_set(save, EV_MAPMARK_CHERYL_ARROW_SIGN);
        sh_save_event_set(save, SH_EV_EXIT_CAFE); /* clears SysFlag_NoEnemySpawn (EventFlag_146) */
        save->has_map = true;
    }
}

/* PaperMap_DrawScaled(72 - 72*D, 16 - 16*D, D/2 + 0.5) for cases 8-11. */
static void map_zoom_from_d(ShTown *t)
{
    const float d = t->map_d < 0.0f ? 0.0f : (t->map_d > 1.0f ? 1.0f : t->map_d);
    t->map_scroll_x = 72.0f - 72.0f * d;
    t->map_scroll_y = 16.0f - 16.0f * d;
    t->map_scale = 0.5f * d + 0.5f;
}

static void map_full(ShTown *t)
{
    t->map_scroll_x = 0.0f;
    t->map_scroll_y = 0.0f;
    t->map_scale = 1.0f;
}

/* MapEvent_CutsceneExitCafe (map2_s00.c), step numbers kept as retail. */
static void exit_cs_update(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)
{
    town->controls_locked = true;
    town->letterbox = true;

    if (!town->bgm_ready) {
        town->timer += dt;
        if (sh_bgm_crossfade(6) || town->timer >= 2.0f) {
            sh_bgm_crossfade_reset();
            town->bgm_ready = true;
            town->timer = 0.0f;
        }
    }

    if (sh_input_down(input, KEY_START)) {
        if (town->step == 2) {
            set_msg(town, NULL, NULL, 0.0f);
            step_set(town, 13);
        } else if (!town->skip_fading && town->step > 5 && town->step < 11) {
            fade_to(town, 1.0f, FADE_SPEED);
            town->skip_fading = true;
        }
    }
    if (town->skip_fading && fade_done(town)) {
        town->skip_fading = false;
        step_set(town, 12);
    }

    switch (town->step) {
    case 0:
        town->map_d = 0.0f;
        town->mark_alpha = 0.0f;
        town->skip_fading = false;
        if (save) {
            sh_save_event_set(save, EV_MAPMARK_COFE_ARROW);
            sh_save_event_set(save, EV_MAPMARK_COFE_SIGN);
        }
        step_set(town, 1);
        /* fallthrough */
    case 1:
        exit_cam_setup(town);
        step_set(town, 2);
        /* fallthrough */
    case 2:
        /* Event_DisplayMapMsg(16) — waits for the player. */
        set_msg(town, "Where could Cheryl have gone?", NULL, 0.0f);
        if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
            set_msg(town, NULL, NULL, 0.0f);
            step_set(town, 3);
        }
        break;
    case 3:
        /* Fs_QueueStartSeek(MP_0TOWN) */
        step_set(town, 4);
        /* fallthrough */
    case 4:
        fade_to(town, 1.0f, FADE_SPEED_LOADING);
        if (fade_done(town)) {
            step_set(town, 5);
        }
        break;
    case 5:
        town->map_open = true;
        map_full(town);
        step_set(town, 6);
        /* fallthrough */
    case 6:
        map_full(town);
        if (!town->skip_fading) {
            fade_to(town, 0.0f, FADE_SPEED);
        }
        if (fade_done(town) && !town->skip_fading) {
            step_set(town, 7);
        }
        break;
    case 7:
        map_full(town);
        town->box_mode = 1;
        town->map_d = fminf(town->map_d + 0.008f * PS1_FPS * dt, 1.0f);
        if (town->map_d >= 1.0f) {
            step_set(town, 8);
        }
        break;
    case 8:
        town->box_mode = 2;
        town->map_d = fmaxf(town->map_d - (64.0f / 4096.0f) * PS1_FPS * dt, 0.0f);
        map_zoom_from_d(town);
        if (town->map_d <= 0.0f) {
            town->box_mode = 0;
            step_set(town, 9);
        }
        break;
    case 9:
        map_zoom_from_d(town);
        if (wait_timer(town, 1.0f, dt)) {
            step_set(town, 10);
        }
        break;
    case 10:
        map_zoom_from_d(town);
        town->mark_alpha += PS1_FPS * dt;
        if (town->mark_alpha >= 128.0f) {
            town->mark_alpha = 128.0f;
            if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL) ||
                wait_timer(town, 1.5f, dt)) {
                step_set(town, 11);
            }
        }
        break;
    case 11:
        map_zoom_from_d(town);
        fade_to(town, 1.0f, FADE_SPEED);
        if (fade_done(town)) {
            step_set(town, 12);
        }
        break;
    case 12:
        /* Event_PaperMapCmd(PaperMapCmd_Unload) */
        town->map_open = false;
        town->box_mode = 0;
        town->skip_fading = false;
        step_set(town, 13);
        /* fallthrough */
    case 13:
        fade_to(town, 1.0f, FADE_SPEED);
        if (fade_done(town)) {
            step_set(town, 14);
        }
        break;
    default:
        exit_cs_finish(town, save);
        break;
    }
}

bool sh_town_update(ShTown *town, ShSavegame *save, float *player_x, float *player_y, float *player_z,
                    float *player_yaw, const ShInputState *input, float dt)
{
    if (!town || town->stage == SH_TOWN_OFF || !save || !input) {
        return false;
    }

    fade_update(town, dt);
    msg_tick(town, dt);
    if (town->harry_state != 0) {
        town->harry_time += dt;
    }

    const float px = player_x ? *player_x : 0.0f;
    const float py = player_y ? *player_y : 0.0f;
    const float pz = player_z ? *player_z : 0.0f;
    const float yaw = player_yaw ? *player_yaw : 0.0f;

    switch (town->stage) {
    case SH_TOWN_EXIT_CS:
        exit_cs_update(town, save, input, dt);
        if (town->stage == SH_TOWN_EXPLORE) {
            update_gameplay_camera(town, px, py, pz, yaw, dt);
        }
        break;

    case SH_TOWN_SCRIPT:
        script_update(town, save, player_x, player_y, player_z, player_yaw, input, dt);
        if (!town->script_use_cam) {
            update_gameplay_camera(town, player_x ? *player_x : px, player_y ? *player_y : py,
                                   player_z ? *player_z : pz, player_yaw ? *player_yaw : yaw, dt);
        }
        break;

    case SH_TOWN_LEAVING:
        town->controls_locked = true;
        if (fade_done(town)) {
            /* SysState_LoadArea_Update, game_sys_states.c: complete the triggering transition. */
            if (town->overlay_complete_flag > 0) {
                sh_save_event_set(save, (unsigned)town->overlay_complete_flag);
            }
            town->leave_overlay = true;
        }
        update_gameplay_camera(town, px, py, pz, yaw, dt);
        break;

    case SH_TOWN_EXPLORE:
        town->letterbox = false;
        if (!sh_save_event_get(save, SH_EV_EXIT_CAFE)) {
            sh_save_event_set(save, SH_EV_EXIT_CAFE);
        }
        if (town->pickup_active) {
            town->controls_locked = true;
            item_take_update(town, save, input, dt);
            update_gameplay_camera(town, px, py, pz, yaw, dt);
            break;
        }
        town->controls_locked = false;
        if (sh_input_down(input, SH_BTN_MAP) && (save->has_map || sh_save_event_get(save, 38))) {
            town->map_open = !town->map_open;
            map_full(town);
        }
        if (town->map_open) {
            town->controls_locked = true;
            town->aiming = false;
            if (sh_input_down(input, SH_BTN_CANCEL)) {
                town->map_open = false;
            }
        } else {
            if (save->map_idx == SH_MAP_IDX_MAP2_S00) {
                save->map_room_idx = map2_room_idx_at(px, pz);
            } else if (sh_school_map_is_school(save->map_idx)) {
                save->map_room_idx = sh_school_map_room_idx(save->map_idx, px, pz);
            }
            town_events_update(town, save, px, pz, yaw, input);
            if (!town->pickup_active) {
                combat_update(town, save, px, pz, yaw, input, dt);
                dogs_update(town, save, px, py, pz, dt);
            }
        }
        update_gameplay_camera(town, px, py, pz, yaw, dt);
        break;

    default:
        break;
    }

    if (sh_school_map_is_school(save->map_idx) && save->map_idx != SH_MAP_IDX_MAP1_S01) {
        sh_school_world_update(town, save, player_x ? *player_x : px, player_z ? *player_z : pz, dt);
    }
    sh_border_update(&town->border, SH_Q12(dt));
    if (town->school.smoke.active) { sh_smoke_update(&town->school.smoke, dt); }
    sh_env_update(&town->environment, sh_save_flashlight_on(save), dt);
    return town->controls_locked;
}

const char *sh_town_subtitle(const ShTown *town)
{
    if (!town) {
        return "";
    }
    if (town->prompt_open && town->pickup_msg) {
        return town->pickup_msg;
    }
    return town->subtitle ? town->subtitle : "";
}

const char *sh_town_dialogue(const ShTown *town)
{
    if (!town) {
        return "";
    }
    if (town->prompt_open && town->pickup_msg) {
        return town->pickup_msg;
    }
    return town->dialogue ? town->dialogue : "";
}

const char *sh_town_hint(const ShTown *town)
{
    if (!town || town->stage == SH_TOWN_OFF) {
        return "";
    }
    if (town->stage == SH_TOWN_SCRIPT) {
        if (town->prompt_open) {
            return "A : Yes   B : No";
        }
        if (town->subtitle && town->subtitle[0]) {
            return "A : continue";
        }
        return "";
    }
    if (town->stage == SH_TOWN_EXIT_CS) {
        if (town->step == 2) {
            return "A : continue   START : skip";
        }
        if (town->map_open) {
            return town->step == 10 && town->mark_alpha >= 128.0f ? "A / B : continue" : "START : skip";
        }
        return "";
    }
    if (town->pickup_active) {
        return town->prompt_open ? "A : Yes   B : No" : "";
    }
    if (town->map_open) {
        return "Y / B : close map";
    }
    return "L : run   R : aim   A : attack / examine\nY : carte";
}

bool sh_town_letterbox(const ShTown *town)
{
    return town && town->letterbox;
}

float sh_town_fade(const ShTown *town)
{
    return town ? town->fade : 0.0f;
}

bool sh_town_camera(const ShTown *town, ShCamera *out)
{
    if (!town || !town->cam_active) {
        return false;
    }
    if (out) {
        *out = town->cam;
    }
    return true;
}

bool sh_town_map_open(const ShTown *town)
{
    return town && (town->map_open || town->stage == SH_TOWN_EXIT_CS ||
                    (town->stage == SH_TOWN_SCRIPT &&
                     (town->script == SH_TOWN_SCR_SKETCHBOOK || town->script == SH_TOWN_SCR_PAPER_MAP ||
                      town->script == SH_TOWN_SCR_WATER_WORKS)));
}

unsigned sh_town_world_key(const ShTown *town, const ShSavegame *save)
{
    if (!town || !save || town->stage == SH_TOWN_OFF) {
        return 0;
    }
    unsigned key = 2u;
    key |= sh_save_event_get(save, 161) ? 1u << 4 : 0u;
    key |= sh_save_event_get(save, 147) ? 1u << 5 : 0u;
    key |= sh_save_event_get(save, 148) ? 1u << 6 : 0u;
    for (int i = 0; i < MAP_WORLD_OBJECT_COUNT_MAP2_S00; i++) {
        if (sh_save_event_get(save, s_map_world_objects_map2_s00[i].event_flag)) {
            key ^= (unsigned)(i + 1) * 0x9E3779B9u;
        }
    }
    return key;
}

void sh_town_map_hud(const ShTown *town, const ShSavegame *save, float player_x, float player_z, float player_yaw,
                     ShMapHud *hud, ShMapSprite *sprites, int max)
{
    if (!town || !hud) {
        return;
    }
    hud->overlay_scale = town->map_scale > 0.0f ? town->map_scale : 1.0f;
    hud->overlay_scroll_x = town->map_scroll_x;
    hud->overlay_scroll_y = town->map_scroll_y;
    hud->map_box_count = 0;
    hud->map_sprites = sprites;
    hud->map_sprite_count = 0;
    hud->map_player_valid = false;
    /* func_80067914 — always when the free / cutscene paper map is up. */
    if (save && town->map_open && town->stage != SH_TOWN_OFF) {
        float mx = 0.0f;
        float my = 0.0f;
        float turn = 0.0f;
        if (sh_papermap_player_map2_s00(save->map_idx, player_x, -player_z, -player_yaw, hud->overlay_scroll_x,
                                        hud->overlay_scroll_y, hud->overlay_scale, &mx, &my, &turn)) {
            hud->map_player_valid = true;
            hud->map_player_x = mx;
            hud->map_player_y = my;
            hud->map_player_yaw = -player_yaw + turn * 2.0f * PI_F;
        }
    }
    if (!sprites || max <= 0) {
        return;
    }

    int n = sh_papermap_marks(save, SH_PAPERMAP_OLD_TOWN, hud->overlay_scroll_x, hud->overlay_scroll_y,
                              hud->overlay_scale, sprites, max);
    const bool cs = town->stage == SH_TOWN_EXIT_CS;
    const bool sketch = town->stage == SH_TOWN_SCRIPT && town->script == SH_TOWN_SCR_SKETCHBOOK;
    if (sketch && town->step >= 15 && n < max &&
        sh_papermap_mark_flag(SH_PAPERMAP_OLD_TOWN, EV_SCHOOL_CIRCLE, town->mark_alpha / 128.0f,
                              hud->overlay_scroll_x, hud->overlay_scroll_y, hud->overlay_scale, &sprites[n])) {
        n++;
    }
    if (cs && (town->step == 10 || town->step == 11) && n < max &&
        sh_papermap_mark_flag(SH_PAPERMAP_OLD_TOWN, EV_MAPMARK_CHERYL_ARROW_SIGN, town->mark_alpha / 128.0f,
                              hud->overlay_scroll_x, hud->overlay_scroll_y, hud->overlay_scale, &sprites[n])) {
        n++;
    }
    hud->map_sprite_count = n;

    if ((cs || sketch) && town->box_mode == 1) {
        static const float start[4] = {-160.0f, -224.0f, 319.0f, 447.0f};
        static const float end_cafe[4] = {-88.0f, -192.0f, 159.0f, 223.0f};
        static const float end_sketch[4] = {-160.0f, 8.0f, 159.0f, 223.0f};
        sh_papermap_boxes(town->map_d, start, sketch ? end_sketch : end_cafe, hud->map_boxes);
        hud->map_box_count = SH_MAP_MAX_BOXES;
    } else if ((cs || sketch) && town->box_mode == 2) {
        static const float start[4] = {-160.0f, -224.0f, 319.0f, 447.0f};
        static const float end_cafe[4] = {-16.0f, -160.0f, 0.0f, 0.0f};
        static const float end_sketch[4] = {-160.0f, 240.0f, 0.0f, 0.0f};
        sh_papermap_boxes(-(town->map_d / (town->map_d + 1.0f)), start, sketch ? end_sketch : end_cafe,
                          hud->map_boxes);
        hud->map_box_count = SH_MAP_MAX_BOXES;
    }
}

bool sh_town_aiming(const ShTown *town)
{
    return town && town->aiming;
}

float sh_town_gun_flash(const ShTown *town)
{
    return town ? town->gun_flash : 0.0f;
}

bool sh_town_pickup_item(const ShTown *town, int *item_id, float *scale, float *spin)
{
    if (!town || !town->pickup_active) {
        return false;
    }
    if (item_id) {
        *item_id = town->pickup_item_id;
    }
    if (scale) {
        *scale = town->pickup_scale;
    }
    if (spin) {
        *spin = town->pickup_spin;
    }
    return true;
}

static const ShTownDog *town_dog_on_gfx(const ShTown *town, int gfx_slot)
{
    if (!town || gfx_slot < 0 || gfx_slot >= SH_GFX_MAX_ACTORS) {
        return NULL;
    }
    for (int i = 0; i < town->dog_count; i++) {
        const ShTownDog *d = &town->dogs[i];
        if (d->alive && d->gfx_slot == gfx_slot) {
            return d;
        }
    }
    return NULL;
}

bool sh_town_actor_pose(const ShTown *town, int slot, ShActorPose *out)
{
    if (!town || !out || slot < 0 || slot >= SH_GFX_MAX_ACTORS) {
        return false;
    }
    const ShTownDog *d = town_dog_on_gfx(town, slot);
    if (!d) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->slot = slot;
    out->visible = true;
    out->pos[0] = d->pos[0];
    out->pos[1] = d->pos[1] + (d->air ? 0.0f : 0.35f);
    out->pos[2] = d->pos[2];
    out->yaw = d->yaw;
    return true;
}

int sh_town_actor_keyframe(const ShTown *town, int slot, int kf_count)
{
    if (!town || kf_count <= 0 || slot < 0 || slot >= SH_GFX_MAX_ACTORS) {
        return 0;
    }
    const ShTownDog *d = town_dog_on_gfx(town, slot);
    if (!d) {
        return 0;
    }
    if (d->air) {
        /* BIRD.ANM HoverVariable 384..407 (cafe Air Screamer). */
        int start = 384, end = 407;
        const float fps = 20.0f;
        if (end >= kf_count) {
            end = kf_count - 1;
        }
        if (start >= kf_count) {
            start = 0;
            end = kf_count > 0 ? kf_count - 1 : 0;
        }
        const float span = (float)(end - start + 1);
        float t = d->anim * fps;
        while (span > 0.0f && t >= span) {
            t -= span;
        }
        return start + (int)t;
    }
    /* DOG.ANM — Chara_Groaner idle/walk clips (early file range). */
    int start = 0, end = kf_count > 11 ? 11 : kf_count - 1;
    float fps = 8.0f;
    if (d->state == 1) {
        start = kf_count > 12 ? 12 : 0;
        end = kf_count > 23 ? 23 : kf_count - 1;
        fps = 12.0f;
    } else if (d->state == 2) {
        start = kf_count > 40 ? 24 : 0;
        end = kf_count > 55 ? 55 : kf_count - 1;
        fps = 18.0f;
    } else if (d->state == 3) {
        start = kf_count > 70 ? 60 : 0;
        end = kf_count > 80 ? 80 : kf_count - 1;
        fps = 14.0f;
    }
    if (end < start) {
        end = start;
    }
    const float span = (float)(end - start + 1);
    float t = d->anim * fps;
    while (t >= span) {
        t -= span;
    }
    return start + (int)t;
}

void sh_town_bgm_update(const ShTown *town, const ShSavegame *save, bool player_dead, float dt)
{
    (void)town;
    (void)save;
    /* Map_RoomBgmInit + BgmCmd_Track6 — keep layer alive on the street. */
    sh_bgm_layers_update(SH_BGM_LAYER1 | SH_BGM_KEEP_ALIVE, 0.333f, player_dead, dt);
}

bool sh_town_take_ground_snap(ShTown *town)
{
    if (!town || !town->need_ground_snap) {
        return false;
    }
    town->need_ground_snap = false;
    return true;
}

bool sh_town_debug_fire_event(ShTown *town, int idx)
{
    if (!town || idx < 0 || idx >= TOWN_EVENT_COUNT) {
        return false;
    }
    const ShMapEvent *ev = &s_map_events_map2_s00[idx];
    if (ev->sys_state != SH_SYS_LOAD_OVERLAY && ev->sys_state != SH_SYS_LOAD_ROOM) {
        return false;
    }
    begin_load_overlay(town, ev, SH_MAP_IDX_MAP2_S00);
    return true;
}

bool sh_town_leave_overlay(const ShTown *town)
{
    return town && town->leave_overlay;
}

uint8_t sh_town_overlay_map_idx(const ShTown *town)
{
    return town ? town->overlay_map_idx : 0;
}

uint8_t sh_town_overlay_spawn_pt(const ShTown *town)
{
    return town ? town->overlay_spawn_pt : 0;
}

void sh_town_overlay_target(const ShTown *town, float *x, float *z)
{
    if (!town || !x || !z) {
        return;
    }
    *x = town->overlay_x;
    *z = town->overlay_z;
}

float sh_town_overlay_yaw(const ShTown *town)
{
    return town ? town->overlay_yaw : SH_TOWN_SPAWN_YAW;
}

const char *sh_town_bg_tim(const ShTown *town)
{
    if (!town || town->stage != SH_TOWN_SCRIPT) {
        return NULL;
    }
    if (town->script == SH_TOWN_SCR_SCHOOL) {
        return sh_school_background(town);
    }
    if (town->script == SH_TOWN_SCR_ECLIPSE_LOCKS && town->step >= 3 && town->step <= 12) {
        return s_town_lock_images[town->bg_image];
    }
    /* func_800E9470 cases 5-7: BgTextureCmd_Draw SKETCHBK.TIM */
    if (town->script == SH_TOWN_SCR_SKETCH_COVER && town->step >= 5 && town->step <= 7) {
        return "SKETCHBK.TIM";
    }
    /* MapEvent_CherylsSketchbook cases 5-9: BgTextureCmd_Draw TOSCHOOL.TIM */
    if (town->script == SH_TOWN_SCR_SKETCHBOOK && town->step >= 5 && town->step <= 9) {
        return "TOSCHOOL.TIM";
    }
    return NULL;
}

const char *sh_town_bg_next_tim(const ShTown *town, float *alpha)
{
    if (town && town->stage == SH_TOWN_SCRIPT && town->script == SH_TOWN_SCR_SCHOOL) {
        return sh_school_background_next(town, alpha);
    }
    if (!town || town->stage != SH_TOWN_SCRIPT || town->script != SH_TOWN_SCR_ECLIPSE_LOCKS || town->step != 9) {
        return NULL;
    }
    if (alpha) {
        *alpha = town->bg_transition;
    }
    return s_town_lock_images[town->bg_next_image];
}

bool sh_town_harry_keyframe(const ShTown *town, int *keyframe)
{
    if (sh_school_harry_keyframe(town, keyframe)) {
        return true;
    }
    if (!town || town->stage != SH_TOWN_SCRIPT || town->script != SH_TOWN_SCR_WOODMAN_KEY || !town->harry_state) {
        return false;
    }
    const int first = town->harry_state == 59 ? SH_TOWN_STATE_59_FIRST : SH_TOWN_STATE_60_FIRST;
    const int last = town->harry_state == 59 ? SH_TOWN_STATE_59_LAST : SH_TOWN_STATE_60_LAST;
    const float fps = town->harry_state == 59 ? SH_TOWN_STATE_59_FPS : SH_TOWN_STATE_60_FPS;
    int frame = first + (int)(town->harry_time * fps);
    if (frame > last) {
        frame = last;
    }
    if (keyframe) {
        *keyframe = frame;
    }
    return true;
}

bool sh_town_overlay_is_room(const ShTown *town)
{
    return town && town->overlay_sys_state == SH_SYS_LOAD_ROOM;
}

bool sh_town_overlay_skip_fade_in(const ShTown *town)
{
    return town && (town->overlay_transition_flags & SH_RETAIL_AreaTransitionFlag_SkipFadeIn) != 0;
}
