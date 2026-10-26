#include "sh3ds/school.h"
#include "sh3ds/town.h"
#include "sh3ds/fixed.h"
#include "sh3ds/controls.h"
#include "sh3ds/snd.h"
#include "sh3ds/sound.h"
#include "sh3ds/roof_water.h"

#include "mapdata_map1_s00.h"
#include "map_pickups_map1_s00.h"
#include "vc_roads_map1_s00.h"
#include "mapdata_map1_s01.h"
#include "map_pickups_map1_s01.h"
#include "vc_roads_map1_s01.h"
#include "mapdata_map1_s02.h"
#include "map_pickups_map1_s02.h"
#include "vc_roads_map1_s02.h"
#include "mapdata_map1_s03.h"
#include "map_pickups_map1_s03.h"
#include "vc_roads_map1_s03.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

typedef struct ShSchoolMessage {
    const char *text;
    bool end;
} ShSchoolMessage;

typedef struct ShSchoolInspection {
    int callback;
    const char *image;
    float fade_out;
    float fade_in;
    int message;
    int dim_message;
    unsigned brightness;
} ShSchoolInspection;

typedef struct ShSchoolMedallion {
    int callback;
    const char *from;
    const char *to;
    int message;
    int flag;
    int marker;
    uint16_t sounds[2];
} ShSchoolMedallion;

typedef struct ShSchoolClock {
    const char *image;
    int message;
} ShSchoolClock;

typedef struct ShSchoolPickup {
    int callback;
    int item;
    int flag;
    int message;
    int prompt_step;
} ShSchoolPickup;

typedef struct ShSchoolAnimation {
    int state;
    int first;
    int last;
    float fps;
} ShSchoolAnimation;

typedef struct ShSchoolAcid {
    int callback, item, flag, animation, sound;
    float player[2], yaw, camera[3], look[3], sound_pos[3], volume;
    float animation_wait, smoke_wait;
} ShSchoolAcid;

#include "school_data.h"
#include "school_upper_data.h"
#include "school_alternative_data.h"
#include "school_roof_data.h"
static bool in_chunk(int32_t value, int a, int b, int c, int d);
#define SH_SCHOOL_ROOF_PROP_MASK
#include "school_roof_props.h"
#include "school_roof_keys.h"
#include "school_roof_pickup_data.h"
#include "school_roof_locker.h"
#include "school_ladder_anims.h"
#include "school_arrival_anim.h"
#include "school_acid_data.h"
#include "piano_scene_data.h"
#define SH_SCHOOL_BATHROOM_PROP_MASK
#include "school_bathroom_props.h"
#include "school_boss_world.h"
#include "school_phone_world.h"

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

#include "school_routing.h"

bool sh_school_map_is_school(uint8_t map)
{
    return map == SH_MAP_IDX_MAP1_S00 || map == SH_MAP_IDX_MAP1_S01 ||
           map == SH_MAP_IDX_MAP1_S02 || map == SH_MAP_IDX_MAP1_S03;
}

const ShMapPoint *sh_school_points(int *count)
{
    *count = COUNT(s_map_points_map1_s00);
    return s_map_points_map1_s00;
}

const ShMapEvent *sh_school_events(int *count)
{
    *count = COUNT(s_map_events_map1_s00);
    return s_map_events_map1_s00;
}

const ShMapCommonPickup *sh_school_pickups(int *count)
{
    *count = MAP_PICKUP_COUNT_MAP1_S00;
    return s_map_pickups_map1_s00;
}

const ShMapPoint *sh_school_map_points(uint8_t map, int *count)
{
    if (map == SH_MAP_IDX_MAP1_S03) {
        *count = COUNT(s_map_points_map1_s03);
        return s_map_points_map1_s03;
    }
    if (map == SH_MAP_IDX_MAP1_S02) {
        *count = COUNT(s_map_points_map1_s02);
        return s_map_points_map1_s02;
    }
    if (map == SH_MAP_IDX_MAP1_S01) {
        *count = COUNT(s_map_points_map1_s01);
        return s_map_points_map1_s01;
    }
    return sh_school_points(count);
}

const ShMapEvent *sh_school_map_events(uint8_t map, int *count)
{
    if (map == SH_MAP_IDX_MAP1_S03) {
        *count = COUNT(s_map_events_map1_s03);
        return s_map_events_map1_s03;
    }
    if (map == SH_MAP_IDX_MAP1_S02) {
        *count = COUNT(s_map_events_map1_s02);
        return s_map_events_map1_s02;
    }
    if (map == SH_MAP_IDX_MAP1_S01) {
        *count = COUNT(s_map_events_map1_s01);
        return s_map_events_map1_s01;
    }
    return sh_school_events(count);
}

const ShMapCommonPickup *sh_school_map_pickups(uint8_t map, int *count)
{
    if (map == SH_MAP_IDX_MAP1_S03) {
        *count = MAP_PICKUP_COUNT_MAP1_S03;
        return s_map_pickups_map1_s03;
    }
    if (map == SH_MAP_IDX_MAP1_S02) {
        *count = MAP_PICKUP_COUNT_MAP1_S02;
        return s_map_pickups_map1_s02;
    }
    if (map == SH_MAP_IDX_MAP1_S01) {
        *count = MAP_PICKUP_COUNT_MAP1_S01;
        return s_map_pickups_map1_s01;
    }
    return sh_school_pickups(count);
}

void sh_school_map_camera_init(ShVc *vc, uint8_t map)
{
    if (map == SH_MAP_IDX_MAP1_S03) {
        sh_vc_init(vc, s_vc_roads_map1_s03, COUNT(s_vc_roads_map1_s03));
    } else if (map == SH_MAP_IDX_MAP1_S02) {
        sh_vc_init(vc, s_vc_roads_map1_s02, COUNT(s_vc_roads_map1_s02));
    } else if (map == SH_MAP_IDX_MAP1_S01) {
        sh_vc_init(vc, s_vc_roads_map1_s01, COUNT(s_vc_roads_map1_s01));
    } else {
        sh_school_camera_init(vc);
    }
}

void sh_school_camera_init(ShVc *vc)
{
    sh_vc_init(vc, s_vc_roads_map1_s00, COUNT(s_vc_roads_map1_s00));
}

/* map_util.c Map_RoomIdxGet, MAP1_S00 fallback grid. */
uint8_t sh_school_room_idx(float x, float z)
{
    int32_t px = SH_Q12(x);
    int32_t pz = SH_Q12(-z);
    if (px < SH_SCHOOL_ROOM_MIN_X || px >= SH_SCHOOL_ROOM_MAX_X ||
        pz < SH_SCHOOL_ROOM_MIN_Z || pz >= SH_SCHOOL_ROOM_MAX_Z) {
        return 0;
    }
    px = (px - SH_SCHOOL_ROOM_MIN_X) / SH_SCHOOL_ROOM_CHUNK;
    pz = (pz - SH_SCHOOL_ROOM_MIN_Z) / SH_SCHOOL_ROOM_CHUNK;
    return s_school_rooms[px * SH_SCHOOL_ROOM_STRIDE + pz];
}

uint8_t sh_school_map_room_idx(uint8_t map, float x, float z)
{
    if (map != SH_MAP_IDX_MAP1_S01 && map != SH_MAP_IDX_MAP1_S02 && map != SH_MAP_IDX_MAP1_S03) {
        return sh_school_room_idx(x, z);
    }
    int32_t px = SH_Q12(x), pz = SH_Q12(-z);
    if (px < SH_SCHOOL_ROOM_MIN_X || px >= SH_SCHOOL_ROOM_MAX_X ||
        pz < SH_SCHOOL_ROOM_MIN_Z || pz >= SH_SCHOOL_ROOM_MAX_Z) {
        return 0;
    }
    px = (px - SH_SCHOOL_ROOM_MIN_X) / SH_SCHOOL_ROOM_CHUNK;
    pz = (pz - SH_SCHOOL_ROOM_MIN_Z) / SH_SCHOOL_ROOM_CHUNK;
    const uint8_t *rooms = map == SH_MAP_IDX_MAP1_S02 ? s_school_alternative_rooms : s_school_upper_rooms;
    if (map == SH_MAP_IDX_MAP1_S03) { rooms = s_school_roof_rooms; }
    return rooms[px * SH_SCHOOL_ROOM_STRIDE + pz];
}

/* events_main.c: completed non-exclusive map markings may be inspected again. */
bool sh_school_event_enabled(const ShMapEvent *event, const ShSavegame *save)
{
    if (event->required_flag > 0 && !sh_save_event_get(save, (unsigned)event->required_flag)) {
        return false;
    }
    return event->complete_flag <= 0 || !sh_save_event_get(save, (unsigned)event->complete_flag) ||
           (event->complete_flag >= SH_SCHOOL_EventFlag_MapMark_Begin &&
            event->activation_type != SH_ACT_EXCLUSIVE && event->sys_state != SH_SYS_EVENT_SET_FLAG);
}

static const ShSchoolInspection *inspection(int callback, uint8_t map)
{
    if (map == SH_MAP_IDX_MAP1_S03) {
        return callback == s_school_roof_locker.callback ? &s_school_roof_locker : NULL;
    }
    if (map == SH_MAP_IDX_MAP1_S02) {
        for (int i = 0; i < COUNT(s_school_alternative_inspections); i++) {
            if (s_school_alternative_inspections[i].callback == callback) {
                return &s_school_alternative_inspections[i];
            }
        }
        return NULL;
    }
    if (map == SH_MAP_IDX_MAP1_S01) {
        for (int i = 0; i < COUNT(s_school_upper_inspections); i++) {
            if (s_school_upper_inspections[i].callback == callback) {
                return &s_school_upper_inspections[i];
            }
        }
        return NULL;
    }
    for (int i = 0; i < COUNT(s_school_inspections); i++) {
        if (s_school_inspections[i].callback == callback) {
            return &s_school_inspections[i];
        }
    }
    return NULL;
}

static const ShSchoolPickup *upper_pickup(int callback)
{
    for (int i = 0; i < COUNT(s_school_upper_pickups); i++) {
        if (s_school_upper_pickups[i].callback == callback) {
            return &s_school_upper_pickups[i];
        }
    }
    return NULL;
}

static const ShSchoolPickup *alternative_pickup(int callback)
{
    for (int i = 0; i < COUNT(s_school_alternative_pickups); i++) {
        if (s_school_alternative_pickups[i].callback == callback) {
            return &s_school_alternative_pickups[i];
        }
    }
    return NULL;
}

static int alternative_pickup_count(int callback)
{
    for (int i = 0; i < COUNT(s_school_alternative_pickup_counts); i++) {
        if (s_school_alternative_pickup_counts[i].callback == callback) {
            return s_school_alternative_pickup_counts[i].count;
        }
    }
    return 1;
}

static const ShSchoolMedallion *medallion(int callback)
{
    for (int i = 0; i < COUNT(s_school_medallions); i++) {
        if (s_school_medallions[i].callback == callback) {
            return &s_school_medallions[i];
        }
    }
    return NULL;
}

static void step_set(ShTown *town, int step)
{
    town->step = step;
    town->script_ticks = 0;
    town->school.substep = 0;
    town->school.fade_started = false;
}

static void fade_to(ShTown *town, float target, float speed)
{
    town->fade_target = target;
    town->fade_speed = speed == 0.0f ? 3.0f : speed;
}

static bool fade_done(const ShTown *town)
{
    return fabsf(town->fade - town->fade_target) < 0.001f;
}

/* Event_ScreenFadeCmd(Auto): start and wait occur on separate invocations. */
static bool fade_auto(ShTown *town, float target, float speed)
{
    if (!town->school.fade_started) {
        fade_to(town, target, speed);
        town->school.fade_started = true;
        return false;
    }
    if (!fade_done(town)) {
        return false;
    }
    town->school.fade_started = false;
    return true;
}

static void finish(ShTown *town)
{
    town->stage = SH_TOWN_EXPLORE;
    town->script = SH_TOWN_SCR_NONE;
    town->controls_locked = false;
    town->prompt_open = false;
    town->pickup_active = false;
    town->pickup_msg = NULL;
    town->subtitle = NULL;
    town->dialogue = NULL;
    town->school.image = NULL;
    town->school.next_image = NULL;
    town->school.brightness = 128;
    town->school.choice_active = false;
    sh_vc_warp(&town->vc);
}

static void ladder_wait(ShTown *town, float dt, int32_t delay)
{
    town->script_ticks += SH_Q12(dt);
    if (town->script_ticks > delay) { step_set(town, town->step + 1); }
}

static void ladder_fade(ShTown *town, int command, bool out, int32_t speed)
{
    if (!command) { fade_to(town, out ? 1.0f : 0.0f, speed / 4096.0f); }
    else if (command == 1) {
        if (fade_done(town)) { step_set(town, town->step + 1); }
    } else if (fade_auto(town, out ? 1.0f : 0.0f, speed / 4096.0f)) {
        step_set(town, town->step + 1);
    }
}

static void ladder_camera(ShTown *town, bool look, int32_t x, int32_t y, int32_t z)
{
    if (!look) {
        town->cam.pos[0] = x / 4096.0f;
        town->cam.pos[1] = -y / 4096.0f;
        town->cam.pos[2] = -z / 4096.0f;
    } else {
        const float dx = x / 4096.0f - town->cam.pos[0];
        const float dy = -y / 4096.0f - town->cam.pos[1];
        const float dz = -z / 4096.0f - town->cam.pos[2];
        town->cam.yaw = atan2f(-dx, -dz);
        town->cam.pitch = atan2f(dy, hypotf(dx, dz));
        town->cam.fov_deg = 53.13f;
        town->cam_active = town->script_use_cam = true;
        if (town->school.overlay == SH_MAP_IDX_MAP1_S03) {
            town->school.locker_camera_active = false;
        }
    }
}

static void ladder_animation(ShTown *town, int animation)
{
    town->harry_state = animation;
    town->harry_time = 0.0f;
}

static void ladder_unfreeze(ShTown *town)
{
    town->harry_state = 0;
    town->controls_locked = false;
}

static void ladder_finish(ShTown *town)
{
    town->cam_active = town->script_use_cam = false;
    finish(town);
}

#include "school_ladders.h"

/* Gfx_MapMsg_Draw: adjacent strings without ~E remain in the same message. */
static bool message(ShTown *town, const ShInputState *input)
{
    const int idx = town->school.message;
    const bool upper = town->school.overlay == SH_MAP_IDX_MAP1_S01;
    const ShSchoolMessage *messages = upper ? s_school_upper_messages : s_school_messages;
    int count = upper ? COUNT(s_school_upper_messages) : COUNT(s_school_messages);
    if (town->school.overlay == SH_MAP_IDX_MAP1_S02) {
        messages = s_school_alternative_messages;
        count = COUNT(s_school_alternative_messages);
    }
    if (town->school.overlay == SH_MAP_IDX_MAP1_S03) {
        messages = s_school_roof_messages;
        count = COUNT(s_school_roof_messages);
    }
    if (idx < 0 || idx >= count) {
        return false;
    }
    town->subtitle = messages[idx].text;
    if (!sh_input_down(input, SH_BTN_ACTION)) {
        return false;
    }
    town->subtitle = NULL;
    if (!messages[idx].end) {
        town->school.message++;
        return false;
    }
    return true;
}

static void card_animation(ShTown *town, int state, float dt)
{
    if (!town->school.substep) {
        sh_path_school_animation_start(&town->school.card_player, state);
        town->harry_state = state;
        town->harry_time = 0.0f;
        town->school.substep = 1;
    } else if (town->school.substep == 1) {
        ShAnimPoseTiming pose;
        sh_path_animation_timing(&town->school.card_player, SH_Q12(dt), 0, 0, &pose);
        if (sh_path_school_animation_done(&town->school.card_player)) { town->school.substep = 2; }
    } else {
        step_set(town, town->step + 1);
    }
}

static void card_warp(ShTown *town)
{
    town->school.player_warp = true;
    town->school.player_warp_position[0] = town->school.card_player.position.vx / 4096.0f;
    town->school.player_warp_position[1] = -town->school.card_player.position.vz / 4096.0f;
    town->school.player_warp_yaw = -town->school.card_player.rotation.vy * (2.0f * 3.14159265358979323846f / 4096.0f);
}

static void card_message(ShTown *town, const ShInputState *input, int index)
{
    if (!town->school.substep) {
        town->school.message = index;
        town->school.substep = 1;
    }
    if (message(town, input)) { step_set(town, town->step + 1); }
}

#include "school_card.h"
#include "school_roof_books.h"

static ShRoofWater s_roof_water;

void sh_school_roof_water_reset(void)
{
    sh_roof_water_stop(&s_roof_water);
}

void sh_school_roof_world_reset(ShTown *town)
{
    sh_school_roof_water_reset();
    town->school.valve = (ShDrainValve){0};
    town->school.valve_active = false;
    town->school.player_hidden = false;
    town->school.drain_light_override = false;
    town->school.drain_flare_suppressed = false;
    town->school.hand_world_valid = false;
    town->school.locker = (ShLockerWorld){0};
    town->school.locker_clock = (ShLockerClock){0};
    town->school.locker_frame = (ShLockerFrame){0};
    town->school.locker_frame.draw_mask = 7;
    town->school.locker_render_active = false;
    town->school.locker_camera_mode = 0;
    town->school.locker_camera_active = false;
}

const ShRoofWater *sh_school_roof_water(const ShTown *town)
{
    return town->school.overlay == SH_MAP_IDX_MAP1_S03 && (s_roof_water.pool.flags & (1u << 3)) ? &s_roof_water : NULL;
}

void sh_school_roof_water_update(ShTown *town, int yaw, const ShEnvState *environment,
    const ShEnvLight *light, float dt)
{
    if (sh_school_roof_water(town)) {
        sh_roof_water_update(&s_roof_water, yaw, environment, light, SH_Q12(dt), sh_piano_gravity(dt));
    }
}

static void valve_prompt(ShTown *town, const ShInputState *input, int index, int yes, int no)
{
    town->school.message = index;
    town->subtitle = s_school_roof_messages[index].text;
    town->prompt_open = true;
    if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
        town->prompt_open = false;
        town->subtitle = NULL;
        step_set(town, sh_input_down(input, SH_BTN_ACTION) ? yes : no);
    }
}

#include "school_roof_valve.h"
#include "school_locker_body.h"

/* Event_ScreenFadeCmd(Auto, ScreenBorders). */
static void locker_border_in(ShTown *town)
{
    if (!town->school.fade_started) {
        town->border.state = SH_BORDER_FADEINSTART;
        town->school.fade_started = true;
    } else if (town->border.state == SH_BORDER_SHOWN) {
        step_set(town, town->step + 1);
    }
}

/* Event_BgTextureFadeIn: load, wait for black, draw and fade from black. */
static void locker_image_in(ShTown *town, const char *image, float to_black, float from_black)
{
    switch (town->school.substep) {
    case 0:
        fade_to(town, 1.0f, to_black);
        town->school.substep = 1;
        /* fall through */
    case 1:
        town->school.substep = 2;
        break;
    case 2:
        if (fade_done(town)) { town->school.substep = 3; }
        break;
    default:
        town->school.image = image;
        if (fade_auto(town, 0.0f, from_black)) { step_set(town, town->step + 1); }
        break;
    }
}

/* Event_BgTextureFadeOut. */
static void locker_image_out(ShTown *town, float from_black, float to_black)
{
    if (!town->school.substep) {
        if (fade_auto(town, 1.0f, to_black)) { town->school.substep = 1; }
        return;
    }
    town->school.image = NULL;
    fade_to(town, 0.0f, from_black);
    step_set(town, town->step + 1);
}

static void locker_animation_start(ShTown *town, int state)
{
    sh_path_school_animation_start(&town->school.card_player, state);
    town->harry_state = state;
    town->harry_time = 0.0f;
}

/* ScreenFade_ResetTimestep advances directly to black on the next screen update. */
static void locker_fade_reset(ShTown *town)
{
    town->fade = 1.0f;
    fade_to(town, 1.0f, 0.0f);
}

static void locker_animation_reset(ShTown *town)
{
    town->harry_state = 0;
    town->school.image = NULL;
}

#include "school_locker_open.h"
#include "school_bathroom_scene.h"

static void boss_choice(ShTown *town, const ShInputState *input, float dt)
{
    if (!town->school.choice_active) {
        sh_map_choice_begin(&town->school.choice, 3);
        for (int row = 0; row < 3; row++) { town->school.choice_options[row] = s_school_alternative_messages[42 + row].text; }
        town->school.choice_active = town->prompt_open = true;
        return;
    }
    const ShMapChoiceFrame frame = sh_map_choice_step(&town->school.choice, SH_Q12(dt),
        sh_input_down(input, KEY_DUP | KEY_CPAD_UP), sh_input_down(input, KEY_DDOWN | KEY_CPAD_DOWN),
        sh_input_down(input, SH_BTN_ACTION), sh_input_down(input, SH_BTN_CANCEL));
    const unsigned sounds = frame.moves ? frame.moves : frame.selection >= 0 ? 1 : 0;
    for (unsigned i = 0; i < sounds; i++) { sh_snd_play(frame.sound, 0, 191); }
    if (frame.selection >= 0) {
        town->school.choice_active = town->prompt_open = false;
        step_set(town, frame.selection == 0 ? 2 : frame.selection == 1 ? 4 : -1);
    }
}

static void boss_camera_position(ShTown *town, int32_t x, int32_t y, int32_t z,
    int32_t acceleration_xz, int32_t acceleration_y, int32_t speed_xz, int32_t speed_y)
{
    const float position[3] = {SH_Q12_TO_FLOAT(x), SH_Q12_TO_FLOAT(y), SH_Q12_TO_FLOAT(z)};
    const ShVcScriptParams params = {{SH_Q12_TO_FLOAT(acceleration_xz), SH_Q12_TO_FLOAT(acceleration_y),
        SH_Q12_TO_FLOAT(speed_xz), SH_Q12_TO_FLOAT(speed_y)}, {0}};
    sh_vc_camera_target(&town->vc, position, &params, false);
    town->script_use_cam = false;
}

static void boss_finish(ShTown *town)
{
    const bool warp = town->vc.warp;
    finish(town);
    town->vc.warp = warp;
}

#include "school_boss_scene.h"

static int32_t descent_min(int32_t a, int32_t b) { return a < b ? a : b; }
static void descent_fade_start(ShTown *town, int32_t speed)
{
    town->school.descent_fade_speed = speed;
    fade_to(town, 1.0f, SH_Q12_TO_FLOAT(speed));
}
static void descent_fade_wait(ShTown *town)
{
    if (town->fade >= 1.0f) { step_set(town, town->step + 1); }
}

#include "school_boss_descent.h"

static bool s_phone_ready, s_phone_previous_ready;

void sh_school_phone_assets(bool phone_ready, bool previous_ready)
{
    s_phone_ready = phone_ready;
    s_phone_previous_ready = previous_ready;
}

static bool phone_held_ready(const ShTown *town)
{
    return town->school.phone.held ? s_phone_ready : s_phone_previous_ready;
}

static bool phone_assets_wait(const ShTown *town) { return phone_held_ready(town); }

static void phone_freeze(ShTown *town)
{
    town->controls_locked = true;
    sh_phone_audio_reset(&town->school.phone.audio);
}

/* Event_WaitPlayerStop. */
static void phone_wait_stop(ShTown *town, float dt)
{
    town->script_ticks += SH_Q12(dt);
    if (!town->school.card_player.move_speed || town->script_ticks > SH_Q12(1.0f)) {
        step_set(town, town->step + 1);
    }
}

static void phone_path(ShTown *town, float dt)
{
    if (sh_phone_path_step(&town->school.phone.motion, &town->school.card_player, SH_Q12(dt))) {
        step_set(town, town->step + 1);
    }
    town->harry_state = town->school.card_player.state;
}

static void phone_camera(ShTown *town, int step, float dt)
{
    ShPhoneView view;
    if (!sh_phone_camera_view(&town->school.phone.motion, &town->school.card_player, step, SH_Q12(dt), &view)) { return; }
    const float position[3] = {SH_Q12_TO_FLOAT(view.position.vx), SH_Q12_TO_FLOAT(view.position.vy), SH_Q12_TO_FLOAT(view.position.vz)};
    const float look[3] = {SH_Q12_TO_FLOAT(view.look.vx), SH_Q12_TO_FLOAT(view.look.vy), SH_Q12_TO_FLOAT(view.look.vz)};
    sh_vc_script_update(&town->vc, position, look, NULL, view.warp_position, view.warp_look, dt, &town->cam);
    town->cam_active = town->script_use_cam = true;
}

static void phone_animation_wait(ShTown *town)
{
    if (sh_path_school_animation_done(&town->school.card_player)) { step_set(town, town->step + 1); }
}

static void phone_unfreeze(ShTown *town)
{
    sh_path_state_set(&town->school.card_player, 0);
    town->school.phone.hidden = false;
    ladder_unfreeze(town);
}

static void school_timed_message(ShTown *town, const ShInputState *input, int index, float dt,
    bool voice, const uint16_t *voices, unsigned voice_count)
{
    ShPhoneScene *phone = &town->school.phone;
    if (voice) { sh_bgm_status_set(SH_BGM_STATUS_VOICE_DIALOG); }
    if (!town->school.substep) {
        phone->text_frames = 0;
        phone->text_delta = 0;
        phone->text_skip = false;
        town->school.substep = 1;
    }
    phone->text_frames += (double)dt * 30.0;
    phone->text_delta += SH_Q12(dt);
    phone->text_skip |= sh_input_down(input, SH_BTN_ACTION | SH_BTN_CANCEL) || (input->held & KEY_START) != 0;
    while (phone->text_frames >= 1.0) {
        const int32_t delta = (int32_t)llround((double)phone->text_delta / phone->text_frames);
        phone->text_frames -= 1.0;
        phone->text_delta -= delta;
        const int result = sh_phone_text_step(&phone->text, index, delta, phone->text_skip,
            (sh_bgm_status() & SH_BGM_STATUS_VOICE_DIALOG) != 0, sh_sound_streaming());
        phone->text_skip = false;
        if (phone->text.stop_audio) { sh_snd_call(19); }
        if (voice && result == -1 && town->school.arrival_audio_index < voice_count) {
            sh_snd_call(voices[town->school.arrival_audio_index++]);
        }
        town->subtitle = phone->text.visible[0] ? phone->text.visible : NULL;
        if (result > 0) {
            town->subtitle = NULL;
            step_set(town, town->step + 1);
            break;
        }
    }
}

static void phone_message(ShTown *town, const ShInputState *input, int index, float dt)
{
    school_timed_message(town, input, index, dt, false, NULL, 0);
}

#include "school_phone_scene.h"

static void arrival_view_position(ShTown *town, ShPathPoint *position)
{
    *position = (ShPathPoint){SH_Q12(town->cam.pos[0]), -SH_Q12(town->cam.pos[1]), -SH_Q12(town->cam.pos[2])};
}

static void arrival_unfreeze(ShTown *town, bool idle)
{
    if (idle) { card_warp(town); }
    phone_unfreeze(town);
}

static void arrival_camera_release(ShTown *town, bool warp)
{
    town->cam_active = town->script_use_cam = false;
    if (warp) { sh_vc_warp(&town->vc); }
}

static void arrival_autosave(ShTown *town, ShSavegame *save)
{
    const ShPathPlayer *player = &town->school.card_player;
    sh_save_autosave_set(save, player->position.vx, player->position.vy, player->position.vz, player->rotation.vy);
}

/* Event_CharaAnimPlayToEnd: the player update advances playback once after the event. */
static void arrival_animation(ShTown *town)
{
    if (!town->school.substep) {
        locker_animation_start(town, 74);
        town->school.substep = 1;
    } else if (town->school.substep == 1) {
        if (sh_path_school_animation_done(&town->school.card_player)) { town->school.substep = 2; }
    } else { step_set(town, town->step + 1); }
}

#include "school_arrival_scene.h"

/* GameState_InGame_Update runs the event before Player_Update. */
static void phone_update(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)
{
    if (town->school.callback == SH_ARRIVAL_CALLBACK) {
        if (town->school.transition_flags && town->school.completion_flag > 0) { sh_save_event_set(save, town->school.completion_flag); }
        arrival_scene(town, save, input, dt);
    } else { phone_scene(town, save, input, dt); }
    if (town->stage != SH_TOWN_SCRIPT) { return; }
    ShPathPlayer *player = &town->school.card_player;
    sh_phone_animation_events(save, player->state, player->model.keyframe, &town->school.phone.hidden);
    const bool walk = player->state == 53 || player->state == 56 || player->state == 57;
    if (walk) { sh_path_locomotion(&town->school.phone.motion.path, player, player->model.keyframe, SH_Q12(dt)); }
    town->school.phone.audio_frames += (double)dt * 30.0;
    while (town->school.phone.audio_frames >= 1.0) {
        town->school.phone.audio_frames -= 1.0;
        ShPhoneAudioFrame audio;
        sh_phone_audio_step(&town->school.phone.audio, player, town->school.phone.ground_type, &audio);
        const float position[3] = {SH_Q12_TO_FLOAT(player->position.vx), -SH_Q12_TO_FLOAT(player->position.vy), -SH_Q12_TO_FLOAT(player->position.vz)};
        for (unsigned i = 0; i < audio.count; i++) {
            const ShPhoneSound *sound = &audio.sounds[i];
            if (sound->flags & sh_phone_sound_no_distance_flag()) {
                sh_snd_play(sound->id, sh_snd_camera_balance(&town->cam, position), (uint8_t)~sound->volume);
            } else {
                sh_snd_at_pitch(sound->id, position, sound->volume / 256.0f, sound->pitch);
            }
        }
    }
    if (town->school.phone.locked) { return; }
    ShAnimPoseTiming pose;
    if (walk || player->state == 74 || player->state == 97 || player->state == 98) {
        sh_path_animation_timing(player, SH_Q12(dt), 0, 0, &pose);
    }
    if (walk) {
        const int32_t distance = sh_q12_mul(player->move_speed, SH_Q12(dt));
        player->position.vx += sh_q12_mul(distance, sh_fixed_sin(player->heading_angle));
        player->position.vz += sh_q12_mul(distance, sh_fixed_cos(player->heading_angle));
        card_warp(town);
    }
    town->harry_state = player->state;
}

static bool precise_school_callback(uint8_t overlay, int callback)
{
    if (overlay == SH_MAP_IDX_MAP1_S03) {
        return callback == SH_SCHOOL_ROOF_BALL_CALLBACK || callback == SH_SCHOOL_ROOF_DRAIN0_CALLBACK ||
               callback == SH_SCHOOL_ROOF_DRAIN1_CALLBACK || callback == SH_SCHOOL_ROOF_KEY_PICKUP_CALLBACK ||
               callback == SH_SCHOOL_LOCKER_OPEN_CALLBACK;
    }
    const ShSchoolPickup *pickup = alternative_pickup(callback);
    return callback == SH_SCHOOL_ALT_CARD_CALLBACK || callback == SH_BOSS_DESCENT_CALLBACK ||
        callback == SH_PHONE_CALLBACK || callback == SH_ARRIVAL_CALLBACK || (pickup && pickup->prompt_step == 5);
}

bool sh_school_begin(ShTown *town, ShSavegame *save, const ShMapEvent *event)
{
    const int callback = event->sys_state == SH_SYS_READ_MESSAGE ? -1 : event->event_param;
    const uint8_t overlay = save && (save->map_idx == SH_MAP_IDX_MAP1_S01 || save->map_idx == SH_MAP_IDX_MAP1_S02 ||
                                    save->map_idx == SH_MAP_IDX_MAP1_S03) ?
                            save->map_idx : SH_MAP_IDX_MAP1_S00;
    if (overlay == SH_MAP_IDX_MAP1_S03) {
        if (callback != -1 && callback > 2 && callback != SH_SCHOOL_ROOF_MONSTER_CALLBACK &&
            callback != SH_SCHOOL_ROOF_VALVE_CALLBACK && callback != SH_SCHOOL_LOCKER_BODY_CALLBACK &&
            !roof_key_use(callback) &&
            !inspection(callback, overlay) &&
            callback != SH_SCHOOL_ROOF_FAIRY_CALLBACK && !precise_school_callback(overlay, callback)) { return false; }
    } else if (overlay == SH_MAP_IDX_MAP1_S02) {
        if (callback != -1 && callback > 2 && callback != SH_SCHOOL_ALT_TOWER_CALLBACK &&
            callback != SH_SCHOOL_ALT_LOBBY_CALLBACK &&
            callback != SH_SCHOOL_ALT_SLOT_CALLBACK &&
            callback != SH_SCHOOL_ALT_CARD_CALLBACK &&
            callback != SH_BATHROOM_CALLBACK &&
            callback != SH_BOSS_CALLBACK &&
            callback != SH_BOSS_DESCENT_CALLBACK &&
            callback != SH_PHONE_CALLBACK &&
            callback != SH_ARRIVAL_CALLBACK &&
            !inspection(callback, overlay) && !alternative_pickup(callback)) { return false; }
    } else if (overlay == SH_MAP_IDX_MAP1_S01) {
        if (callback != -1 && callback > 2 && callback != SH_PIANO_CALLBACK && callback != s_school_acid.callback &&
            !inspection(callback, overlay) && !upper_pickup(callback)) {
            return false;
        }
    } else if (callback != -1 && callback > 2 && callback != 10 && callback != 11 && callback != 18 &&
          callback != 19 && callback != 20 && callback != 23 && callback != 22 && callback != 15 && callback != 16 &&
        !inspection(callback, overlay) && !medallion(callback)) {
        return false;
    }
    const bool preserve_roof = overlay == SH_MAP_IDX_MAP1_S03 && town->school.overlay == overlay;
    const ShDrainValve valve = town->school.valve;
    const bool valve_active = town->school.valve_active;
    const ShCharaBone hand = town->school.hand_world;
    const bool hand_valid = town->school.hand_world_valid;
    const ShPathPoint player_position = town->school.player_position;
    const ShLockerWorld locker = town->school.locker;
    const ShLockerClock locker_clock = town->school.locker_clock;
    const ShLockerFrame locker_frame = town->school.locker_frame;
    const bool locker_render_active = town->school.locker_render_active;
    const uint8_t locker_camera_mode = town->school.locker_camera_mode;
    const bool locker_camera_active = town->school.locker_camera_active;
    const ShVc locker_camera_vc = town->school.locker_camera_vc;
    const ShBathroomMotion bathroom = town->school.bathroom;
    const ShBossDoors boss_doors = town->school.boss_doors;
    const int32_t boss_box_y = town->school.boss_box_y;
    const bool preserve_bathroom = overlay == SH_MAP_IDX_MAP1_S02 && town->school.overlay == overlay;
    memset(&town->school, 0, sizeof(town->school));
    if (preserve_bathroom) { town->school.bathroom = bathroom; town->school.boss_doors = boss_doors; town->school.boss_box_y = boss_box_y; }
    if (preserve_roof) {
        town->school.valve = valve;
        town->school.valve_active = valve_active;
        town->school.locker = locker;
        town->school.locker_clock = locker_clock;
        town->school.locker_frame = locker_frame;
        town->school.locker_render_active = locker_render_active;
        town->school.locker_camera_mode = locker_camera_mode;
        town->school.locker_camera_active = locker_camera_active;
        town->school.locker_camera_vc = locker_camera_vc;
    }
    if (overlay == SH_MAP_IDX_MAP1_S03) {
        town->school.hand_world = hand;
        town->school.hand_world_valid = hand_valid;
        town->school.player_position = player_position;
    }
    town->school.overlay = overlay;
    town->school.callback = callback;
    town->school.ladder_point = event->point_idx;
    town->school.brightness = 128;
    town->school.message = callback == -1 ? event->event_param : 0;
    town->school.completion_flag = event->complete_flag;
    town->school.transition_flags = event->transition_flags;
    town->school.activation_type = event->activation_type;
    town->stage = SH_TOWN_SCRIPT;
    town->script = SH_TOWN_SCR_SCHOOL;
    town->step = 0;
    town->script_ticks = 0;
    town->controls_locked = true;
    town->aiming = false;
    town->subtitle = NULL;
    town->dialogue = NULL;
    int point_count;
    const ShMapPoint *points = sh_school_map_points(overlay, &point_count);
    if (event->point_idx < point_count) {
        const ShMapPoint *point = &points[event->point_idx];
        town->school.source_pos[0] = point->x;
        town->school.source_pos[1] = 1.2f;
        town->school.source_pos[2] = -point->z;
    }
    const ShSchoolPickup *pickup = overlay == SH_MAP_IDX_MAP1_S01 ? upper_pickup(callback) :
                                   overlay == SH_MAP_IDX_MAP1_S02 ? alternative_pickup(callback) : NULL;
    if (pickup) {
        town->pickup_active = true;
        town->pickup_item_id = pickup->item;
        town->pickup_scale = 0.0f;
        town->pickup_spin = 0.0f;
        town->pickup_msg = overlay == SH_MAP_IDX_MAP1_S02 ? s_school_alternative_messages[pickup->message].text :
                           s_school_upper_messages[pickup->message].text;
    }
    return true;
}

bool sh_school_take_player_warp(ShTown *town, float *x, float *z, float *yaw)
{
    if (!town || !town->school.player_warp) { return false; }
    town->school.player_warp = false;
    if (x) { *x = town->school.player_warp_position[0]; }
    if (z) { *z = town->school.player_warp_position[1]; }
    if (yaw) { *yaw = town->school.player_warp_yaw; }
    return true;
}

/* map1_s01.c func_800D7308. */
static void acid_update(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)
{
    sh_bgm_status_set(SH_BGM_STATUS_APPLY_MUTE);
    if (sh_input_down(input, KEY_START) && town->step > 0 && town->step < 8) {
        step_set(town, 8);
    }
    switch (town->step) {
    case 0:
        town->border.state = SH_BORDER_FADEINSTART;
        step_set(town, 1);
        /* fall through */
    case 1:
        step_set(town, 2);
        break;
    case 2:
        if (town->border.state == SH_BORDER_SHOWN) { step_set(town, 3); }
        break;
    case 3: {
        town->school.player_warp = true;
        memcpy(town->school.player_warp_position, s_school_acid.player, sizeof(s_school_acid.player));
        town->school.player_warp_yaw = s_school_acid.yaw;
        memcpy(town->cam.pos, s_school_acid.camera, sizeof(town->cam.pos));
        const float dx = s_school_acid.look[0] - town->cam.pos[0];
        const float dy = s_school_acid.look[1] - town->cam.pos[1];
        const float dz = s_school_acid.look[2] - town->cam.pos[2];
        town->cam.yaw = atan2f(-dx, -dz);
        town->cam.pitch = atan2f(dy, hypotf(dx, dz));
        town->cam.fov_deg = 53.13f;
        town->cam_active = true;
        town->script_use_cam = true;
        step_set(town, 4);
        /* fall through */
    }
    case 4:
        town->harry_state = s_school_acid.animation;
        town->harry_time = 0.0f;
        step_set(town, 5);
        /* fall through */
    case 5:
        town->script_ticks += SH_Q12(dt);
        if (town->script_ticks > SH_Q12(s_school_acid.animation_wait)) { step_set(town, 6); }
        break;
    case 6:
        sh_snd_at((uint16_t)s_school_acid.sound, s_school_acid.sound_pos, s_school_acid.volume);
        sh_smoke_begin_acid(&town->school.smoke);
        step_set(town, 7);
        /* fall through */
    case 7:
        town->script_ticks += SH_Q12(dt);
        if (town->script_ticks > SH_Q12(s_school_acid.smoke_wait)) { step_set(town, 8); }
        break;
    case 8:
        sh_snd_stop((uint16_t)s_school_acid.sound);
        if (fade_auto(town, 1.0f, 0.0f)) { step_set(town, 9); }
        break;
    case 9:
        sh_smoke_stop(&town->school.smoke);
        step_set(town, 10);
        /* fall through */
    default:
        town->border.state = SH_BORDER_FADEOUTSTART;
        town->cam_active = false;
        town->script_use_cam = false;
        town->harry_state = 0;
        if (town->fade >= 1.0f) { fade_to(town, 0.0f, 0.0f); }
        sh_save_event_set(save, (unsigned)s_school_acid.flag);
        finish(town);
        break;
    }
}

static void piano_camera(ShTown *town, unsigned index)
{
    memcpy(town->cam.pos, s_piano_camera[index], sizeof(town->cam.pos));
    const float dx = s_piano_look[index][0] - town->cam.pos[0];
    const float dy = s_piano_look[index][1] - town->cam.pos[1];
    const float dz = s_piano_look[index][2] - town->cam.pos[2];
    town->cam.yaw = atan2f(-dx, -dz);
    town->cam.pitch = atan2f(dy, hypotf(dx, dz));
    town->cam.fov_deg = 53.13f;
    town->cam_active = true;
    town->script_use_cam = true;
}

static void piano_wait(ShTown *town, float dt)
{
    town->script_ticks += SH_Q12(dt);
    if (s_piano_scene_waits[town->step] < town->script_ticks) { step_set(town, town->step + 1); }
}

/* MapEvent_PianoPuzzleInteract. */
void sh_school_piano_update(ShTown *town, ShSavegame *save, const ShInputState *input,
                             float dt, int stick_x, int stick_y, int32_t gravity)
{
    ShSchool *school = &town->school;
    sh_bgm_status_set(SH_BGM_STATUS_APPLY_MUTE);
    school->piano_result.draw_cursor = false;
    if (sh_input_down(input, KEY_START) && town->step >= 10 && town->step < 19) {
        town->fade_speed = 3.0f;
        step_set(town, 17);
        sh_silver_ground(&school->silver);
    }
    switch (town->step) {
    case 0:
        sh_silver_init(&school->silver);
        school->silver_active = false;
        step_set(town, 1);
        /* fall through */
    case 1:
        switch (school->substep) {
        case 0:
            fade_to(town, 1.0f, 2.5f);
            school->substep = 1;
            /* fall through */
        case 1:
            school->image = "PIANO1.TIM";
            school->substep = 2;
            break;
        default:
            if (fade_done(town)) { step_set(town, 2); }
            break;
        }
        break;
    case 2:
        fade_to(town, 0.0f, 2.0f);
        school->message = 27;
        step_set(town, 3);
        break;
    case 3:
        if (message(town, input)) { step_set(town, 4); }
        break;
    case 4:
        school->message = 29;
        town->subtitle = s_school_upper_messages[29].text;
        town->prompt_open = true;
        if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
            step_set(town, sh_input_down(input, SH_BTN_ACTION) ? 5 : 21);
            town->prompt_open = false;
            town->subtitle = NULL;
        }
        break;
    case 5:
        if (fade_auto(town, 1.0f, 1.5f)) { step_set(town, 6); }
        break;
    case 6:
        school->image = "PIANO2.TIM";
        step_set(town, 7);
        break;
    case 7:
        fade_to(town, 0.0f, 1.5f);
        sh_piano_init(&school->piano);
        step_set(town, 8);
        break;
    case 8:
        school->piano_result = sh_piano_update(&school->piano, stick_x, stick_y,
                                                sh_input_down(input, SH_BTN_ACTION),
                                                sh_input_down(input, SH_BTN_CANCEL), dt);
        if (school->piano_result.sound) { sh_snd_call(school->piano_result.sound); }
        if (school->piano_result.solved) { sh_save_event_set(save, SH_PIANO_SOLVED_FLAG); }
        if (school->piano_result.finished) { step_set(town, 9); }
        break;
    case 9:
        step_set(town, sh_save_event_get(save, SH_PIANO_SOLVED_FLAG) ? 10 : 19);
        break;
    case 10:
        town->border.state = SH_BORDER_FADEINSTART;
        school->silver_active = true;
        school->image = NULL;
        step_set(town, 11);
        /* fall through */
    case 11:
        school->player_warp = true;
        memcpy(school->player_warp_position, s_piano_player, sizeof(s_piano_player));
        school->player_warp_yaw = SH_PIANO_PLAYER_YAW;
        piano_wait(town, dt);
        piano_camera(town, 0);
        break;
    case 12:
        piano_wait(town, dt);
        sh_silver_swing(&school->silver, SH_Q12(dt));
        break;
    case 13:
        if (sh_silver_fall(&school->silver, SH_Q12(dt), gravity)) {
            sh_snd_call(SH_PIANO_FALL_SOUND);
            step_set(town, 14);
        }
        break;
    case 14:
        piano_wait(town, dt);
        break;
    case 15:
        piano_wait(town, dt);
        sh_silver_ground(&school->silver);
        piano_camera(town, 1);
        break;
    case 17:
        if (!school->fade_started) {
            town->border.state = SH_BORDER_FADEOUTSTART;
            school->fade_started = true;
        } else if (town->border.state == SH_BORDER_NONE) {
            step_set(town, 18);
        }
        break;
    case 18:
    case 20:
        step_set(town, -1);
        break;
    case 19:
    case 21:
    case 16:
        if (fade_auto(town, 1.0f, 2.0f)) { step_set(town, town->step + 1); }
        break;
    default:
        fade_to(town, 0.0f, 2.5f);
        school->silver_active = false;
        town->cam_active = false;
        town->script_use_cam = false;
        finish(town);
        break;
    }
}

static void upper_pickup_prompt(ShTown *town, ShSavegame *save, const ShSchoolPickup *pickup,
                                const ShInputState *input, float dt, int *step)
{
    sh_save_event_set(save, (unsigned)pickup->flag);
    if (town->pickup_scale < 1.0f) {
        town->pickup_scale = fminf(1.0f, town->pickup_scale + dt * 4.0f);
        return;
    }
    town->prompt_open = true;
    town->subtitle = town->pickup_msg;
    town->pickup_spin += 3.14159265358979323846f * dt;
    if (sh_input_down(input, SH_BTN_ACTION)) {
        *step = pickup->prompt_step + 1;
    } else if (sh_input_down(input, SH_BTN_CANCEL)) {
        *step = pickup->prompt_step + 2;
    }
}

static bool upper_animation_done(const ShTown *town)
{
    for (int i = 0; i < COUNT(s_school_upper_animations); i++) {
        const ShSchoolAnimation *animation = &s_school_upper_animations[i];
        if (animation->state == town->harry_state) {
            return town->harry_time * animation->fps >= animation->last - animation->first;
        }
    }
    return false;
}

/* map1_s01.c func_800D857C. */
#include "school_roof_pickup_steps.h"

static void silver_pickup_update(ShTown *town, ShSavegame *save, const ShSchoolPickup *pickup,
                                 const ShInputState *input, float dt)
{
    switch (town->step) {
    case 0:
        step_set(town, 1);
        /* fall through */
    case 1:
        step_set(town, 2);
        break;
    case 2:
        town->harry_state = 59;
        town->harry_time = 0.0f;
        if (town->school.overlay == SH_MAP_IDX_MAP1_S02) {
            sh_path_school_animation_start(&town->school.card_player, 59);
        }
        step_set(town, 3);
        break;
    case 3:
        if (town->school.overlay == SH_MAP_IDX_MAP1_S02) {
            ShAnimPoseTiming pose;
            sh_path_animation_timing(&town->school.card_player, SH_Q12(dt), 0, 0, &pose);
        }
        step_set(town, 4);
        break;
    case 4:
        if (town->school.overlay == SH_MAP_IDX_MAP1_S02) {
            ShAnimPoseTiming pose;
            sh_path_animation_timing(&town->school.card_player, SH_Q12(dt), 0, 0, &pose);
            if (sh_path_school_animation_done(&town->school.card_player)) { step_set(town, 5); }
        } else if (upper_animation_done(town)) { step_set(town, 5); }
        break;
    case 5:
        upper_pickup_prompt(town, save, pickup, input, dt, &town->step);
        break;
    case 6:
        sh_save_item_add(save, (uint8_t)pickup->item, town->school.overlay == SH_MAP_IDX_MAP1_S02 ?
                         alternative_pickup_count(pickup->callback) : 1);
        town->pickup_active = false;
        town->prompt_open = false;
        town->subtitle = NULL;
        step_set(town, 8);
        break;
    case 7:
        sh_save_event_clear(save, (unsigned)pickup->flag);
        town->pickup_active = false;
        town->prompt_open = false;
        town->subtitle = NULL;
        step_set(town, 8);
        /* fall through */
    case 8:
        switch (town->school.substep) {
        case 0:
            town->harry_state = 60;
            town->harry_time = 0.0f;
            if (town->school.overlay == SH_MAP_IDX_MAP1_S02) {
                sh_path_school_animation_start(&town->school.card_player, 60);
            }
            town->school.substep = 1;
            break;
        case 1:
            if (town->school.overlay == SH_MAP_IDX_MAP1_S02) {
                ShAnimPoseTiming pose;
                sh_path_animation_timing(&town->school.card_player, SH_Q12(dt), 0, 0, &pose);
                if (sh_path_school_animation_done(&town->school.card_player)) { town->school.substep = 2; }
            } else if (upper_animation_done(town)) { town->school.substep = 2; }
            break;
        default:
            step_set(town, 9);
            break;
        }
        break;
    default:
        town->harry_state = 0;
        finish(town);
        break;
    }
}

/* map1_s01.c GoldMedallionItemPickup / events_util.c Event_ItemTake for Chemical. */
static void upper_pickup_update(ShTown *town, ShSavegame *save, const ShSchoolPickup *pickup,
                                const ShInputState *input, float dt)
{
    if (pickup->prompt_step == 5) {
        silver_pickup_update(town, save, pickup, input, dt);
        return;
    }
    if (pickup->prompt_step == 2) {
        switch (town->school.substep) {
        case 0:
            town->school.substep = 1;
            /* fall through */
        case 1:
            town->school.substep = 2;
            break;
        case 2:
            upper_pickup_prompt(town, save, pickup, input, dt, &town->school.substep);
            break;
        case 3:
            sh_save_item_add(save, (uint8_t)pickup->item, 1);
            finish(town);
            break;
        default:
            sh_save_event_clear(save, (unsigned)pickup->flag);
            finish(town);
            break;
        }
        return;
    }
    switch (town->step) {
    case 0:
        step_set(town, 1);
        /* fall through */
    case 1:
        step_set(town, 2);
        break;
    case 2:
        step_set(town, 3);
        break;
    case 3:
        upper_pickup_prompt(town, save, pickup, input, dt, &town->step);
        break;
    case 4:
        sh_save_item_add(save, (uint8_t)pickup->item, 1);
        /* fall through */
    default:
        finish(town);
        break;
    case 5:
        sh_save_event_clear(save, (unsigned)pickup->flag);
        finish(town);
        break;
    }
}

/* events_util.c Event_PaperMapTake(6, EventFlag_M1S00_PickupMap, 37). */
static void map_take_update(ShTown *town, ShSavegame *save, const ShInputState *input)
{
    ShSchool *school = &town->school;
    switch (school->substep) {
    case 0:
        school->substep = 1;
        break;
    case 1:
        if (fade_auto(town, 1.0f, 0.0f)) {
            school->substep = 2;
        }
        break;
    case 2:
        school->image = SH_SCHOOL_MAP_IMAGE;
        fade_to(town, 0.0f, 0.0f);
        school->substep = 3;
        /* fall through */
    case 3:
        school->brightness = 88; /* Q8(11.0f / 32.0f). */
        town->prompt_open = true;
        town->subtitle = s_school_messages[SH_SCHOOL_MAP_MESSAGE].text;
        if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
            school->substep = sh_input_down(input, SH_BTN_ACTION) ? 4 : 5;
            town->prompt_open = false;
            town->subtitle = NULL;
        }
        break;
    case 4:
        save->paper_map_flags |= SH_SCHOOL_MAP_FLAGS;
        sh_save_event_set(save, SH_SCHOOL_MAP_FLAG);
        school->substep = 5;
        /* fall through */
    case 5:
        school->brightness = 88;
        if (fade_auto(town, 1.0f, 0.0f)) {
            school->substep = 6;
        }
        break;
    default:
        fade_to(town, 0.0f, 0.0f);
        finish(town);
        break;
    }
}

/* map1_s00.c MapEvent_Boiler0/1/2. PS1 controller vibration has no 3DS actuator. */
static void boiler_update(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)
{
    ShSchool *school = &town->school;
    const bool working = school->callback == 23;
    const bool start = school->callback == 18;
    switch (town->step) {
    case 0:
        step_set(town, 1);
        /* fall through */
    case 1:
        school->message = working ? 31 : 29;
        step_set(town, 2);
        break;
    case 2:
        if (working) {
            if (message(town, input)) {
                step_set(town, 3);
            }
        } else {
            town->prompt_open = true;
            town->subtitle = s_school_messages[29].text;
            if (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
                const bool yes = sh_input_down(input, SH_BTN_ACTION);
                town->prompt_open = false;
                town->subtitle = NULL;
                step_set(town, yes ? 3 : -1);
            }
        }
        break;
    case 3: {
        if (working) {
            finish(town);
            break;
        }
        const float pos[3] = {20.5f, -SH_Q12_TO_FLOAT(SH_Q12(-1.2f)), 17.5f};
        sh_snd_at(SH_SCHOOL_Sfx_Unk1420, pos, 0.5f);
        if (start) {
            sh_save_event_set(save, SH_SCHOOL_EventFlag_83);
        }
        step_set(town, 4);
        /* fall through */
    }
    case 4:
        town->script_ticks += SH_Q12(dt);
        if (town->script_ticks > SH_Q12(start ? 1.0f : 0.5f)) {
            school->message = 30;
            step_set(town, 5);
        }
        break;
    case 5:
        if (start) {
            school->boiler_ticks += SH_Q12(dt);
            school->boiler_glow = school->boiler_ticks % SH_Q12(0.8499f) < SH_Q12(0.45f);
            if (school->boiler_ticks > SH_Q12(3.5f)) {
                step_set(town, 6);
            }
        } else if (message(town, input)) {
            step_set(town, 6);
        }
        break;
    case 6: {
        if (!start) {
            finish(town);
            break;
        }
        const float pos[3] = {SH_Q12_TO_FLOAT(SH_Q12(18.8f)), -4.0f, 17.5f};
        sh_snd_at(SH_SCHOOL_Sfx_Unk1422, pos, 0.5f);
        step_set(town, 7);
        /* fall through */
    }
    case 7:
        sh_save_event_set(save, SH_SCHOOL_EventFlag_73);
        town->script_ticks += SH_Q12(dt);
        if (town->script_ticks > SH_Q12(1.5f)) {
            step_set(town, 8);
        }
        break;
    case 8:
        sh_save_event_set(save, SH_SCHOOL_EventFlag_84);
        step_set(town, -1);
        break;
    default:
        finish(town);
        break;
    }
}

/* events_util.c Event_BgTextureFadeIn. */
static void background_fade_in(ShTown *town, const char *image, float fade_out, float fade_in, int next)
{
    switch (town->school.substep) {
    case 0:
        fade_to(town, 1.0f, fade_out);
        town->school.substep = 1;
        /* fall through */
    case 1:
        town->school.substep = 2;
        break;
    case 2:
        if (fade_done(town)) {
            town->school.substep = 3;
        }
        break;
    default:
        town->school.image = image;
        if (fade_auto(town, 0.0f, fade_in)) {
            step_set(town, next);
        }
        break;
    }
}

/* events_util.c Event_BgTextureFadeOut. */
static void background_fade_out(ShTown *town, float fade_in, float fade_out, int next)
{
    if (town->school.substep == 0) {
        if (fade_auto(town, 1.0f, fade_out)) {
            town->school.substep = 1;
        }
        return;
    }
    town->school.image = NULL;
    fade_to(town, 0.0f, fade_in);
    step_set(town, next);
}

/* map1_s00.c MapEvent_ClockTowerInspect / func_800D8354. */
static void clock_or_locked_paper_update(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)
{
    ShSchool *school = &town->school;
    const bool clock = school->callback == 10;
    const int variant = sh_save_event_get(save, SH_SCHOOL_EventFlag_72) ? 2 :
                        sh_save_event_get(save, SH_SCHOOL_EventFlag_71) ? 1 : 0;
    switch (town->step) {
    case 0:
        if (!clock) {
            sh_snd_at(SH_SCHOOL_Sfx_DoorLocked, school->source_pos, 0.5f);
        }
        step_set(town, 1);
        /* fall through */
    case 1:
        town->script_ticks += SH_Q12(dt);
        if (!clock || town->script_ticks > SH_Q12(0.3f)) {
            school->message = clock ? 15 : SH_SCHOOL_MapMsgIdx_DoorLocked;
            step_set(town, 2);
        }
        break;
    case 2:
        if (message(town, input)) {
            step_set(town, 3);
        }
        break;
    case 3:
        background_fade_in(town, clock ? s_school_clocks[variant].image : "PAPER1.TIM", 3.0f, 2.0f, 4);
        school->message = s_school_clocks[variant].message;
        break;
    case 4:
        if (clock ? message(town, input) :
            (sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL))) {
            step_set(town, 5);
        }
        break;
    case 5:
        background_fade_out(town, 3.0f, 2.0f, 6);
        break;
    default:
        finish(town);
        break;
    }
}

/* map1_s00.c GoldenSunItemUse / SilverMoonItemUse, identical retail state sequences. */
static void medallion_update(ShTown *town, ShSavegame *save, const ShSchoolMedallion *data,
                             const ShInputState *input, float dt)
{
    ShSchool *school = &town->school;
    switch (town->step) {
    case 0:
        fade_to(town, 1.0f, 2.5f);
        step_set(town, 1);
        /* fall through */
    case 1:
        step_set(town, 2);
        break;
    case 2:
        if (fade_done(town)) {
            step_set(town, 3);
        }
        break;
    case 3:
        fade_to(town, 0.0f, 2.0f);
        step_set(town, 4);
        /* fall through */
    case 4:
        school->image = data->from;
        step_set(town, 5);
        break;
    case 5:
        if (fade_done(town)) {
            step_set(town, 6);
        }
        break;
    case 6:
        town->script_ticks += SH_Q12(dt);
        if (town->script_ticks > SH_Q12(0.5f)) {
            step_set(town, 7);
        }
        break;
    case 7:
        sh_snd_call(data->sounds[0]);
        step_set(town, 8);
        /* fall through */
    case 8:
        school->next_image = data->to;
        town->bg_transition = SH_Q12_TO_FLOAT(town->script_ticks);
        town->script_ticks += SH_Q12(dt * 30.0f / 16.0f);
        if (town->script_ticks > SH_Q12_ONE) {
            step_set(town, 9);
        }
        break;
    case 9:
        sh_snd_call(data->sounds[1]);
        step_set(town, 10);
        /* fall through */
    case 10:
        school->image = data->to;
        school->next_image = NULL;
        town->script_ticks += SH_Q12(dt);
        if (town->script_ticks > SH_Q12(1.0f)) {
            school->message = data->message;
            step_set(town, 11);
        }
        break;
    case 11:
        if (message(town, input)) {
            step_set(town, 12);
        }
        break;
    case 12:
        if (fade_auto(town, 1.0f, 2.0f)) {
            step_set(town, 13);
        }
        break;
    default:
        fade_to(town, 0.0f, 2.5f);
        sh_save_event_set(save, (unsigned)data->flag);
        sh_save_event_set(save, (unsigned)data->marker);
        finish(town);
        break;
    }
}

/* events_util.c Event_DisplayMapMsgWithBg / Event_DisplayMapMsgWithDimmedBg. */
static void inspection_update(ShTown *town, const ShSchoolInspection *data, const ShInputState *input)
{
    ShSchool *school = &town->school;
    switch (school->substep) {
    case 0:
        fade_to(town, 1.0f, data->fade_out);
        school->substep = 1;
        /* fall through */
    case 1:
        school->substep = 2;
        break;
    case 2:
        if (fade_done(town)) {
            school->substep = 3;
        }
        break;
    case 3:
        school->image = data->image;
        school->brightness = data->brightness;
        if (fade_auto(town, 0.0f, data->fade_in)) {
            school->message = data->message;
            school->substep = 4;
        }
        break;
    case 4:
        if (message(town, input)) {
            school->substep = 5;
            if (data->dim_message >= 0) {
                school->message = data->dim_message;
            }
        }
        break;
    case 5:
        if (data->dim_message >= 0) {
            school->brightness = 48; /* events_util.c: Q8(6.0f / 32.0f). */
            if (message(town, input)) {
                school->substep = 6;
            }
        } else if (fade_auto(town, 1.0f, data->fade_in)) {
            school->substep = 6;
        }
        break;
    case 6:
        if (data->dim_message >= 0) {
            if (fade_auto(town, 1.0f, data->fade_in)) {
                school->substep = 7;
            }
            break;
        }
        /* fall through */
    default:
        fade_to(town, 0.0f, data->fade_out);
        finish(town);
        break;
    }
}

/* map1_s02.c func_800DA8F8. */
static void alternative_tower_update(ShTown *town, const ShInputState *input)
{
    switch (town->step) {
    case 0:
        town->controls_locked = true;
        step_set(town, 1);
        /* fall through */
    case 1:
        step_set(town, 2);
        break;
    case 2:
        town->school.message = SH_SCHOOL_ALT_TOWER_MESSAGE;
        if (message(town, input)) { step_set(town, 3); }
        break;
    default:
        finish(town);
        break;
    }
}

/* map1_s02.c MapEvent_DoorWithHorizontalSlotInteract. */
static void alternative_slot_update(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)
{
    ShSchool *school = &town->school;
    switch (town->step) {
    case 0:
        town->controls_locked = true;
        fade_to(town, 1.0f, 0.0f);
        school->image = SH_SCHOOL_ALT_SLOT_IMAGE1;
        town->bg_transition = 0.0f;
        step_set(town, 1);
        /* fall through */
    case 1:
        step_set(town, 2);
        break;
    case 2:
        if (fade_done(town)) { step_set(town, 3); }
        break;
    case 3:
        school->next_image = SH_SCHOOL_ALT_SLOT_IMAGE2;
        step_set(town, 4);
        /* fall through */
    case 4:
        if (fade_auto(town, 0.0f, 0.0f)) {
            school->message = sh_save_event_get(save, SH_SCHOOL_ALT_SLOT_FLAG) ?
                              SH_SCHOOL_ALT_SLOT_SEEN_MESSAGE : SH_SCHOOL_ALT_SLOT_NEW_MESSAGE;
            step_set(town, 5);
        }
        break;
    case 5:
        if (message(town, input)) { step_set(town, 6); }
        break;
    case 6:
        school->message = SH_SCHOOL_ALT_SLOT_DETAIL_MESSAGE;
        step_set(town, 7);
        break;
    case 7:
        if (sh_save_event_get(save, SH_SCHOOL_ALT_SLOT_FLAG) || message(town, input)) {
            step_set(town, 8);
        }
        /* fall through */
    case 8:
        town->bg_transition += SH_SCHOOL_ALT_SLOT_BLEND_STEP * 30.0f * dt;
        if (town->bg_transition > 1.0f) {
            if (town->step == 8) {
                school->message = SH_SCHOOL_ALT_SLOT_SLOT_MESSAGE;
                step_set(town, 9);
                break;
            }
            town->bg_transition = 1.0f;
        }
        break;
    case 9:
        school->image = SH_SCHOOL_ALT_SLOT_IMAGE2;
        school->next_image = NULL;
        if (message(town, input)) { step_set(town, 10); }
        break;
    case 10:
        if (fade_auto(town, 1.0f, 0.0f)) { step_set(town, 11); }
        break;
    default:
        fade_to(town, 0.0f, 0.0f);
        sh_save_event_set(save, SH_SCHOOL_ALT_SLOT_FLAG);
        finish(town);
        break;
    }
}

void sh_school_update(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)
{
    ShSchool *school = &town->school;
    if (school->overlay == SH_MAP_IDX_MAP1_S02 && (school->callback == SH_PHONE_CALLBACK || school->callback == SH_ARRIVAL_CALLBACK)) {
        phone_update(town, save, input, dt);
        return;
    }
    if (school->overlay == SH_MAP_IDX_MAP1_S02 && school->callback == SH_BOSS_DESCENT_CALLBACK) {
        if (town->harry_state == 106) {
            ShAnimPoseTiming pose;
            sh_path_animation_timing(&school->card_player, SH_Q12(dt), 0, 0, &pose);
        }
        boss_descent(town, save, dt);
        return;
    }
    if (school->overlay == SH_MAP_IDX_MAP1_S02 && school->callback == SH_BOSS_CALLBACK) {
        if (school->transition_flags && school->completion_flag > 0) { sh_save_event_set(save, school->completion_flag); }
        boss_scene(town, save, input, dt);
        return;
    }
    if (school->overlay == SH_MAP_IDX_MAP1_S02 && school->callback == SH_BATHROOM_CALLBACK) {
        bathroom_scene(town, save, dt);
        return;
    }
    const ShSchoolRoofKey *roof_key = school->overlay == SH_MAP_IDX_MAP1_S03 ? roof_key_use(school->callback) : NULL;
    if (roof_key) {
        /* SysState_EventCallback_Update sets transition-mark flags before the callback. */
        if (school->transition_flags && school->completion_flag > 0) { sh_save_event_set(save, school->completion_flag); }
        sh_save_item_remove_one(save, roof_key->item);
    }
    if (school->overlay == SH_MAP_IDX_MAP1_S03) {
        if (school->callback == SH_SCHOOL_LOCKER_OPEN_CALLBACK) {
            if (town->harry_state == 86 && town->step >= 8 && town->step < 13) {
                ShAnimPoseTiming pose;
                sh_path_animation_timing(&school->card_player, SH_Q12(dt), 0, 0, &pose);
            }
            locker_open_steps(town, save, input, dt);
            return;
        }
        if (school->callback == SH_SCHOOL_LOCKER_BODY_CALLBACK) {
            locker_body_steps(town, save, dt);
            return;
        }
        const ShSchoolInspection *roof_inspection = inspection(school->callback, school->overlay);
        if (roof_inspection) { inspection_update(town, roof_inspection, input); return; }
        if (school->callback == SH_SCHOOL_ROOF_KEY_PICKUP_CALLBACK) {
            roof_library_key_pickup(town, save, input, dt);
            return;
        }
        if (school->callback == SH_SCHOOL_ROOF_VALVE_CALLBACK) {
            if (!town->step) { sh_roof_water_init(&s_roof_water); }
            roof_valve_steps(town, save, input, dt);
            roof_valve_hand_update(town);
            roof_valve_loop_update(town);
            return;
        }
        if (school->callback == SH_SCHOOL_ROOF_DRAIN0_CALLBACK) {
            roof_drain_inspect0(town, save, input, dt);
            return;
        }
        if (school->callback == SH_SCHOOL_ROOF_DRAIN1_CALLBACK) {
            roof_drain_inspect1(town, save, input, dt);
            return;
        }
        if (school->callback == SH_SCHOOL_ROOF_BALL_CALLBACK) {
            roof_rubber_ball(town, save, input, dt);
            return;
        }
        if (school->callback == SH_SCHOOL_ROOF_MONSTER_CALLBACK) {
            roof_monster_book(town, save, input, dt);
            return;
        }
        if (school->callback == SH_SCHOOL_ROOF_FAIRY_CALLBACK) {
            roof_fairy_book(town, save, input, dt);
            return;
        }
    }
    if (school->overlay == SH_MAP_IDX_MAP1_S02 && school->callback == SH_SCHOOL_ALT_CARD_CALLBACK) {
        alternative_card_update(town, save, input, dt);
        return;
    }
    if (school->overlay == SH_MAP_IDX_MAP1_S02 && school->callback == SH_SCHOOL_ALT_SLOT_CALLBACK) {
        alternative_slot_update(town, save, input, dt);
        return;
    }
    if (school->overlay == SH_MAP_IDX_MAP1_S02 && school->callback == SH_SCHOOL_ALT_TOWER_CALLBACK) {
        alternative_tower_update(town, input);
        return;
    }
    if (school->overlay == SH_MAP_IDX_MAP1_S02) {
        if (school->callback == SH_SCHOOL_ALT_LOBBY_CALLBACK) {
            sh_save_item_remove_one(save, SH_SCHOOL_ALT_LOBBY_ITEM);
        }
        const ShSchoolPickup *pickup = alternative_pickup(school->callback);
        if (pickup) {
            upper_pickup_update(town, save, pickup, input, dt);
            return;
        }
        const ShSchoolInspection *data = inspection(school->callback, school->overlay);
        if (data) {
            for (int i = 0; i < COUNT(s_school_alternative_inspection_flags); i++) {
                if (s_school_alternative_inspection_flags[i].callback == school->callback) {
                    sh_save_event_set(save, (unsigned)s_school_alternative_inspection_flags[i].flag);
                }
            }
            inspection_update(town, data, input);
            return;
        }
    }
    if (school->overlay == SH_MAP_IDX_MAP1_S00 && (school->callback == 15 || school->callback == 16)) {
        if (school->callback == 15) { ladder_exit_update(town, save, input, dt); }
        else { ladder_enter_update(town, save, input, dt); }
        return;
    }
    /* SysState_EventCallback_Update sets this before each flagged callback invocation. */
    if (school->callback != -1 && school->transition_flags && school->completion_flag > 0) {
        sh_save_event_set(save, (unsigned)school->completion_flag);
    }
    if (school->overlay == SH_MAP_IDX_MAP1_S01) {
        if (school->callback == SH_PIANO_CALLBACK) {
            const int x = sh_piano_axis(input->circle_x, (input->held & KEY_DLEFT) != 0, (input->held & KEY_DRIGHT) != 0);
            const int y = sh_piano_axis(-input->circle_y, (input->held & KEY_DUP) != 0, (input->held & KEY_DDOWN) != 0);
            sh_school_piano_update(town, save, input, dt, x, y, sh_piano_gravity(dt));
            return;
        }
        if (school->callback == s_school_acid.callback) {
            acid_update(town, save, input, dt);
            return;
        }
        const ShSchoolPickup *pickup = upper_pickup(school->callback);
        if (pickup) {
            upper_pickup_update(town, save, pickup, input, dt);
            return;
        }
    }
    if (school->callback == 20) {
        map_take_update(town, save, input);
        return;
    }
    if (school->callback == 18 || school->callback == 19 || school->callback == 23) {
        boiler_update(town, save, input, dt);
        return;
    }
    if (school->overlay != SH_MAP_IDX_MAP1_S01 && (school->callback == 10 || school->callback == 11)) {
        clock_or_locked_paper_update(town, save, input, dt);
        return;
    }
    const ShSchoolMedallion *use = medallion(school->callback);
    if (use) {
        medallion_update(town, save, use, input, dt);
        return;
    }
    const ShSchoolInspection *data = inspection(school->callback, school->overlay);
    if (data) {
        inspection_update(town, data, input);
        return;
    }
    if (school->callback == -1) { /* SysState_ReadMessage_Update */
        if (message(town, input)) {
            if (school->completion_flag > 0) {
                sh_save_event_set(save, (unsigned)school->completion_flag);
            }
            finish(town);
        }
        return;
    }
    if (school->callback == 22) { /* func_800D928C */
        finish(town);
        return;
    }
    /* Event_DisplayMapMsgWithSfx, shared MapEvent_Door* wrappers. */
    switch (school->substep) {
    case 0: {
        int sound, msg;
        if (school->callback == 0) {
            sound = SH_SCHOOL_Sfx_DoorJammed;
            msg = SH_SCHOOL_MapMsgIdx_DoorJammed;
        } else if (school->callback == 1) {
            sound = SH_SCHOOL_Sfx_DoorLocked;
            msg = SH_SCHOOL_MapMsgIdx_DoorLocked;
        } else {
            sound = SH_SCHOOL_Sfx_DoorUnlocked;
            msg = SH_SCHOOL_MapMsgIdx_DoorUnlocked;
        }
        if (school->overlay == SH_MAP_IDX_MAP1_S02 && school->callback == SH_SCHOOL_ALT_LOBBY_CALLBACK) {
            sound = SH_SCHOOL_ALT_LOBBY_SOUND;
            msg = SH_SCHOOL_ALT_LOBBY_MESSAGE;
        }
        if (roof_key) { sound = roof_key->sound; msg = roof_key->message; }
        sh_snd_at(sound, school->source_pos, 0.5f);
        school->message = msg;
        school->substep = 1;
        /* fall through */
    }
    case 1:
        town->script_ticks += SH_Q12(dt);
        if (town->script_ticks > SH_Q12(0.2f)) {
            school->substep = 2;
        }
        break;
    case 2:
        if (message(town, input)) {
            school->substep = 3;
        }
        break;
    default:
        finish(town);
        break;
    }
    if (roof_key && roof_key->flag > 0) { sh_save_event_set(save, roof_key->flag); }
}

const char *sh_school_background(const ShTown *town)
{
    return town->script == SH_TOWN_SCR_SCHOOL ? town->school.image : NULL;
}

const char *sh_school_background_next(const ShTown *town, float *alpha)
{
    if (town->script == SH_TOWN_SCR_SCHOOL && town->school.overlay == SH_MAP_IDX_MAP1_S02 &&
        town->school.callback == SH_SCHOOL_ALT_SLOT_CALLBACK && town->step >= 7 && town->step <= 9) {
        if (alpha) { *alpha = fminf(1.0f, town->bg_transition); }
        return town->school.next_image;
    }
    if (town->script != SH_TOWN_SCR_SCHOOL || town->school.overlay != SH_MAP_IDX_MAP1_S00 ||
        !medallion(town->school.callback) ||
        (town->step != 8 && town->step != 9)) {
        return NULL;
    }
    if (alpha) {
        *alpha = town->bg_transition;
    }
    return town->school.next_image;
}

const ShSchoolProp *sh_school_props(int *count)
{
    *count = COUNT(s_school_boiler_props);
    return s_school_boiler_props;
}

bool sh_school_harry_keyframe(const ShTown *town, int *keyframe)
{
    if (town && town->stage == SH_TOWN_SCRIPT && town->script == SH_TOWN_SCR_SCHOOL &&
        (town->school.overlay == SH_MAP_IDX_MAP1_S02 || town->school.overlay == SH_MAP_IDX_MAP1_S03) &&
        (precise_school_callback(town->school.overlay, town->school.callback) ||
         (town->school.overlay == SH_MAP_IDX_MAP1_S03 && town->school.callback == SH_SCHOOL_ROOF_VALVE_CALLBACK)) &&
        town->school.card_player.model.state_step && town->school.card_player.state == town->harry_state) {
        if (keyframe) { *keyframe = town->school.card_player.model.keyframe; }
        return true;
    }
    if (!town || town->stage != SH_TOWN_SCRIPT || town->script != SH_TOWN_SCR_SCHOOL ||
        (town->school.overlay != SH_MAP_IDX_MAP1_S01 && town->school.overlay != SH_MAP_IDX_MAP1_S00 &&
         town->school.overlay != SH_MAP_IDX_MAP1_S02)) {
        return false;
    }
    const bool ladder = town->school.overlay == SH_MAP_IDX_MAP1_S00;
    const ShSchoolAnimation *animations = ladder ? s_school_ladder_animations : s_school_upper_animations;
    int count = ladder ? COUNT(s_school_ladder_animations) : COUNT(s_school_upper_animations);
    if (town->school.overlay == SH_MAP_IDX_MAP1_S02) {
        animations = s_school_arrival_animations;
        count = COUNT(s_school_arrival_animations);
    }
    for (int i = 0; i < count; i++) {
        const ShSchoolAnimation *anim = &animations[i];
        if (anim->state == town->harry_state) {
            const int frame = anim->first + (int)floorf(town->harry_time * anim->fps);
            if (keyframe) {
                *keyframe = ladder ? anim->first + (frame - anim->first) % (anim->last - anim->first + 1) :
                            anim->fps < 0.0f ? (frame > anim->last ? frame : anim->last) :
                            frame < anim->last ? frame : anim->last;
            }
            return true;
        }
    }
    return false;
}

void sh_school_seed_harry_keyframe(ShTown *town, int keyframe)
{
    if (town->stage != SH_TOWN_SCRIPT || town->script != SH_TOWN_SCR_SCHOOL || town->step != 0 ||
        (town->school.overlay != SH_MAP_IDX_MAP1_S02 && town->school.overlay != SH_MAP_IDX_MAP1_S03) ||
        !(precise_school_callback(town->school.overlay, town->school.callback) ||
          (town->school.overlay == SH_MAP_IDX_MAP1_S03 && town->school.callback == SH_SCHOOL_ROOF_VALVE_CALLBACK))) {
        return;
    }
    town->school.card_player.model.keyframe = town->school.card_player.extra_model.keyframe = keyframe;
    town->school.card_player.model.time = town->school.card_player.extra_model.time = keyframe * 4096;
}

const ShSchoolProp *sh_school_map_props(uint8_t map, int *count)
{
    if (map == SH_MAP_IDX_MAP1_S02) {
        *count = COUNT(s_school_bathroom_props);
        return s_school_bathroom_props;
    }
    if (map == SH_MAP_IDX_MAP1_S03) {
        *count = COUNT(s_school_roof_props);
        return s_school_roof_props;
    }
    if (map == 4) {
        *count = COUNT(s_school_upper_props);
        return s_school_upper_props;
    }
    return sh_school_props(count);
}

void sh_school_seed_harry_hand(ShTown *town, const ShCharaBone *hand)
{
    town->school.hand_world_valid = hand != NULL;
    if (hand) { town->school.hand_world = *hand; }
}

void sh_school_seed_harry_position(ShTown *town, float x, float y, float z)
{
    town->school.player_position = (ShPathPoint){SH_Q12(x), -SH_Q12(y), -SH_Q12(z)};
}

/* game.h PLAYER_IN_MAP_CHUNK, with signed division truncating toward zero. */
static bool in_chunk(int32_t value, int a, int b, int c, int d)
{
    const int index = value / SH_Q12(40.0f);
    return value > 0 ? index + a == b : index + c == d;
}

uint32_t sh_school_prop_mask(const ShTown *town, const ShSavegame *save, float x, float z)
{
    if (save->map_idx == SH_MAP_IDX_MAP1_S02) { return bathroom_prop_mask(save, x, z) | boss_prop_mask(x, z) | phone_prop_mask(save, x, z); }
    if (save->map_idx == SH_MAP_IDX_MAP1_S03) { return roof_prop_mask(save, x, z, town->school.locker_frame.draw_mask); }
    uint32_t mask = 0;
    if (save->map_idx == 4) {
        if (in_chunk(SH_Q12(x), 1, -2, -1, -2) && in_chunk(SH_Q12(-z), 1, 3, -1, 3)) {
            if (sh_save_event_get(save, 74)) {
                mask |= s_school_upper_props[1].mask;
                if (!sh_save_event_get(save, 69)) {
                    mask |= s_school_upper_props[2].mask;
                }
            } else {
                mask |= s_school_upper_props[0].mask;
            }
        }
        if (in_chunk(SH_Q12(x), 1, -3, -1, -3) && in_chunk(SH_Q12(-z), 1, 3, -1, 3) &&
            !sh_save_event_get(save, 68)) {
            mask |= s_school_upper_props[3].mask;
        }
        /* map1_s01.c Map_WorldObjectsUpdate: piano lid and silver medallion. */
        if (in_chunk(SH_Q12(x), 1, -3, -1, -3) && in_chunk(SH_Q12(-z), 0, 0, -1, 1)) {
            mask |= s_school_upper_props[sh_save_event_get(save, 71) ? 4 : 5].mask;
            if (!sh_save_event_get(save, 70)) {
                mask |= s_school_upper_props[!town->school.silver_active && sh_save_event_get(save, 75) ? 7 : 6].mask;
            }
        }
        return mask;
    }
    if (in_chunk(SH_Q12(x), 1, 3, -1, 3) && in_chunk(SH_Q12(-z), 0, 0, -1, 1) &&
        !sh_save_event_get(save, SH_SCHOOL_MAP_FLAG)) {
        mask |= s_school_boiler_props[3].mask;
    }
    if (!in_chunk(SH_Q12(x), 0, 0, -1, 1) || !in_chunk(SH_Q12(-z), 1, -1, 0, 0)) {
        return mask;
    }
    mask |= sh_save_event_get(save, SH_SCHOOL_EventFlag_72) ?
                    s_school_boiler_props[2].mask : s_school_boiler_props[1].mask;
    if (sh_save_event_get(save, SH_SCHOOL_EventFlag_73) ||
        (town->stage == SH_TOWN_SCRIPT && town->script == SH_TOWN_SCR_SCHOOL &&
         town->school.callback == 18 && town->school.boiler_glow)) {
        mask |= s_school_boiler_props[0].mask;
    }
    return mask;
}

uint32_t sh_school_roof_moving_mask(const ShTown *town, const ShSavegame *save, float x, float z)
{
    if (save->map_idx == SH_MAP_IDX_MAP1_S02) {
        return (bathroom_prop_mask(save, x, z) & s_school_bathroom_props[0].mask) |
            (boss_prop_mask(x, z) & (s_school_bathroom_props[4].mask | s_school_bathroom_props[5].mask | s_school_bathroom_props[6].mask));
    }
    if (save->map_idx != SH_MAP_IDX_MAP1_S03) { return 0; }
    uint32_t moving = town->school.valve_active ? s_school_roof_props[0].mask : 0;
    if (town->school.locker_render_active) {
        for (int door = 0; door < 3; door++) { moving |= s_school_roof_props[door + 2].mask; }
    }
    return moving & sh_school_prop_mask(town, save, x, z);
}

int16_t sh_school_bathroom_rotation(const ShTown *town, const ShSavegame *save)
{
    return sh_save_event_get(save, SH_BATHROOM_OPEN_FLAG) ? SH_BATHROOM_OPEN_ROTATION : town->school.bathroom.rotation;
}

/* MAP1_S02 Map_WorldObjectsUpdate / Collision_FlagBitsClear retains Map. */
int sh_school_collision_flags(const ShTown *town, const ShSavegame *save, float x, float z)
{
    if (save->map_idx == SH_MAP_IDX_MAP1_S02 && boss_room(x, z)) {
        return boss_collision_flags(&town->school.boss_doors);
    }
    if (save->map_idx != SH_MAP_IDX_MAP1_S02 || !bathroom_prop_mask(save, x, z)) { return -1; }
    return SH_BATHROOM_COLLISION_MAP |
        (sh_save_event_get(save, SH_BATHROOM_OPEN_FLAG) ? 0 : SH_BATHROOM_COLLISION_OBJECTS);
}

static void locker_commands(const ShLockerFrame *frame, void *context)
{
    ShTown *town = context;
    for (unsigned i = 0; i < frame->command_count; i++) {
        const ShLockerCommand *command = &frame->commands[i];
        /* Original requests controller vibration; Old 3DS has no rumble actuator. */
        if (command->effect) { continue; }
        const float position[3] = {SH_Q12_TO_FLOAT(command->position.vx),
            -SH_Q12_TO_FLOAT(command->position.vy), -SH_Q12_TO_FLOAT(command->position.vz)};
        if (command->flags & SH_LOCKER_SFX_NO_DIST_ATTEN) {
            sh_snd_play((uint16_t)command->id, sh_snd_camera_balance(&town->cam, position), (uint8_t)~command->volume);
        } else {
            sh_snd_at((uint16_t)command->id, position, command->volume / 256.0f);
        }
    }
}

typedef struct ShLockerCameraTarget {
    float position[3], look[3];
    bool set, warp_position, warp_look;
} ShLockerCameraTarget;

static void locker_camera_target(ShLockerCameraTarget *target, bool look, int32_t x, int32_t y, int32_t z, bool warp)
{
    float *position = look ? target->look : target->position;
    position[0] = SH_Q12_TO_FLOAT(x);
    position[1] = SH_Q12_TO_FLOAT(y);
    position[2] = SH_Q12_TO_FLOAT(z);
    if (look) { target->warp_look = warp; } else { target->warp_position = warp; }
    target->set = true;
}

static void locker_camera_return(ShTown *town, int warp)
{
    if (warp) { sh_vc_warp(&town->vc); }
    town->school.locker_camera_active = false;
    town->script_use_cam = false;
}

static void locker_camera_apply(ShTown *town, const ShLockerCameraTarget *target, const ShVcScriptParams *params, float dt)
{
    ShVc *vc = &town->school.locker_camera_vc;
    if (!town->school.locker_camera_active) {
        memset(vc, 0, sizeof(*vc));
        vc->cam_pos[0] = town->cam.pos[0];
        vc->cam_pos[1] = -town->cam.pos[1];
        vc->cam_pos[2] = -town->cam.pos[2];
        vc->ang[0] = town->cam.pitch;
        vc->ang[1] = -town->cam.yaw;
    }
    sh_vc_script_update(vc, target->position, target->look, params,
        target->warp_position, target->warp_look, dt, &town->cam);
    town->school.locker_camera_active = true;
    town->cam_active = town->script_use_cam = true;
}

static void bathroom_camera_frame(ShTown *town, const ShBathroomFrame *frame, float dt)
{
    ShLockerCameraTarget target = {0};
    locker_camera_target(&target, false, frame->position.vx, frame->position.vy, frame->position.vz, false);
    locker_camera_target(&target, true, frame->look.vx, frame->look.vy, frame->look.vz, false);
    locker_camera_apply(town, &target, &frame->params, dt);
}

#include "school_locker_camera.h"

/* map1_s00/map1_s03 Map_WorldObjectsUpdate. */
void sh_school_world_update(ShTown *town, ShSavegame *save, float x, float z, float dt)
{
    if (save->map_idx == SH_MAP_IDX_MAP1_S02) {
        boss_world_update(&town->school.boss_doors, save, x, z);
        return;
    }
    if (save->map_idx == SH_MAP_IDX_MAP1_S03) {
        if (roof_locker_in_room(x, z)) {
            town->school.locker_render_active = true;
            sh_locker_world_advance(&town->school.locker, save, &town->school.locker_clock,
                dt, town->fade >= 1.0f, &town->school.locker_frame, locker_commands, town);
        }
        locker_room_camera(town, save, x, z, dt);
        return;
    }
    const int32_t px = SH_Q12(x), pz = SH_Q12(-z);
    if (in_chunk(px, 0, 0, -1, 1) && in_chunk(pz, 1, -1, 0, 0)) {
        if (sh_save_event_get(save, SH_SCHOOL_EventFlag_72)) {
            if (!town->school_boiler_volume) {
                sh_snd_call(SH_SCHOOL_Sfx_Unk1430);
            }
            if (sh_save_event_get(save, SH_SCHOOL_EventFlag_84)) {
                town->school_boiler_volume = SH_Q12_ONE;
            } else if (sh_save_event_get(save, SH_SCHOOL_EventFlag_83)) {
                town->school_boiler_volume += sh_q12_mul(SH_Q12(dt), SH_Q12(0.15f));
                if (town->school_boiler_volume > SH_Q12_ONE) {
                    town->school_boiler_volume = SH_Q12_ONE;
                }
            } else {
                town->school_boiler_volume = SH_Q12(0.0999f);
            }
            const float pos[3] = {SH_Q12_TO_FLOAT(SH_Q12(18.8f)), 1.0f, 17.5f};
            sh_snd_falloff(SH_SCHOOL_Sfx_Unk1430, pos, town->school_boiler_volume >> 5, 24.0f, 0);
        }
    } else if (in_chunk(px, 1, 2, -1, 2) && in_chunk(pz, 1, -1, 0, 0)) {
        if (sh_save_event_get(save, SH_SCHOOL_EventFlag_84)) {
            if (!town->school_boiler_volume) {
                sh_snd_call(SH_SCHOOL_Sfx_Unk1430);
            }
            town->school_boiler_volume = SH_Q12(0.2f);
            const float pos[3] = {52.0f, 1.0f, 16.0f};
            sh_snd_falloff(SH_SCHOOL_Sfx_Unk1430, pos, 25, 16.0f, 0);
        }
    } else {
        if (town->school_boiler_volume) {
            sh_snd_stop(SH_SCHOOL_Sfx_Unk1430);
        }
        town->school_boiler_volume = 0;
    }
}
