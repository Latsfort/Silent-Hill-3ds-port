#include "../source/game/town.c"
#include "sh3ds/inventory.h"
#include "sh3ds/event_image.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned sounds;
static unsigned valve_squeaks, valve_clicks, valve_water_starts, valve_water_stops;
static unsigned valve_volume_updates;
static unsigned roof_key_sounds;
static unsigned locker_body_sounds, locker_door_sounds;
static unsigned bathroom_sounds;
static unsigned boss_clicks, boss_loop_starts, boss_loop_stops, boss_volume_updates;
static unsigned descent_starts, descent_stops, descent_updates;
static unsigned phone_rings, phone_voices, phone_pickups, phone_returns, phone_preloads;
static unsigned phone_animation_pickups, phone_footsteps;
static unsigned arrival_voices, arrival_footsteps, bgm_flags;
static bool voice_pending;

unsigned sh_bgm_status(void) { return bgm_flags; }
uint8_t sh_sound_streaming(void) { const uint8_t state = voice_pending ? 4 : 0; voice_pending = false; return state; }
void sh_xa_preload(uint16_t command) { assert(command == 4150); phone_preloads++; }
void sh_snd_at_pitch(uint16_t sound, const float position[3], float volume, int pitch)
{
    assert(sound == 1311 && volume == 0.25f && pitch >= -16 && pitch < 16);
    if (position[0] < 0) { assert(position[0] > -20 && position[2] > 60); arrival_footsteps++; return; }
    assert(position[0] > 20 && position[2] < -140);
    phone_footsteps++;
}

void sh_vc_camera_target(ShVc *vc, const float position[3], const ShVcScriptParams *params, bool warp)
{
    assert(!warp && params && params->camera[0] == 2.5f && params->camera[1] == 0.75f);
    assert(params->camera[2] == 1.5f && params->camera[3] == 0.5f);
    assert(position[0] == 61.0f && position[1] == SH_Q12_TO_FLOAT(SH_Q12(-1.2f)) && position[2] == -60.0f);
    vc->user_camera = true;
}

void sh_vc_camera_release(ShVc *vc, bool warp)
{
    assert(!warp);
    vc->user_camera = false;
}
static float roof_key_sound_position[3];
static int valve_attenuation;
static unsigned locker_camera_calls;
static float locker_camera_position[3], locker_camera_look[3];
static bool locker_camera_warp_position, locker_camera_warp_look;

void sh_vc_script_update(ShVc *vc, const float position[3], const float look[3],
    const ShVcScriptParams *params, bool warp_camera, bool warp_watch, float dt, ShCamera *out)
{
    (void)vc; (void)dt;
    if (params && params->watch[0]) { assert(params->watch[0] == 3.0f && params->watch[2] == SH_Q12_TO_FLOAT(SH_Q12(0.3f))); }
    memcpy(locker_camera_position, position, sizeof(locker_camera_position));
    memcpy(locker_camera_look, look, sizeof(locker_camera_look));
    locker_camera_warp_position = warp_camera;
    locker_camera_warp_look = warp_watch;
    locker_camera_calls++;
    out->pos[0] = position[0]; out->pos[1] = -position[1]; out->pos[2] = -position[2];
    out->fov_deg = SH_VC_FOV_DEG;
}

int sh_snd_camera_balance(const ShCamera *camera, const float position[3])
{
    (void)camera; (void)position;
    return -13;
}

void sh_snd_update(uint16_t sound, int balance, int attenuation, int pitch)
{
    assert(sound == 1465 && balance == -7 && pitch == 0);
    valve_volume_updates++;
    valve_attenuation = attenuation;
}
static unsigned warps;
static unsigned medallion_sounds;
static uint32_t last_medallion_sound;
static unsigned boiler_switch_sounds, boiler_door_sounds, boiler_loop_starts, boiler_loop_stops;
static int boiler_loop_volume;
static unsigned acid_sounds, acid_stops, acid_mutes;
static unsigned piano_falls;
static unsigned card_sounds;
static unsigned rubber_ball_sounds;

void sh_bgm_status_set(unsigned flags)
{
    assert(flags == SH_BGM_STATUS_APPLY_MUTE || flags == SH_BGM_STATUS_VOICE_DIALOG);
    bgm_flags |= flags;
    acid_mutes++;
}

bool sh_input_down(const ShInputState *input, uint32_t key)
{
    return input && (input->down & key) != 0;
}

void sh_vc_warp(ShVc *vc)
{
    (void)vc;
    warps++;
}

void sh_snd_at(uint16_t sound, const float pos[3], float volume)
{
    if (sound == 1452) {
        assert(pos[0] == 60.5f && pos[1] == SH_Q12_TO_FLOAT(SH_Q12(0.7f)) && (pos[2] == 61 || pos[2] == 59) && volume == 0.5f);
        boss_clicks++;
        return;
    }
    if (sound == 1435 || sound == 1436) {
        assert(volume == 0.5f && pos[0] < -58 && pos[2] < -17);
        return;
    }
    if (sound == 1466) {
        assert(pos[0] == SH_Q12_TO_FLOAT(SH_Q12(-60.3f)) && pos[1] == 0.0f);
        assert(pos[2] == -SH_Q12_TO_FLOAT(SH_Q12(20.2f)) && volume == 0.5f);
        locker_body_sounds++;
        return;
    }
    if (sound == 1335) {
        assert(pos[1] == 1.2f && volume == 0.5f);
        memcpy(roof_key_sound_position, pos, sizeof(roof_key_sound_position));
        roof_key_sounds++;
        return;
    }
    if (sound == 1462 || sound == 1463) {
        assert(volume == 0.5f);
        if (sound == 1463) { valve_squeaks++; } else { valve_clicks++; }
        return;
    }
    if (sound == 1461) {
        assert(pos[0] == -10.5f && pos[1] == 0.0f && pos[2] == 17.5f && volume == 0.5f);
        rubber_ball_sounds++;
        return;
    }
    if (sound == 1454 || sound == 1343) {
        assert(volume == 0.5f && pos[2] == -SH_Q12_TO_FLOAT(SH_Q12(19.3f)));
        assert(pos[0] == (sound == 1454 ? SH_Q12_TO_FLOAT(SH_Q12(55.85f)) : 56.0f));
        assert(pos[1] == -SH_Q12_TO_FLOAT(SH_Q12(sound == 1454 ? -1.1f : -1.2f)));
        card_sounds++;
        return;
    }
    if (sound == 1433) {
        assert(pos[0] == SH_Q12_TO_FLOAT(SH_Q12(-59.2f)) && pos[1] == 1.0f && pos[2] == -99.5f);
        assert(volume == 0.5f);
        acid_sounds++;
        return;
    }
    if (sound == 1420 || sound == 1422) {
        assert(volume == 0.5f && pos[2] == 17.5f);
        if (sound == 1420) {
            assert(pos[0] == 20.5f && pos[1] == -SH_Q12_TO_FLOAT(SH_Q12(-1.2f)));
            boiler_switch_sounds++;
        } else {
            assert(pos[0] == SH_Q12_TO_FLOAT(SH_Q12(18.8f)) && pos[1] == -4.0f);
            boiler_door_sounds++;
        }
        return;
    }
    assert(sound == 1334 || sound == 1344 || sound == 1342);
    assert(pos[1] == 1.2f && volume == 0.5f);
    sounds++;
}

void sh_snd_call(uint32_t sound)
{
    if (sound == 19) { voice_pending = false; return; }
    if (sound == 4179 || sound == 4175 || sound == 4157) { arrival_voices++; voice_pending = true; return; }
    if (sound == 4149) { phone_rings++; return; }
    if (sound == 4150) { phone_voices++; return; }
    if (sound == 1456) { phone_pickups++; return; }
    if (sound == 1457) { phone_returns++; return; }
    if (sound == 1451) { descent_starts++; return; }
    if (sound == 1453) { boss_loop_starts++; return; }
    if (sound == 4152) { bathroom_sounds++; return; }
    if (sound == 1465) { valve_water_starts++; return; }
    if (sound == 1417) { piano_falls++; return; }
    if (sound == 1421 || (sound >= 1438 && sound <= 1445)) { return; }
    if (sound == 1430) {
        boiler_loop_starts++;
        return;
    }
    assert(sound == 1416 || sound == 1419);
    last_medallion_sound = sound;
    medallion_sounds++;
}

void sh_snd_falloff(uint16_t sound, const float pos[3], int volume, float falloff, int pitch)
{
    if (sound == 1451) {
        assert(pos[0] == SH_Q12_TO_FLOAT(SH_Q12(54.47f)) && pos[1] <= 0 && pos[2] == 60);
        assert(volume >= 0 && volume <= 128 && falloff == 32 && pitch == 0);
        descent_updates++;
        return;
    }
    if (sound == 1453) {
        assert(pos[0] == SH_Q12_TO_FLOAT(SH_Q12(58.4f)) && pos[1] == 1 && pos[2] == 60);
        assert(volume >= 0 && falloff == 18 && pitch == 0);
        boss_volume_updates++;
        return;
    }
    assert(sound == 1430 && volume >= 0 && volume <= 128 && pitch == 0);
    assert((falloff == 24.0f && pos[0] == SH_Q12_TO_FLOAT(SH_Q12(18.8f))) ||
           (falloff == 16.0f && pos[0] == 52.0f && volume == 25));
    boiler_loop_volume = volume;
}

void sh_snd_stop(uint16_t sound)
{
    if (sound == 1451) { descent_stops++; return; }
    if (sound == 1453) { boss_loop_stops++; return; }
    if (sound == 1465) { valve_water_stops++; return; }
    if (sound == 1433) { acid_stops++; return; }
    assert(sound == 1430);
    boiler_loop_stops++;
}

void sh_snd_play(uint16_t sound, int balance, int attenuation)
{
    if (sound == 1456) {
        assert(balance == -13 && attenuation == 191);
        phone_animation_pickups++;
        return;
    }
    if (sound == 1460 || sound == 1434) {
        assert(balance == -13 && (attenuation == 0 || (sound == 1434 && attenuation == 59)));
        locker_door_sounds++;
        return;
    }
    assert(sound == SH_SFX_MENU_CONFIRM || sound == SH_SFX_MENU_MOVE || sound == SH_SFX_MENU_CANCEL ||
           sound == SH_SFX_MENU_ERROR || sound == SH_SFX_MENU_MAP);
    (void)balance;
    (void)attenuation;
}

static void tick(ShTown *town, ShSavegame *save, float dt, uint32_t down)
{
    bgm_flags = 0;
    ShInputState input = {.down = down};
    fade_update(town, dt);
    if (town->harry_state) { town->harry_time += dt; }
    sh_school_update(town, save, &input, dt);
    sh_border_update(&town->border, SH_Q12(dt));
    if (town->school.smoke.active) { sh_smoke_update(&town->school.smoke, dt); }
}

static void acid_test(float dt, int skip_step)
{
    ShTown town = {.stage = SH_TOWN_EXPLORE};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S01;
    int count;
    const ShMapEvent *events = sh_school_map_events(save.map_idx, &count), *event = NULL, *gold = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].required_item == 96) { event = &events[i]; }
        if (events[i].event_param == 7 && events[i].sys_state == SH_SYS_EVENT_CALLBACK) { gold = &events[i]; }
    }
    assert(event && gold && !sh_school_event_enabled(gold, &save));
    const ShMapPoint *points = sh_school_map_points(save.map_idx, &count);
    const ShMapPoint *point = &points[event->point_idx];
    const float yaw = -point->param0 * PI_F / 128.0f;
    assert(!sh_town_item_use(&town, &save, point->x, -point->z, yaw, 96));
    sh_save_item_add(&save, 96, 1);
    sh_save_event_set(&save, 68);
    ShInventory inv;
    sh_inv_open(&inv, &save);
    assert(sh_town_item_triggers(&town, &save, point->x, -point->z, yaw, inv.available_use_items, 5) == 1);
    const ShInputState action = {.down = SH_BTN_ACTION}, none = {0};
    sh_inv_update(&inv, &save, &action, dt);
    assert(inv.cmd_count == 1);
    sh_inv_update(&inv, &save, &none, dt);
    sh_inv_update(&inv, &save, &action, dt);
    assert(!inv.open && inv.request_use_item == 96);
    assert(sh_town_item_use(&town, &save, point->x, -point->z, yaw, 96));
    const unsigned sound_before = acid_sounds, stop_before = acid_stops, mute_before = acid_mutes;
    bool warped = false, smoke_seen = false, animation_seen = false;
    int script_frames = 0;
    if (skip_step >= 1) { town.step = skip_step; }
    for (int frame = 0; frame < 2000 && town.stage == SH_TOWN_SCRIPT; frame++) {
        assert(!sh_save_event_get(&save, 74));
        assert(sh_school_prop_mask(&town, &save, -59.2f, -99.5f) == 1u);
        tick(&town, &save, dt, skip_step >= 1 && frame == 0 ? KEY_START : 0);
        script_frames++;
        float x = 0, z = 0, angle = 1;
        if (sh_school_take_player_warp(&town, &x, &z, &angle)) {
            assert(x == SH_Q12_TO_FLOAT(SH_Q12(-59.2f)) && z == -SH_Q12_TO_FLOAT(SH_Q12(98.9f)) && angle == 0);
            assert(town.cam_active && town.script_use_cam && town.cam.fov_deg == 53.13f);
            assert(town.cam.pos[0] == SH_Q12_TO_FLOAT(SH_Q12(-59.61f)));
            assert(town.cam.pos[1] == -SH_Q12_TO_FLOAT(SH_Q12(-0.41f)));
            assert(town.cam.pos[2] == -SH_Q12_TO_FLOAT(SH_Q12(97.48f)));
            warped = true;
        }
        int frame_number;
        if (sh_school_harry_keyframe(&town, &frame_number)) {
            assert(frame_number >= 900 && frame_number <= 948);
            animation_seen = true;
        }
        for (int i = 0; i < SH_SMOKE_CAPACITY; i++) {
            smoke_seen |= town.school.smoke.particles[i].visible && town.school.smoke.particles[i].shade;
        }
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.cam_active && !town.script_use_cam);
    assert(sh_save_event_get(&save, 74) && !sh_save_event_get(&save, 69));
    assert(sh_save_item_count(&save, 96) == 1 && !town.school.smoke.active);
    assert(acid_stops > stop_before && acid_mutes == mute_before + script_frames);
    assert(sh_school_prop_mask(&town, &save, -59.2f, -99.5f) == 6u);
    if (skip_step < 0) {
        assert(warped && smoke_seen && animation_seen && acid_sounds == sound_before + 1);
        assert(script_frames * dt > 9.5f && script_frames * dt < 10.5f);
    } else { assert(!warped && !smoke_seen && acid_sounds == sound_before); }
    for (int i = 0; i < 200; i++) {
        fade_update(&town, dt);
        sh_border_update(&town.border, SH_Q12(dt));
    }
    assert(town.border.state == SH_BORDER_NONE && town.border.shade == 0 && town.fade == 0.0f);
    uint8_t available[5];
    assert(sh_town_item_triggers(&town, &save, point->x, -point->z, yaw, available, 5) == 0);
    assert(sh_school_event_enabled(gold, &save));
    assert(sh_school_begin(&town, &save, gold));
    for (int i = 0; i < 1000 && town.stage == SH_TOWN_SCRIPT; i++) {
        tick(&town, &save, dt, town.prompt_open ? SH_BTN_ACTION : 0);
    }
    assert(sh_save_item_count(&save, 97) == 1 && sh_save_event_get(&save, 69));
    assert(sh_school_prop_mask(&town, &save, -59.2f, -99.5f) == 2u);
}

static void inspection_test(int callback, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = callback};
    assert(sh_school_begin(&town, &save, &event));
    for (int frame = 0; frame < 1000; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.controls_locked && town.school.substep == 4 && town.subtitle);
    assert(sh_school_background(&town));
    tick(&town, &save, dt, SH_BTN_CANCEL);
    assert(town.school.substep == 4);
    /* RECEPPIC's message 27 continues into 28 before its ~E. */
    if (callback == 17) {
        tick(&town, &save, dt, SH_BTN_ACTION);
        assert(town.school.substep == 4 && town.school.message == 28);
        tick(&town, &save, dt, 0);
        assert(strstr(town.subtitle, "bad taste"));
    }
    tick(&town, &save, dt, SH_BTN_ACTION);
    if (callback >= 12 && callback <= 14) {
        tick(&town, &save, dt, 0);
        assert(town.school.substep == 5 && town.school.brightness == 48);
        assert(town.subtitle && strstr(town.subtitle, callback == 12 ? "10:00" : callback == 13 ? "12:00" : "5:00"));
        for (int frame = 0; frame < 300; frame++) {
            tick(&town, &save, dt, 0);
        }
        assert(town.school.substep == 5);
        tick(&town, &save, dt, SH_BTN_ACTION);
    }
    for (int frame = 0; frame < 1000 && town.stage == SH_TOWN_SCRIPT; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.school.image);
    assert(town.fade_target == 0.0f && town.school.brightness == 128);
}

static void door_test(int callback, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = callback};
    unsigned previous = sounds;
    assert(sh_school_begin(&town, &save, &event));
    int frames = 0;
    do {
        tick(&town, &save, dt, 0);
        frames++;
    } while (town.school.substep == 1 && frames < 60);
    assert(frames == (SH_Q12(0.2f) / SH_Q12(dt)) + 1 && sounds == previous + 1);
    for (int frame = 0; frame < 300; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.school.substep == 2 && town.controls_locked);
    tick(&town, &save, dt, SH_BTN_ACTION);
    tick(&town, &save, dt, 0);
    assert(town.stage == SH_TOWN_EXPLORE);
}

static void message_test(void)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    const ShMapEvent event = {.sys_state = SH_SYS_READ_MESSAGE, .event_param = 33, .complete_flag = 73};
    assert(sh_school_begin(&town, &save, &event));
    tick(&town, &save, 1.0f / 30.0f, 0);
    assert(strstr(town.subtitle, "valve"));
    tick(&town, &save, 1.0f / 30.0f, SH_BTN_ACTION);
    assert(town.school.message == 34 && !sh_save_event_get(&save, 73));
    tick(&town, &save, 1.0f / 30.0f, 0);
    assert(strstr(town.subtitle, "better leave it alone"));
    tick(&town, &save, 1.0f / 30.0f, SH_BTN_ACTION);
    assert(town.stage == SH_TOWN_EXPLORE && sh_save_event_get(&save, 73));
}

static void alternative_message_test(void)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S02;
    int count;
    const ShMapEvent *events = sh_school_map_events(save.map_idx, &count);
    const ShMapEvent *fan = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].sys_state == SH_SYS_READ_MESSAGE && events[i].event_param == 48) { fan = &events[i]; }
    }
    assert(fan && sh_school_begin(&town, &save, fan));
    for (int i = 0; i < 300; i++) { tick(&town, &save, 1.0f / 30.0f, 0); }
    assert(town.stage == SH_TOWN_SCRIPT && town.school.message == 48);
    assert(strcmp(town.subtitle, "A huge fan is rotating.") == 0);
    tick(&town, &save, 1.0f / 30.0f, SH_BTN_ACTION);
    tick(&town, &save, 1.0f / 30.0f, 0);
    assert(town.school.message == 49 && strstr(town.subtitle, "sucked in."));
    tick(&town, &save, 1.0f / 30.0f, SH_BTN_ACTION);
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked);
    const ShMapEvent arrival = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = 5};
    assert(sh_school_begin(&town, &save, &arrival));
}

static void alternative_inspections_test(float dt)
{
    for (int callback = 6; callback <= 8; callback++) {
        ShTown town = {0};
        ShSavegame save;
        sh_save_init(&save);
        save.map_idx = SH_MAP_IDX_MAP1_S02;
        const ShSavegame before = save;
        const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = callback};
        assert(sh_school_begin(&town, &save, &event));
        for (int i = 0; i < 1000; i++) { tick(&town, &save, dt, 0); }
        assert(town.stage == SH_TOWN_SCRIPT && town.controls_locked && town.subtitle);
        if (callback == 8) {
            assert(town.step == 2 && strcmp(town.subtitle, "The tower door is locked.") == 0);
        } else {
            assert(town.school.substep == 4);
            assert(strcmp(town.school.image, callback == 6 ? "GOLD2.TIM" : "SILVER2.TIM") == 0);
            assert(strstr(town.subtitle, callback == 6 ? "A Golden Sun" : "A Silver Moon"));
        }
        tick(&town, &save, dt, SH_BTN_CANCEL);
        assert(town.stage == SH_TOWN_SCRIPT);
        tick(&town, &save, dt, SH_BTN_ACTION);
        for (int i = 0; i < 1000 && town.stage == SH_TOWN_SCRIPT; i++) { tick(&town, &save, dt, 0); }
        assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked);
        assert(memcmp(&save, &before, sizeof(save)) == 0);
    }
}

static void alternative_pickup_test(int callback, bool accept, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S02;
    int count;
    const ShMapEvent *events = sh_school_map_events(save.map_idx, &count), *event = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].sys_state == SH_SYS_EVENT_CALLBACK && events[i].event_param == callback) { event = &events[i]; break; }
    }
    assert(event && sh_school_begin(&town, &save, event));
    const uint8_t item = town.pickup_item_id;
    assert(item == (callback == 12 ? 85 : 99));
    for (int frame = 0; frame < 300; frame++) { tick(&town, &save, dt, 0); }
    assert(town.prompt_open && town.controls_locked && town.pickup_scale == 1.0f);
    assert(sh_save_event_get(&save, event->complete_flag));
    assert(strstr(town.subtitle, callback == 12 ? "Picture card" : "Rubber ball"));
    assert(!sh_save_item_count(&save, item));
    tick(&town, &save, dt, accept ? SH_BTN_ACTION : SH_BTN_CANCEL);
    for (int frame = 0; frame < 30 && town.stage == SH_TOWN_SCRIPT; frame++) { tick(&town, &save, dt, 0); }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.pickup_active);
    assert(sh_save_item_count(&save, item) == (accept ? 1 : 0));
    assert(sh_save_event_get(&save, event->complete_flag) == accept);
    assert(sh_school_event_enabled(event, &save) == !accept);
}

static void alternative_book_test(float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S02;
    const ShSavegame before = save;
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = 16};
    assert(sh_school_begin(&town, &save, &event));
    assert(!sh_save_event_get(&save, 97));
    tick(&town, &save, dt, 0);
    assert(sh_save_event_get(&save, 97));
    for (int i = 0; i < 300; i++) { tick(&town, &save, dt, 0); }
    assert(town.school.substep == 4 && town.school.message == 38);
    assert(strcmp(town.school.image, "LVTRYMSG.TIM") == 0);
    assert(strstr(town.subtitle, "Leonard Rhine") && strstr(town.subtitle, "The Monster Lurks"));
    tick(&town, &save, dt, SH_BTN_ACTION);
    for (int i = 0; i < 300; i++) { tick(&town, &save, dt, 0); }
    assert(town.school.substep == 4 && town.school.message == 39);
    assert(strstr(town.subtitle, "What's that supposed to mean?"));
    tick(&town, &save, dt, SH_BTN_CANCEL);
    assert(town.school.substep == 4);
    tick(&town, &save, dt, SH_BTN_ACTION);
    for (int i = 0; i < 300 && town.stage == SH_TOWN_SCRIPT; i++) { tick(&town, &save, dt, 0); }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && sh_save_event_get(&save, 97));
    sh_save_event_clear(&save, 97);
    assert(memcmp(&save, &before, sizeof(save)) == 0);
}

static void alternative_lobby_key_test(float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S02;
    sh_save_item_add(&save, 64, 1);
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = 4};
    assert(sh_school_begin(&town, &save, &event));
    const unsigned previous_sounds = sounds;
    tick(&town, &save, dt, 0);
    assert(!sh_save_item_count(&save, 64) && sounds == previous_sounds + 1);
    for (int i = 0; i < 300; i++) { tick(&town, &save, dt, 0); }
    assert(town.school.substep == 2 && town.school.message == 19 && town.subtitle[0] == '\0');
    tick(&town, &save, dt, SH_BTN_ACTION);
    tick(&town, &save, dt, 0);
    assert(town.school.message == 20 && town.subtitle[0] == '\0');
    tick(&town, &save, dt, SH_BTN_ACTION);
    tick(&town, &save, dt, 0);
    assert(town.school.message == 21 && strstr(town.subtitle, "A Golden Sun"));
    tick(&town, &save, dt, SH_BTN_ACTION);
    tick(&town, &save, dt, 0);
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked);
    assert(sounds == previous_sounds + 1 && !sh_save_item_count(&save, 64));
}

static void alternative_slot_test(bool seen, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S02;
    if (seen) { sh_save_event_set(&save, 94); }
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = 10};
    assert(sh_school_begin(&town, &save, &event));
    for (int i = 0; i < 300; i++) { tick(&town, &save, dt, 0); }
    assert(town.step == 5 && town.school.message == (seen ? 25 : 23));
    assert(strcmp(town.school.image, "RECEPDR1.TIM") == 0);
    assert(sh_save_event_get(&save, 94) == seen);
    for (int msg = seen ? 25 : 23; msg <= 25; msg++) {
        assert(town.school.message == msg);
        tick(&town, &save, dt, SH_BTN_ACTION);
        tick(&town, &save, dt, 0);
    }
    assert(town.step == 7);
    for (int i = 0; i < 300; i++) { tick(&town, &save, dt, 0); }
    if (!seen) {
        assert(town.step == 7 && town.school.message == 26 && town.bg_transition == 1.0f);
        float alpha;
        assert(strcmp(sh_school_background_next(&town, &alpha), "RECEPDR2.TIM") == 0 && alpha == 1.0f);
        tick(&town, &save, dt, SH_BTN_CANCEL);
        assert(town.step == 7);
        tick(&town, &save, dt, SH_BTN_ACTION);
    }
    tick(&town, &save, dt, 0);
    assert(town.step == 9 && town.school.message == 27);
    assert(strcmp(town.school.image, "RECEPDR2.TIM") == 0);
    assert(sh_save_event_get(&save, 94) == seen);
    tick(&town, &save, dt, SH_BTN_ACTION);
    tick(&town, &save, dt, 0);
    assert(town.school.message == 28 && town.step == 9);
    tick(&town, &save, dt, SH_BTN_ACTION);
    assert(town.step == 10 && sh_save_event_get(&save, 94) == seen);
    for (int i = 0; i < 300 && town.stage == SH_TOWN_SCRIPT; i++) { tick(&town, &save, dt, 0); }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && sh_save_event_get(&save, 94));
    assert(!town.school.image && !town.school.next_image && town.fade_target == 0.0f);
}

static void alternative_card_test(float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S02;
    sh_save_item_add(&save, 85, 1);
    save.equipped_weapon = SH_ITEM_HANDGUN;
    int count;
    const ShMapEvent *events = sh_school_map_events(save.map_idx, &count), *event = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].sys_state == SH_SYS_EVENT_CALLBACK && events[i].event_param == 11) { event = &events[i]; break; }
    }
    assert(event && event->required_item == 85 && sh_school_begin(&town, &save, event));
    sh_school_seed_harry_keyframe(&town, 503);
    assert(town.school.card_player.model.keyframe == 503);
    const unsigned before = card_sounds;
    tick(&town, &save, dt, 0);
    float x, z, yaw;
    assert(sh_school_take_player_warp(&town, &x, &z, &yaw));
    assert(x == 56.5f && z == -SH_Q12_TO_FLOAT(SH_Q12(19.3f)) && yaw > 1.57f && yaw < 1.58f);
    assert(town.cam_active && town.cam.fov_deg == 53.13f && town.school.empty_hand);
    bool first_animation = false, second_animation = false;
    for (int frame = 0; frame < 1000 && town.step != 5; frame++) {
        tick(&town, &save, dt, 0);
        if (town.harry_state == 94) {
            int keyframe;
            assert(sh_school_harry_keyframe(&town, &keyframe));
            first_animation = true;
        }
        assert(!sh_save_event_get(&save, 1207));
    }
    assert(town.step == 5 && first_animation && card_sounds == before + 1);
    for (int frame = 0; frame < 300; frame++) { tick(&town, &save, dt, 0); }
    assert(town.step == 5 && strstr(town.subtitle, "Picture card") && town.school.empty_hand);
    tick(&town, &save, dt, SH_BTN_CANCEL);
    assert(town.step == 5);
    tick(&town, &save, dt, KEY_START);
    assert(town.step == 5);
    tick(&town, &save, dt, SH_BTN_ACTION);
    for (int frame = 0; frame < 1000 && town.stage == SH_TOWN_SCRIPT; frame++) {
        tick(&town, &save, dt, 0);
        if (town.harry_state == 95) { second_animation = true; }
    }
    assert(town.stage == SH_TOWN_EXPLORE && second_animation && card_sounds == before + 2);
    assert(!town.controls_locked && !town.cam_active && !town.school.empty_hand && !town.harry_state);
    assert(sh_save_event_get(&save, 1207) && sh_save_item_count(&save, 85) == 1);
    assert(save.equipped_weapon == SH_ITEM_HANDGUN && town.fade_target == 0.0f);
}

static void alternative_animated_pickup_test(int callback, int item, int amount, bool accept, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S02;
    int count;
    const ShMapEvent *events = sh_school_map_events(save.map_idx, &count), *event = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].sys_state == SH_SYS_EVENT_CALLBACK && events[i].event_param == callback) { event = &events[i]; break; }
    }
    assert(event && event->required_flag > 0 && !sh_school_event_enabled(event, &save));
    sh_save_event_set(&save, event->required_flag);
    assert(sh_school_begin(&town, &save, event));
    sh_school_seed_harry_keyframe(&town, 503);
    bool crouched = false, stood = false;
    for (int frame = 0; frame < 1000 && !town.prompt_open; frame++) {
        tick(&town, &save, dt, 0);
        if (town.harry_state == 59) { crouched = true; }
        if (town.step < 5) { assert(!sh_save_event_get(&save, event->complete_flag)); }
    }
    assert(crouched && town.prompt_open && town.pickup_item_id == item);
    assert(strstr(town.subtitle, callback == 17 ? "Shotgun" : "Classroom key"));
    assert(sh_save_event_get(&save, event->complete_flag) && sh_save_item_count(&save, item) == 0);
    tick(&town, &save, dt, accept ? SH_BTN_ACTION : SH_BTN_CANCEL);
    for (int frame = 0; frame < 1000 && town.stage == SH_TOWN_SCRIPT; frame++) {
        tick(&town, &save, dt, 0);
        if (town.harry_state == 60) {
            stood = true;
            assert(!town.pickup_active && !town.prompt_open);
        }
    }
    assert(stood && town.stage == SH_TOWN_EXPLORE && !town.controls_locked);
    assert(sh_save_item_count(&save, item) == (accept ? amount : 0));
    assert(sh_save_event_get(&save, event->complete_flag) == accept);
    assert(sh_school_event_enabled(event, &save) == !accept);
    assert(save.picked_up_item_count == (accept ? 1u : 0u));
}

static void roof_books_test(int callback, bool seen, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S03;
    const unsigned flag = callback == 9 ? 116 : 117;
    if (seen) { sh_save_event_set(&save, flag); }
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = callback};
    assert(sh_school_begin(&town, &save, &event));
    for (int i = 0; i < 300; i++) { tick(&town, &save, dt, 0); }
    if (callback == 9 && !seen) {
        assert(town.step == 3 && town.school.message == 15);
        tick(&town, &save, dt, SH_BTN_ACTION);
        for (int i = 0; i < 300; i++) { tick(&town, &save, dt, 0); }
        assert(town.step == 5 && town.school.message == 16);
        tick(&town, &save, dt, SH_BTN_ACTION);
        for (int i = 0; i < 300; i++) { tick(&town, &save, dt, 0); }
    }
    assert(town.step == (callback == 9 ? 10 : 5) && town.subtitle && town.controls_locked);
    assert(strcmp(town.school.image, callback == 9 ? "LMONSTER.TIM" : "LIBRBOOK.TIM") == 0);
    assert(sh_save_event_get(&save, flag) == seen);
    const int first = callback == 9 ? 17 : 23, last = callback == 9 ? 22 : 29;
    for (int msg = first; msg <= last; msg++) {
        assert(town.school.message == msg);
        tick(&town, &save, dt, SH_BTN_CANCEL);
        assert(town.school.message == msg);
        tick(&town, &save, dt, SH_BTN_ACTION);
        tick(&town, &save, dt, 0);
    }
    for (int i = 0; i < 300 && town.stage == SH_TOWN_SCRIPT; i++) { tick(&town, &save, dt, 0); }
    if (callback == 10 && !seen) {
        assert(town.stage == SH_TOWN_SCRIPT && town.step == 9 && town.school.message == 30);
        assert(strstr(town.subtitle, "old fairy tale"));
        assert(!sh_save_event_get(&save, flag));
        tick(&town, &save, dt, SH_BTN_ACTION);
        tick(&town, &save, dt, 0);
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && sh_save_event_get(&save, flag));
    assert(!town.school.image && town.fade_target == 0.0f);
}

static void roof_rubber_ball_test(int point, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S03;
    sh_save_item_add(&save, 99, 1);
    int count;
    const ShMapEvent *events = sh_school_map_events(save.map_idx, &count), *event = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].sys_state == SH_SYS_EVENT_CALLBACK && events[i].event_param == 15 && events[i].point_idx == point) { event = &events[i]; break; }
    }
    assert(event && event->required_item == 99 && sh_school_begin(&town, &save, event));
    sh_school_seed_harry_keyframe(&town, 503);
    const unsigned sounds_before = rubber_ball_sounds;
    bool crouch = false, stand = false;
    for (int frame = 0; frame < 1000 && town.step != 7; frame++) {
        tick(&town, &save, dt, 0);
        if (town.harry_state == 59) { crouch = true; }
    }
    for (int frame = 0; frame < 300; frame++) { tick(&town, &save, dt, 0); }
    assert(crouch && town.step == 7 && strstr(town.subtitle, "Used the Rubber ball"));
    assert(strcmp(town.school.image, "BALLHOLE.TIM") == 0);
    assert(sh_save_event_get(&save, 111) && sh_save_item_count(&save, 99) == 1);
    assert(rubber_ball_sounds == sounds_before + 1);
    tick(&town, &save, dt, KEY_START);
    assert(town.step == 7);
    tick(&town, &save, dt, SH_BTN_ACTION);
    for (int frame = 0; frame < 1000 && town.stage == SH_TOWN_SCRIPT; frame++) {
        tick(&town, &save, dt, 0);
        if (town.step >= 10) { assert(!town.school.image); }
        if (town.harry_state == 60) { stand = true; }
    }
    assert(stand && town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.harry_state);
    assert(sh_save_item_count(&save, 99) == 1 && sh_save_event_get(&save, 111));
    assert(!town.school.image && !town.school.next_image && rubber_ball_sounds == sounds_before + 1);
}

static void roof_drain_inspection_test(int callback, int point, unsigned flags, bool item_use, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S03;
    if (flags & 1) { sh_save_event_set(&save, 111); }
    if (flags & 2) { sh_save_event_set(&save, 113); }
    if (flags & 4) { sh_save_event_set(&save, 114); }
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = callback,
                              .point_idx = point, .activation_type = item_use ? SH_ACT_ITEM : SH_ACT_BUTTON};
    ShSavegame expected = save;
    const bool ball_point = point >= 19;
    if (ball_point && item_use) { sh_save_event_set(&expected, 111); }
    if (callback == 7 && !ball_point && !(flags & 2)) { sh_save_event_set(&expected, 112); }
    assert(sh_school_begin(&town, &save, &event));
    sh_school_seed_harry_keyframe(&town, 503);
    bool crouch = false, stand = false;
    for (int frame = 0; frame < 1000 && !town.subtitle; frame++) {
        tick(&town, &save, dt, 0);
        if (town.harry_state == 59) { crouch = true; }
    }
    assert(crouch && town.subtitle && town.controls_locked);
    int message;
    if (ball_point) { message = item_use ? 36 : callback == 7 && (flags & 1) ? 34 : 35; }
    else { message = (flags & 4) ? 35 : callback == 6 || (flags & 2) ? 33 : 32; }
    assert(town.school.message == message);
    if (callback == 7) {
        const char *image = ball_point ? (item_use || (flags & 1) ? "BALLHOLE.TIM" : "BALLHOL1.TIM") :
                                        flags & 2 ? "HOLEKEY1.TIM" : "HOLEKEY0.TIM";
        assert(town.school.image && strcmp(town.school.image, image) == 0);
    } else { assert(!town.school.image); }
    for (int frame = 0; frame < 300; frame++) { tick(&town, &save, dt, 0); }
    assert(town.school.message == message && town.stage == SH_TOWN_SCRIPT);
    tick(&town, &save, dt, SH_BTN_ACTION);
    for (int frame = 0; frame < 1000 && town.stage == SH_TOWN_SCRIPT; frame++) {
        tick(&town, &save, dt, 0);
        if (town.harry_state == 60) { stand = true; }
    }
    assert(stand && town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.harry_state);
    assert(memcmp(&save, &expected, sizeof(save)) == 0);
}

static void flags_and_transition_test(void)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    int count;
    const ShMapEvent *events = sh_school_events(&count);
    assert(count == 100); /* EndOfArray sentinel is excluded by gen_map_data. */
    const ShMapEvent *marker = &events[14];
    assert(sh_school_begin(&town, &save, marker));
    assert(!sh_save_event_get(&save, marker->complete_flag));
    tick(&town, &save, 1.0f / 30.0f, 0);
    assert(sh_save_event_get(&save, marker->complete_flag));
    assert(sh_school_event_enabled(marker, &save));
    ShMapEvent exclusive = *marker;
    exclusive.activation_type = SH_ACT_EXCLUSIVE;
    assert(!sh_school_event_enabled(&exclusive, &save));
    assert(!sh_school_event_enabled(&events[9], &save));
    sh_save_event_set(&save, 72);
    assert(sh_school_event_enabled(&events[9], &save));
    int point_count;
    const ShMapPoint *points = sh_school_points(&point_count);
    const ShMapEvent *door = &events[39];
    begin_load_overlay(&town, door, SH_MAP_IDX_MAP1_S00);
    const ShMapPoint *destination = &points[door->event_param];
    float x, z, yaw;
    map_point_player_spawn(destination, &x, &z, &yaw);
    assert(town.overlay_x == x && town.overlay_z == z && town.overlay_yaw == yaw);
    assert(town.overlay_room_idx == (destination->field_4_5 ? destination->field_4_5 - 1 : 0));
    assert(town.overlay_complete_flag == door->complete_flag);
    assert(point_count == 99);
}

static void shade_test(void)
{
    const uint8_t white[] = {0xff, 0x7f};
    const ShTim source = {.bpp = 16, .width = 1, .height = 1, .stride = 2, .pixels = white};
    ShTim result;
    uint8_t pixels[2];
    assert(sh_event_image_shade(&result, pixels, sizeof pixels, &source, 48));
    assert(pixels[0] == 0x6b && pixels[1] == 0xad); /* floor(31*48/128) = 11, RGB555. */
    assert(!sh_event_image_shade(&result, pixels, 1, &source, 48));
}

static void dispatch_test(void)
{
    ShTown town = {.stage = SH_TOWN_EXPLORE};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S00;
    int count;
    const ShMapEvent *events = sh_school_events(&count);
    const ShMapEvent *document = &events[14];
    const ShMapPoint *points = sh_school_points(&count);
    const ShMapPoint *point = &points[document->point_idx];
    const ShInputState action = {.down = SH_BTN_ACTION};
    sh_save_event_set(&save, document->complete_flag);
    town_events_update(&town, &save, point->x, -point->z, -point->param0 * PI_F / 128.0f, &action);
    assert(town.script == SH_TOWN_SCR_SCHOOL && town.school.callback == 12);
    /* A different overlay cannot dispatch the town's tick cutscene. */
    town.stage = SH_TOWN_EXPLORE;
    town.script = SH_TOWN_SCR_NONE;
    save.map_idx = 4;
    sh_save_event_set(&save, SH_RETAIL_EventFlag_158);
    town_events_update(&town, &save, 0, 0, 0, &action);
    assert(town.script == SH_TOWN_SCR_NONE);
}

static void medallion_use_test(const ShMapEvent *event, float dt)
{
    ShTown town = {.stage = SH_TOWN_EXPLORE};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S00;
    int count;
    const ShMapPoint *points = sh_school_points(&count);
    const ShMapPoint *point = &points[event->point_idx];
    const float x = point->x, z = -point->z, yaw = -point->param0 * PI_F / 128.0f;
    uint8_t items[5] = {0};
    assert(sh_town_item_triggers(&town, &save, 0, 0, 0, items, 5) == 0);
    assert(!sh_town_item_use(&town, &save, x, z, yaw, event->required_item));
    sh_save_item_add(&save, event->required_item, 1);
    sh_save_item_add(&save, SH_ITEM_FLASHLIGHT, 1);
    ShInventory inv;
    sh_inv_open(&inv, &save);
    ShInputState action = {.down = SH_BTN_ACTION}, none = {0};
    sh_inv_update(&inv, &save, &action, dt);
    assert(inv.focus == SH_INV_FOCUS_CMD && inv.cmd_count == 1);
    sh_inv_update(&inv, &save, &none, dt);
    sh_inv_update(&inv, &save, &action, dt);
    assert(inv.open && !inv.request_use_item && strcmp(inv.status, "Can't use it here.") == 0);
    assert(sh_town_item_triggers(&town, &save, x, z, yaw, inv.available_use_items, 5) >= 1);
    sh_save_flashlight_set(&save, false);
    sh_inv_update(&inv, &save, &none, dt);
    sh_inv_update(&inv, &save, &action, dt);
    assert(!inv.open && inv.request_use_item == event->required_item && sh_save_flashlight_on(&save));
    assert(sh_town_item_use(&town, &save, x, z, yaw, inv.request_use_item));
    assert(town.script == SH_TOWN_SCR_SCHOOL && town.school.callback == event->event_param);
    const unsigned sounds_before = medallion_sounds;
    bool blended = false, source_shown = false, destination_shown = false;
    for (int frame = 0; frame < 3000 && town.step != 11; frame++) {
        tick(&town, &save, dt, 0);
        assert(!sh_save_event_get(&save, event->complete_flag));
        if (town.step == 6) {
            source_shown = sh_school_background(&town) != NULL;
        }
        float alpha = -1.0f;
        if (sh_town_bg_next_tim(&town, &alpha)) {
            assert(alpha >= 0 && alpha <= 1.0f && last_medallion_sound == 1416);
            blended = true;
        }
        if (town.step == 10) {
            const char *name = sh_school_background(&town);
            assert(name && strstr(name, "2.TIM"));
            destination_shown = true;
        }
    }
    assert(town.step == 11 && blended && source_shown && destination_shown);
    assert(medallion_sounds == sounds_before + 2 && last_medallion_sound == 1419);
    for (int frame = 0; frame < 300; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.step == 11 && town.controls_locked && town.subtitle);
    tick(&town, &save, dt, SH_BTN_CANCEL);
    assert(town.step == 11);
    tick(&town, &save, dt, SH_BTN_ACTION);
    assert(town.step == 12 && !sh_save_event_get(&save, event->complete_flag));
    for (int frame = 0; frame < 300 && town.stage == SH_TOWN_SCRIPT; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && town.fade_target == 0.0f);
    assert(sh_save_event_get(&save, event->complete_flag) && sh_save_event_get(&save, 1120));
    assert(sh_save_item_count(&save, event->required_item) == 1);
    assert(!sh_town_item_use(&town, &save, x, z, yaw, event->required_item));
}

static void clock_test(unsigned flags, bool paper, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    if (flags & 1) {
        sh_save_event_set(&save, 71);
    }
    if (flags & 2) {
        sh_save_event_set(&save, 72);
    }
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = paper ? 11 : 10};
    assert(sh_school_begin(&town, &save, &event));
    for (int frame = 0; frame < 300; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.step == 2 && town.subtitle && !sh_school_background(&town));
    tick(&town, &save, dt, SH_BTN_ACTION);
    for (int frame = 0; frame < 300 && town.step != 4; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.step == 4);
    const char *expected = paper ? "PAPER1.TIM" : (flags & 2) ? "CLOCK5.TIM" : (flags & 1) ? "CLOCK12.TIM" : "CLOCK.TIM";
    assert(strcmp(sh_school_background(&town), expected) == 0);
    for (int frame = 0; frame < 300; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.step == 4 && town.controls_locked);
    tick(&town, &save, dt, SH_BTN_CANCEL);
    if (!paper) {
        assert(town.step == 4);
        tick(&town, &save, dt, SH_BTN_ACTION);
    }
    assert(town.step == 5);
    for (int frame = 0; frame < 300 && town.stage == SH_TOWN_SCRIPT; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.stage == SH_TOWN_EXPLORE && town.fade_target == 0.0f);
}

static void upper_pickup_test(int callback, bool accept, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S01;
    int count;
    const ShMapEvent *events = sh_school_map_events(save.map_idx, &count);
    const ShMapEvent *event = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].sys_state == SH_SYS_EVENT_CALLBACK && events[i].event_param == callback) {
            event = &events[i];
            break;
        }
    }
    assert(event);
    if (event->required_flag) {
        sh_save_event_set(&save, event->required_flag);
    }
    assert(sh_school_begin(&town, &save, event));
    const uint8_t item = town.pickup_item_id;
    assert(item == (callback == 5 ? 96 : callback == 7 ? 97 : 98) && town.pickup_active);
    for (int frame = 0; frame < 300; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.prompt_open && town.controls_locked && town.school.overlay == SH_MAP_IDX_MAP1_S01);
    assert(sh_save_event_get(&save, event->complete_flag) && !sh_save_item_count(&save, item));
    assert(strstr(town.subtitle, callback == 5 ? "Chemical" : callback == 7 ? "Gold medallion" : "Silver medallion"));
    int display_item;
    float scale, spin;
    assert(sh_town_pickup_item(&town, &display_item, &scale, &spin) && display_item == item && scale == 1.0f);
    tick(&town, &save, dt, accept ? SH_BTN_ACTION : SH_BTN_CANCEL);
    tick(&town, &save, dt, 0);
    if (callback == 11) {
        assert(town.stage == SH_TOWN_SCRIPT && town.controls_locked && town.step == 8);
        assert(!town.pickup_active && !town.prompt_open && !town.subtitle);
        bool standing_up = town.harry_state == 60;
        for (int frame = 0; frame < 120 && town.stage == SH_TOWN_SCRIPT; frame++) {
            tick(&town, &save, dt, 0);
            standing_up |= town.harry_state == 60;
        }
        assert(standing_up && town.harry_state == 0);
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.pickup_active && !town.prompt_open && !town.controls_locked);
    assert(sh_save_event_get(&save, event->complete_flag) == accept);
    assert(sh_save_item_count(&save, item) == accept);
    if (callback == 7 && accept) {
        /* The retail item acquired upstairs remains usable at the downstairs sun socket. */
        save.map_idx = SH_MAP_IDX_MAP1_S00;
        const ShMapPoint *points = sh_school_points(&count);
        const ShMapEvent *lower = sh_school_events(&count);
        const ShMapEvent *use = &lower[33];
        const ShMapPoint *point = &points[use->point_idx];
        assert(sh_town_item_use(&town, &save, point->x, -point->z, -point->param0 * PI_F / 128.0f, item));
        for (int frame = 0; frame < 3000 && town.stage == SH_TOWN_SCRIPT; frame++) {
            tick(&town, &save, dt, town.step == 11 ? SH_BTN_ACTION : 0);
        }
        assert(town.stage == SH_TOWN_EXPLORE && sh_save_event_get(&save, use->complete_flag));
        assert(sh_save_event_get(&save, event->complete_flag));
    }
}

static void upper_inspection_test(int callback, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S01;
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = callback};
    assert(sh_school_begin(&town, &save, &event));
    for (int frame = 0; frame < 300; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.school.substep == 4 && town.subtitle);
    assert(strcmp(sh_school_background(&town), callback == 4 ? "HAND.TIM" : callback == 8 ? "SCORE.TIM" :
                                                callback == 10 ? "PIANO1.TIM" : "LOCKER0.TIM") == 0);
    for (int frame = 0; frame < 300 && town.stage == SH_TOWN_SCRIPT; frame++) {
        tick(&town, &save, dt, town.subtitle ? SH_BTN_ACTION : 0);
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked);
}

static void piano_scene_test(float dt, int route, int skip_step)
{
    ShTown town = {.stage = SH_TOWN_SCRIPT, .script = SH_TOWN_SCR_SCHOOL, .controls_locked = true,
                   .school = {.overlay = SH_MAP_IDX_MAP1_S01, .callback = 9, .brightness = 128}};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S01;
    const unsigned falls = piano_falls;
    bool skipped = false, warped = false, camera0 = false, camera1 = false, swung = false;
    for (int frame = 0; frame < 3000 && town.stage == SH_TOWN_SCRIPT; frame++) {
        ShInputState input = {0};
        if (town.step == 3) { input.down = SH_BTN_ACTION; }
        if (town.step == 4) { input.down = route == 1 ? SH_BTN_CANCEL : SH_BTN_ACTION; }
        if (town.step == 8) {
            if (route == 2) { input.down = SH_BTN_CANCEL; }
            else { town.school.piano.counter = 5; }
        }
        if (skip_step >= 10 && town.step == skip_step && !skipped) { input.down = KEY_START; skipped = true; }
        if (town.step == 12) { swung = true; }
        fade_update(&town, dt);
        sh_school_piano_update(&town, &save, &input, dt, 0, 0, SH_Q12(9.8f * dt));
        sh_border_update(&town.border, SH_Q12(dt));
        float x, z, yaw;
        if (sh_school_take_player_warp(&town, &x, &z, &yaw)) {
            assert(x == -100.0f && fabsf(z + 20.1f) < 0.001f && fabsf(yaw - PI_F / 8.0f) < 0.001f);
            warped = true;
        }
        if (town.cam_active) {
            assert(town.cam.fov_deg == 53.13f);
            if (fabsf(town.cam.pos[0] + 97.3f) < 0.001f) { camera0 = true; }
            if (fabsf(town.cam.pos[0] + 101.39f) < 0.001f) { camera1 = true; }
        }
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.cam_active && !town.script_use_cam);
    assert(!town.school.image && !town.school.silver_active && !town.school.piano_result.draw_cursor);
    assert(sh_save_event_get(&save, 75) == (route == 0));
    assert(!sh_save_event_get(&save, 70) && !sh_save_item_count(&save, 98));
    assert(skipped == (skip_step >= 10));
    if (!route) {
        assert(town.school.silver.position[1] == 0 && town.school.silver.rotation[0] == -1024);
        if (skip_step < 0) { assert(warped && camera0 && camera1 && swung && piano_falls == falls + 1); }
    } else {
        assert(!warped && !camera0 && !camera1 && piano_falls == falls);
    }
}

static void ladder_test(int callback, unsigned branch, int skip, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S00;
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = callback,
                              .point_idx = (uint8_t)(branch ? 17 : 16)};
    sh_save_event_set(&save, callback == 15 ? (branch ? 81 : 78) : 82);
    if (callback == 15) { sh_save_event_set(&save, branch ? 185 : 184); }
    else { sh_save_event_set(&save, 79); sh_save_event_set(&save, 80); }
    assert(sh_school_begin(&town, &save, &event));
    bool skipped = false, animated = false;
    float elapsed = 0.0f;
    for (int frame = 0; frame < 1000 && town.stage == SH_TOWN_SCRIPT; frame++) {
        const bool press = skip >= 0 && town.step == skip && !skipped;
        if (press) { skipped = true; }
        tick(&town, &save, dt, press ? KEY_START : 0);
        elapsed += dt;
        if (town.harry_state) {
            int keyframe;
            assert(town.harry_state == (callback == 15 ? 88 : 87));
            assert(sh_school_harry_keyframe(&town, &keyframe));
            assert(town.cam_active && town.cam.fov_deg == 53.13f);
            animated = true;
        }
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.cam_active && !town.script_use_cam);
    assert(town.harry_state == 0 && animated && skipped == (skip >= 0));
    if (callback == 15) {
        assert(!sh_save_event_get(&save, branch ? 81 : 78));
        assert(!sh_save_event_get(&save, branch ? 185 : 184));
        assert(town.school.ladder_player.position.vx == SH_Q12(-60.0f));
        assert(town.school.ladder_player.position.vy == (branch ? SH_Q12(0.6f) : 0));
        assert(town.school.ladder_player.position.vz == SH_Q12(branch ? -56.3f : -63.7f));
        assert(town.school.ladder_player.rotation.vy == (branch ? 2048 : 0));
    } else {
        assert(sh_save_event_get(&save, 79) == !branch && sh_save_event_get(&save, 80) == (bool)branch);
        assert(!sh_save_event_get(&save, 82));
        assert(town.school.ladder_player.position.vy == 0);
    }
    if (skip < 0) { assert(elapsed > 4.1f && elapsed < 5.2f); }
}

static void upper_transitions_test(void)
{
    ShTown town = {.stage = SH_TOWN_EXPLORE};
    ShSavegame save;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP1_S00;
    int count;
    const ShMapEvent *events = sh_school_events(&count);
    const ShMapEvent *stairs = &events[90];
    const ShMapPoint *points = sh_school_points(&count);
    const ShInputState none = {0};
    town_events_update(&town, &save, points[stairs->point_idx].x, -points[stairs->point_idx].z, 0, &none);
    assert(town.stage == SH_TOWN_LEAVING && town.overlay_map_idx == SH_MAP_IDX_MAP1_S01);
    save.map_idx = SH_MAP_IDX_MAP1_S01;
    events = sh_school_map_events(save.map_idx, &count);
    assert(count == 103);
    const ShMapEvent *back = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].sys_state == SH_SYS_LOAD_OVERLAY && events[i].map_idx == SH_MAP_IDX_MAP1_S00) {
            back = &events[i];
            break;
        }
    }
    assert(back);
    points = sh_school_map_points(save.map_idx, &count);
    assert(count == 101 && points[back->event_param].param1 == 0);
    begin_load_overlay(&town, back, save.map_idx);
    assert(town.overlay_x == points[back->event_param].x && town.overlay_z == -points[back->event_param].z);
}

static void map_take_test(bool accept, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    save.paper_map_flags = 1u << 2;
    int count;
    const ShMapEvent *events = sh_school_events(&count);
    const ShMapEvent *event = &events[7];
    assert(event->event_param == 20 && sh_school_begin(&town, &save, event));
    assert(sh_school_prop_mask(&town, &save, 96.9f, -21.2f) & 8u);
    for (int frame = 0; frame < 300; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.school.substep == 3 && town.prompt_open && town.controls_locked);
    assert(strcmp(sh_school_background(&town), "MP_SCL1F.TIM") == 0 && town.school.brightness == 88);
    assert(!sh_save_event_get(&save, event->complete_flag) && save.paper_map_flags == (1u << 2));
    tick(&town, &save, dt, accept ? SH_BTN_ACTION : SH_BTN_CANCEL);
    assert(!town.prompt_open && !sh_save_event_get(&save, event->complete_flag));
    for (int frame = 0; frame < 300 && town.stage == SH_TOWN_SCRIPT; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.school.image);
    assert(sh_save_event_get(&save, event->complete_flag) == accept);
    assert((save.paper_map_flags & (1u << 2)) != 0);
    for (unsigned floor = 5; floor <= 12; floor++) {
        assert(((save.paper_map_flags >> floor) & 1u) == accept);
    }
    assert(sh_school_event_enabled(event, &save) == !accept);
    assert(((sh_school_prop_mask(&town, &save, 96.9f, -21.2f) & 8u) != 0) == !accept);
}

static void map_image_test(void)
{
    FILE *file = fopen("romfs/game/tim/MP_SCL1F.TIM", "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    const size_t size = (size_t)ftell(file);
    rewind(file);
    uint8_t *data = malloc(size);
    uint8_t *pixels = malloc(320u * 480u * 2u);
    assert(data && pixels && fread(data, 1, size, file) == size);
    fclose(file);
    ShTim source, result;
    assert(sh_tim_parse(data, size, &source));
    assert(source.width == 320 && source.height == 480);
    assert(sh_event_image_shade(&result, pixels, 320u * 480u * 2u, &source, 88));
    assert(result.width == source.width && result.height == source.height);
    assert(!sh_event_image_shade(&result, pixels, 320u * 240u * 2u, &source, 88));
    free(data);
    free(pixels);
}

static void boiler_test(bool powered, bool accept, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    sh_save_init(&save);
    if (powered) {
        sh_save_event_set(&save, 72);
    }
    const ShMapEvent event = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = powered ? 18 : 19};
    const unsigned switch_before = boiler_switch_sounds, door_before = boiler_door_sounds;
    assert(sh_school_begin(&town, &save, &event));
    for (int frame = 0; frame < 300; frame++) {
        tick(&town, &save, dt, 0);
    }
    assert(town.step == 2 && town.prompt_open && town.subtitle);
    tick(&town, &save, dt, accept ? SH_BTN_ACTION : SH_BTN_CANCEL);
    assert(!town.prompt_open && !sh_save_event_get(&save, 83));
    if (!accept) {
        tick(&town, &save, dt, 0);
        assert(town.stage == SH_TOWN_EXPLORE && !sh_save_event_get(&save, 73) && !sh_save_event_get(&save, 84));
        assert(boiler_switch_sounds == switch_before && boiler_door_sounds == door_before);
        return;
    }
    bool on = false, off = false;
    int blink_frames = 0;
    for (int frame = 0; frame < 3000 && town.stage == SH_TOWN_SCRIPT; frame++) {
        const int step = town.step;
        tick(&town, &save, dt, !powered && town.step == 5 ? SH_BTN_ACTION : 0);
        if (powered && step == 5) {
            const uint32_t mask = sh_school_prop_mask(&town, &save, 20.5f, 17.5f);
            on |= (mask & 1u) != 0;
            off |= (mask & 1u) == 0;
            blink_frames++;
            assert(!sh_save_event_get(&save, 73) && !sh_save_event_get(&save, 84));
        }
        if (powered && town.step == 7) {
            assert(sh_save_event_get(&save, 73) && !sh_save_event_get(&save, 84));
        }
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked);
    assert(boiler_switch_sounds == switch_before + 1);
    assert(boiler_door_sounds == door_before + (powered ? 1u : 0u));
    assert(sh_save_event_get(&save, 83) == powered && sh_save_event_get(&save, 73) == powered &&
           sh_save_event_get(&save, 84) == powered);
    if (powered) {
        assert(on && off && blink_frames == SH_Q12(3.5f) / SH_Q12(dt) + 1);
        assert(sh_school_prop_mask(&town, &save, 20.5f, 17.5f) == 5u);
        const unsigned starts = boiler_loop_starts, stops = boiler_loop_stops;
        sh_school_world_update(&town, &save, 20.5f, 17.5f, dt);
        assert(boiler_loop_starts == starts + 1 && boiler_loop_volume == 128);
        sh_school_world_update(&town, &save, 52.0f, 16.0f, dt);
        assert(boiler_loop_starts == starts + 1 && boiler_loop_volume == 25);
        sh_school_world_update(&town, &save, 140.0f, -19.0f, dt);
        assert(boiler_loop_stops == stops + 1 && town.school_boiler_volume == 0);
        assert(sh_school_prop_mask(&town, &save, 140.0f, -19.0f) == 0);
        const ShMapEvent running = {.sys_state = SH_SYS_EVENT_CALLBACK, .event_param = 23};
        assert(sh_school_begin(&town, &save, &running));
        tick(&town, &save, dt, 0);
        tick(&town, &save, dt, 0);
        assert(town.step == 2 && strstr(town.subtitle, "boiler is working"));
        tick(&town, &save, dt, SH_BTN_ACTION);
        tick(&town, &save, dt, 0);
        assert(town.stage == SH_TOWN_EXPLORE);
    } else {
        assert(sh_school_prop_mask(&town, &save, 20.5f, 17.5f) == 2u);
    }
}

static void boss_scene_test(int point_index, int direction, float dt)
{
    ShTown town = {.stage = SH_TOWN_EXPLORE};
    ShSavegame save = {0};
    save.map_idx = town.school.overlay = SH_MAP_IDX_MAP1_S02;
    sh_school_world_update(&town, &save, 0, 0, dt);
    int count;
    const ShMapPoint *point = &sh_school_map_points(save.map_idx, &count)[point_index];
    const ShInputState action = {.down = SH_BTN_ACTION};
    town_events_update(&town, &save, point->x, -point->z, -point->param0 * PI_F / 128.0f, &action);
    assert(town.stage == SH_TOWN_SCRIPT && town.school.callback == 18);
    const unsigned clicks = boss_clicks, starts = boss_loop_starts, stops = boss_loop_stops;
    for (int frame = 0; frame < 10 && !town.school.choice_active; frame++) { tick(&town, &save, dt, 0); }
    assert(town.school.choice_active && town.school.choice.count == 3 && !town.school.choice.selected);
    assert(!strcmp(town.school.choice_options[0], "Turn the valve left."));
    assert(!strcmp(town.school.choice_options[2], "Stop turning the valve."));
    const ShBossDoors before = town.school.boss_doors;
    if (direction < 0) {
        tick(&town, &save, dt, SH_BTN_CANCEL);
    } else {
        if (direction == 1) { tick(&town, &save, dt, KEY_DDOWN); }
        tick(&town, &save, dt, SH_BTN_ACTION);
        assert(town.step == (direction ? 4 : 2));
        unsigned frames = 0;
        while (!town.school.choice_active && frames++ < 2000) {
            tick(&town, &save, dt, KEY_START);
            sh_school_world_update(&town, &save, 60, 60, dt);
        }
        assert(frames < 2000 && town.school.choice_active && town.school.boss_camera_started == 1);
        assert(town.vc.user_camera && boss_clicks == clicks + 1 && boss_loop_starts > starts && boss_loop_stops == stops + 1);
        assert(town.school.boss_doors.rotation[0] != before.rotation[0] && town.school.boss_doors.rotation[1] != before.rotation[1]);
        if (!direction) { tick(&town, &save, dt, KEY_DDOWN); }
        tick(&town, &save, dt, SH_BTN_ACTION);
        unsigned second_frames = 0;
        while (!town.school.choice_active && second_frames++ < 2000) {
            tick(&town, &save, dt, 0);
            sh_school_world_update(&town, &save, 60, 60, dt);
        }
        assert(second_frames < frames && town.school.choice_active && boss_clicks == clicks + 2 && boss_loop_stops == stops + 2);
        assert(!memcmp(before.rotation, town.school.boss_doors.rotation, sizeof(before.rotation)));
        tick(&town, &save, dt, SH_BTN_CANCEL);
    }
    tick(&town, &save, dt, 0);
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.prompt_open && !town.school.choice_active);
    assert(!sh_save_event_get(&save, 118) && !sh_save_event_get(&save, 119) && !town.vc.user_camera);
    if (direction < 0) { assert(!memcmp(&before, &town.school.boss_doors, sizeof(before))); }
}

static void boss_solution_test(float dt)
{
    ShTown town = {.stage = SH_TOWN_EXPLORE};
    ShSavegame save = {0};
    save.map_idx = town.school.overlay = SH_MAP_IDX_MAP1_S02;
    sh_school_world_update(&town, &save, 0, 0, dt);
    int count;
    const ShMapPoint *points = sh_school_map_points(save.map_idx, &count);
    const ShInputState action = {.down = SH_BTN_ACTION};
    unsigned completed = 0;
    for (int valve = 0; valve < 2; valve++) {
        const ShMapPoint *point = &points[valve ? 39 : 40];
        town_events_update(&town, &save, point->x, -point->z, -point->param0 * PI_F / 128.0f, &action);
        assert(town.stage == SH_TOWN_SCRIPT && town.school.callback == 18);
        tick(&town, &save, dt, 0);
        for (int turn = 0; turn <= valve; turn++) {
            assert(town.school.choice_active);
            tick(&town, &save, dt, KEY_DDOWN);
            tick(&town, &save, dt, SH_BTN_ACTION);
            unsigned frames = 0;
            while (!town.school.choice_active && frames++ < 2000) {
                tick(&town, &save, dt, 0);
                sh_school_world_update(&town, &save, 60, 60, dt);
            }
            assert(frames < 2000);
            completed++;
            assert(sh_save_event_get(&save, 362) == (completed == 3));
            assert(sh_school_collision_flags(&town, &save, 60, 60) == (completed == 3 ? 3 : 7));
            assert(!sh_save_event_get(&save, 120));
        }
        tick(&town, &save, dt, SH_BTN_CANCEL);
        tick(&town, &save, dt, 0);
        assert(town.stage == SH_TOWN_EXPLORE);
    }
    assert(town.school.boss_doors.rotation[0] == 2048 && town.school.boss_doors.rotation[1] == 2048);
    sh_school_world_update(&town, &save, 0, 0, dt);
    sh_school_world_update(&town, &save, 60, 60, dt);
    assert(sh_save_event_get(&save, 362) && sh_school_collision_flags(&town, &save, 60, 60) == 3);
    const ShInputState none = {0};
    town_events_update(&town, &save, points[41].x, -points[41].z, 0, &none);
    assert(town.stage == SH_TOWN_SCRIPT && town.school.callback == 19);
    town.school.card_player.position = (ShPathPoint){SH_Q12(points[41].x), 0, SH_Q12(points[41].z)};
    sh_school_seed_harry_keyframe(&town, 970);
    const unsigned starts = descent_starts, stops = descent_stops, updates = descent_updates;
    unsigned frames = 0;
    bool animated = false;
    while (town.stage == SH_TOWN_SCRIPT && frames++ < 2000) {
        tick(&town, &save, dt, KEY_START);
        assert(town.school.card_player.position.vy == town.school.boss_box_y);
        assert(town.school.descent_speed >= 0 && town.school.descent_speed <= SH_Q12(0.4f));
        if (town.harry_state == 106) {
            int keyframe;
            assert(sh_school_harry_keyframe(&town, &keyframe));
            animated = true;
        }
        if (town.stage == SH_TOWN_SCRIPT) { assert(!sh_save_event_get(&save, 120) && town.controls_locked); }
    }
    assert(frames < 2000 && animated && sh_save_event_get(&save, 120) && town.fade == 1.0f);
    assert(town.school.boss_box_y > SH_Q12(1.0f) && !town.controls_locked);
    assert(descent_starts == starts + 1 && descent_stops == stops + 1 && descent_updates > updates);
    town_events_update(&town, &save, points[41].x, -points[41].z, 0, &none);
    assert(town.stage == SH_TOWN_LEAVING && town.overlay_map_idx == 8 && town.overlay_spawn_pt == 18);
    float target_x, target_z, target_yaw;
    map_point_player_spawn(&points[18], &target_x, &target_z, &target_yaw);
    assert(town.overlay_x == target_x && town.overlay_z == target_z && town.overlay_yaw == target_yaw);
}

static void boss_world_test(void)
{
    ShTown town = {0};
    ShSavegame save = {0};
    save.map_idx = town.school.overlay = SH_MAP_IDX_MAP1_S02;
    int count;
    const ShSchoolProp *props = sh_school_map_props(save.map_idx, &count);
    assert(count == 9 && !strcmp(props[4].model, "DOOR2_HI") && !strcmp(props[5].model, "DOOR1_HI"));
    assert(!strcmp(props[7].model, "PHONE3_H") && !strcmp(props[8].model, "PHONE2_H"));
    assert(sh_school_prop_mask(&town, &save, 20, -144) == (props[7].mask | props[8].mask));
    sh_save_event_set(&save, 99);
    assert(!sh_school_prop_mask(&town, &save, 20, -144));
    sh_save_event_clear(&save, 99);
    assert(sh_school_prop_mask(&town, &save, 60, 60) == 124);
    assert(sh_school_roof_moving_mask(&town, &save, 60, 60) == (props[4].mask | props[5].mask | props[6].mask));
    for (int a = 0; a < 4; a++) {
        for (int b = 0; b < 4; b++) {
            town.school.boss_doors.rotation[0] = a * 1024;
            town.school.boss_doors.rotation[1] = b * 1024;
            sh_school_world_update(&town, &save, 60, 60, 1.0f / 30);
            const bool opened = a == 2 && b == 2;
            assert(sh_save_event_get(&save, 362) == opened);
            assert(sh_school_collision_flags(&town, &save, 60, 60) == (opened ? 3 : 7));
            const ShBossDoors before = town.school.boss_doors;
            const ShMapEvent read = {.sys_state = SH_SYS_READ_MESSAGE};
            assert(sh_school_begin(&town, &save, &read));
            assert(!memcmp(&before, &town.school.boss_doors, sizeof(before)));
            sh_school_world_update(&town, &save, 0, 0, 1.0f / 30);
            assert(town.school.boss_doors.rotation[0] == 2048);
            assert(town.school.boss_doors.rotation[1] == (opened ? 2048 : 1024));
            assert(!sh_school_prop_mask(&town, &save, 0, 0));
        }
    }
}

static void bathroom_scene_test(float dt)
{
    ShTown town = {.stage = SH_TOWN_EXPLORE};
    ShSavegame save = {0};
    save.map_idx = SH_MAP_IDX_MAP1_S02;
    int count;
    const ShMapPoint *points = sh_school_map_points(save.map_idx, &count);
    const ShMapPoint *point = &points[37];
    const ShInputState action = {.down = SH_BTN_ACTION};
    const unsigned sounds_before = bathroom_sounds;
    town_events_update(&town, &save, point->x, -point->z, -point->param0 * PI_F / 128.0f, &action);
    assert(town.stage == SH_TOWN_SCRIPT && town.school.callback == 15);
    const ShSchoolProp *props = sh_school_map_props(save.map_idx, &count);
    assert(count == 9 && !strcmp(props[0].model, "DOOR_HID") && !strcmp(props[1].model, "GUN_HIDE"));
    assert(sh_school_prop_mask(&town, &save, 18, -60) == 3);
    assert(sh_school_collision_flags(&town, &save, 18, -60) == 3);
    assert(sh_school_collision_flags(&town, &save, 18, 0) == -1);
    unsigned frames = 0;
    bool hidden = false, moved = false;
    while (town.stage == SH_TOWN_SCRIPT && frames++ < 1000) {
        tick(&town, &save, dt, KEY_START);
        assert(sh_save_event_get(&save, 121));
        if (town.stage == SH_TOWN_SCRIPT) { assert(!sh_save_event_get(&save, 96)); }
        hidden |= town.school.player_hidden && town.school.drain_flare_suppressed;
        moved |= town.school.bathroom.rotation < 0;
    }
    assert(frames < 1000 && frames * dt > 8.5f && hidden && moved);
    assert(bathroom_sounds == sounds_before + 1 && sh_save_event_get(&save, 96));
    assert(!town.controls_locked && !town.school.player_hidden && !town.school.drain_flare_suppressed);
    assert(!town.cam_active && !town.script_use_cam && sh_school_bathroom_rotation(&town, &save) == -1280);
    assert(sh_school_collision_flags(&town, &save, 18, -60) == 1);
    const ShBathroomMotion before = town.school.bathroom;
    const ShMapEvent read = {.sys_state = SH_SYS_READ_MESSAGE};
    ShTown copy = town;
    assert(sh_school_begin(&copy, &save, &read));
    assert(!memcmp(&before, &copy.school.bathroom, sizeof(before)));
    const ShMapEvent *events = sh_school_map_events(save.map_idx, &count), *pickup = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].sys_state == SH_SYS_EVENT_CALLBACK && events[i].event_param == 17) { pickup = &events[i]; break; }
    }
    assert(pickup && pickup->required_flag == 96);
    const ShMapPoint *gun_point = &points[pickup->point_idx];
    const float heading = gun_point->param0 * PI_F / 128.0f;
    town_events_update(&town, &save, gun_point->x - sinf(heading) * 0.25f,
        -gun_point->z + cosf(heading) * 0.25f, -heading, &action);
    assert(town.stage == SH_TOWN_SCRIPT && town.school.callback == 17);
    sh_school_seed_harry_keyframe(&town, 503);
    unsigned pickup_frames = 0;
    while ((!town.prompt_open || town.pickup_scale < 1.0f) && pickup_frames++ < 1000) { tick(&town, &save, dt, 0); }
    assert(town.prompt_open && !sh_save_item_count(&save, 162));
    tick(&town, &save, dt, SH_BTN_ACTION);
    while (town.stage == SH_TOWN_SCRIPT && pickup_frames++ < 2000) { tick(&town, &save, dt, 0); }
    assert(town.stage == SH_TOWN_EXPLORE && sh_save_event_get(&save, 95) && sh_save_item_count(&save, 162) == 6);
    assert(sh_school_prop_mask(&town, &save, 18, -60) == 1);
    assert(sh_school_roof_moving_mask(&town, &save, 18, -60) == 1);
    assert(!sh_school_prop_mask(&town, &save, 18, 0));
}

static void locker_camera_test(void)
{
    ShTown town = {0};
    ShSavegame save = {0};
    save.map_idx = SH_MAP_IDX_MAP1_S03;
    town.school.overlay = save.map_idx;
    const unsigned before = locker_camera_calls;
    sh_school_world_update(&town, &save, -59, -19, 0);
    assert(locker_camera_calls == before + 1 && town.school.locker_camera_mode == 1);
    assert(locker_camera_warp_position && locker_camera_warp_look);
    assert(locker_camera_position[0] == SH_Q12_TO_FLOAT(SH_Q12(-61.18f)));
    assert(locker_camera_position[2] == SH_Q12_TO_FLOAT(SH_Q12(19) + SH_Q12(0.49f)));
    sh_school_world_update(&town, &save, -59, -20, 0);
    assert(!locker_camera_warp_position && !locker_camera_warp_look);
    sh_school_world_update(&town, &save, -61, -20, 0);
    assert(town.school.locker_camera_mode == 2 && locker_camera_warp_position && locker_camera_warp_look);
    sh_school_world_update(&town, &save, -59, -20, 0);
    assert(town.school.locker_camera_mode == 1 && locker_camera_warp_position);
    sh_school_world_update(&town, &save, -57, -20, 0);
    assert(!town.school.locker_camera_active && !town.script_use_cam && !town.school.locker_camera_mode);
    sh_save_event_set(&save, 100); sh_save_event_set(&save, 104);
    sh_school_world_update(&town, &save, -60, -18, 0);
    assert(locker_camera_warp_position && !locker_camera_warp_look);
    assert(locker_camera_look[2] == 18.5f);
    sh_school_world_update(&town, &save, -60, -20, 0);
    assert(locker_camera_look[2] == 20.0f);
    sh_save_event_set(&save, 105); sh_save_event_set(&save, 107);
    sh_school_world_update(&town, &save, -60, -20, 0);
    assert(town.school.locker_camera_mode == 1 && locker_camera_warp_position && locker_camera_warp_look);
    sh_school_world_update(&town, &save, -60, -19, 0);
    assert(!locker_camera_warp_position && !locker_camera_warp_look);
    sh_school_world_update(&town, &save, -60, -18, 0);
    assert(!town.school.locker_camera_mode && locker_camera_warp_position && locker_camera_warp_look);
    assert(locker_camera_position[0] == SH_Q12_TO_FLOAT(SH_Q12(-60.15f)));
}

static void school_routing_test(void)
{
    const uint8_t maps[] = {SH_MAP_IDX_MAP1_S00, SH_MAP_IDX_MAP1_S01, SH_MAP_IDX_MAP1_S02, SH_MAP_IDX_MAP1_S03};
    for (unsigned m = 0; m < sizeof(maps); m++) {
        ShSavegame save = {0};
        save.map_idx = maps[m];
        assert(sh_school_map_is_school(save.map_idx));
        int event_count, point_count, pickup_count;
        const ShMapEvent *events = sh_school_map_events(save.map_idx, &event_count);
        const ShMapPoint *points = sh_school_map_points(save.map_idx, &point_count);
        const ShMapCommonPickup *pickups = sh_school_map_pickups(save.map_idx, &pickup_count);
        assert(pickup_count > 0);
        ShTown pickup_town = {.stage = SH_TOWN_EXPLORE};
        const ShMapPoint *point = &points[pickups[0].point_idx];
        const ShInputState action = {.down = SH_BTN_ACTION};
        town_events_update(&pickup_town, &save, point->x, -point->z, -point->param0 * PI_F / 128.0f, &action);
        assert(pickup_town.pickup_active && pickup_town.pickup_flag == pickups[0].event_flag);
        for (int i = 0; i < event_count; i++) {
            const ShMapEvent *event = &events[i];
            if (event->sys_state != SH_SYS_LOAD_ROOM && event->sys_state != SH_SYS_LOAD_OVERLAY) { continue; }
            assert(event->event_param < point_count);
            ShTown town = {0};
            begin_load_overlay(&town, event, save.map_idx);
            float x, z, yaw;
            map_point_player_spawn(&points[event->event_param], &x, &z, &yaw);
            assert(town.overlay_x == x && town.overlay_z == z && town.overlay_yaw == yaw);
            assert(town.overlay_map_idx == event->map_idx && town.overlay_complete_flag == event->complete_flag);
        }
    }
    assert(!sh_school_map_is_school(SH_MAP_IDX_MAP2_S00));
}

static void locker_open_test(float dt, int skip_step)
{
    ShTown town = {0};
    ShSavegame save = {0};
    save.map_idx = SH_MAP_IDX_MAP1_S03;
    town.school.overlay = save.map_idx;
    int count;
    const ShMapEvent *events = sh_school_map_events(save.map_idx, &count);
    const ShMapEvent *event = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].sys_state == SH_SYS_EVENT_CALLBACK && events[i].event_param == 12) { event = &events[i]; break; }
    }
    assert(event);
    const ShMapPoint *point = &sh_school_map_points(save.map_idx, &count)[event->point_idx];
    const ShInputState action = {.down = SH_BTN_ACTION};
    town.stage = SH_TOWN_EXPLORE;
    town_events_update(&town, &save, point->x, -point->z, -point->param0 * PI_F / 128.0f, &action);
    assert(town.stage == SH_TOWN_SCRIPT && town.school.callback == 12);
    unsigned frames = 0, message_frames = 0;
    bool skipped = false, animation85 = false, animation86 = false;
    while (town.stage == SH_TOWN_SCRIPT && frames++ < 2000) {
        uint32_t input = 0;
        if (skip_step >= 0 && town.step == skip_step && !skipped) { input = KEY_START; skipped = true; }
        if (town.step == 11) {
            assert(town.school.image && !strcmp(town.school.image, "LOCKER1.TIM"));
            if (++message_frames > 8) { input = SH_BTN_ACTION; }
        }
        tick(&town, &save, dt, input);
        animation85 |= town.harry_state == 85;
        animation86 |= town.harry_state == 86;
        sh_school_world_update(&town, &save, -60, -18.37f, dt);
    }
    assert(frames < 2000 && !town.controls_locked && town.harry_state == 0 && !town.school.empty_hand);
    for (int flag = 100; flag <= 104; flag++) { assert(sh_save_event_get(&save, flag)); }
    assert(!sh_save_event_get(&save, 105) && !sh_save_event_get(&save, 107));
    assert(town.school.player_warp && town.school.player_warp_position[0] == SH_Q12_TO_FLOAT(SH_Q12(-59.36f)));
    assert(town.school.player_warp_position[1] == -SH_Q12_TO_FLOAT(SH_Q12(18.37f)));
    if (skip_step < 0) { assert(message_frames > 8 && animation85 && animation86); }
    else { assert(skipped && !message_frames); }
}

static void locker_body_test(float dt)
{
    ShTown town = {0};
    ShSavegame save = {0};
    save.map_idx = SH_MAP_IDX_MAP1_S03;
    for (int flag = 100; flag <= 104; flag++) { sh_save_event_set(&save, flag); }
    town.school.overlay = save.map_idx;
    town.school.locker.phase = 6;
    town.school.player_position = (ShPathPoint){SH_Q12(-60), 0, SH_Q12(19.7f)};
    int count;
    const ShMapEvent *events = sh_school_map_events(save.map_idx, &count);
    const ShMapEvent *event = NULL;
    for (int i = 0; i < count; i++) {
        if (events[i].event_param == 13 && events[i].point_idx == 77) { event = &events[i]; break; }
    }
    assert(event && event->required_flag == 103 && event->complete_flag == 105);
    town.stage = SH_TOWN_EXPLORE;
    const ShInputState none = {0};
    for (int frame = 0; frame < 16 && town.stage == SH_TOWN_EXPLORE; frame++) {
        town_events_update(&town, &save, -60, -19.7f, 0, &none);
    }
    assert(town.stage == SH_TOWN_SCRIPT && town.school.callback == 13);
    const unsigned sounds_before = locker_body_sounds, doors_before = locker_door_sounds;
    unsigned frames = 0;
    bool waited = false;
    while (town.stage == SH_TOWN_SCRIPT && frames++ < 1000) {
        tick(&town, &save, dt, 0);
        if (town.stage == SH_TOWN_SCRIPT) {
            assert(town.controls_locked && town.school.player_hidden && town.school.drain_flare_suppressed);
            assert(!sh_save_event_get(&save, 107));
            if (town.step == 4 && !sh_save_event_get(&save, 106)) { waited = true; }
        }
        sh_school_world_update(&town, &save, -60, -19.7f, dt);
    }
    assert(frames < 1000 && waited && sh_save_event_get(&save, 105) && sh_save_event_get(&save, 106));
    assert(sh_save_event_get(&save, 107) && !sh_save_event_get(&save, 108));
    assert(!town.controls_locked && !town.school.player_hidden && !town.school.drain_flare_suppressed);
    assert(locker_body_sounds == sounds_before + 1 && locker_door_sounds == doors_before + 2);
    assert(town.school.card_player.rotation.vy == sh_fixed_atan2(SH_Q12(-61.4f) - SH_Q12(-60), SH_Q12(20.8f) - SH_Q12(19.7f)));
    assert(sh_school_prop_mask(&town, &save, -60, -20) & (1u << 7));

    const ShMapPoint *points = sh_school_map_points(save.map_idx, &count);
    const ShMapPoint *key_point = &points[82];
    const ShInputState action = {.down = SH_BTN_ACTION};
    for (int frame = 0; frame < 16 && town.stage == SH_TOWN_EXPLORE; frame++) {
        town_events_update(&town, &save, key_point->x, -key_point->z, -key_point->param0 * PI_F / 128.0f, &action);
    }
    assert(town.stage == SH_TOWN_SCRIPT && town.school.callback == 14);
    unsigned pickup_frames = 0;
    while (!town.prompt_open && pickup_frames++ < 1000) { tick(&town, &save, dt, 0); }
    assert(town.prompt_open && !sh_save_item_count(&save, 69));
    for (int frame = 0; frame < 8; frame++) { tick(&town, &save, dt, 0); }
    tick(&town, &save, dt, SH_BTN_ACTION);
    while (town.stage == SH_TOWN_SCRIPT && pickup_frames++ < 2000) { tick(&town, &save, dt, 0); }
    assert(town.stage == SH_TOWN_EXPLORE && sh_save_event_get(&save, 108) && sh_save_item_count(&save, 69) == 1);
    assert(!(sh_school_prop_mask(&town, &save, -60, -20) & (1u << 7)));

    const ShMapPoint *door = &points[34];
    const float yaw = -door->param0 * PI_F / 128.0f;
    uint8_t items[8];
    assert(sh_town_item_triggers(&town, &save, door->x, -door->z, yaw, items, 8) > 0);
    assert(sh_town_item_use(&town, &save, door->x, -door->z, yaw, 69));
    assert(town.school.callback == 3);
    unsigned use_frames = 0;
    while (!town.subtitle && use_frames++ < 1000) { tick(&town, &save, dt, 0); }
    assert(town.subtitle && sh_save_event_get(&save, 1311) && !sh_save_item_count(&save, 69));
    tick(&town, &save, dt, SH_BTN_ACTION);
    while (town.stage == SH_TOWN_SCRIPT && use_frames++ < 2000) { tick(&town, &save, dt, 0); }
    assert(town.stage == SH_TOWN_EXPLORE);
    town_events_update(&town, &save, door->x, -door->z, yaw, &action);
    assert(town.stage == SH_TOWN_LEAVING && town.overlay_sys_state == SH_SYS_LOAD_ROOM);
    float target_x, target_z, target_yaw;
    map_point_player_spawn(&points[37], &target_x, &target_z, &target_yaw);
    assert(town.overlay_x == target_x && town.overlay_z == target_z && town.overlay_yaw == target_yaw);
}

static void phone_scene_test(float dt, int skip_step)
{
    ShTown town = {.stage = SH_TOWN_EXPLORE};
    ShSavegame save = {.map_idx = SH_MAP_IDX_MAP1_S02};
    int count;
    const ShMapPoint *points = sh_school_map_points(save.map_idx, &count);
    const ShMapPoint *point = &points[91];
    float x = point->x, y = 0, z = -point->z, yaw = -point->param0 * PI_F / 128.0f;
    const ShInputState action = {.down = SH_BTN_ACTION};
    town_events_update(&town, &save, x, z, yaw, &action);
    assert(town.stage == SH_TOWN_SCRIPT && town.school.callback == 14 && !sh_save_event_get(&save, 98));
    const unsigned rings = phone_rings, voices = phone_voices, pickups = phone_pickups, returns = phone_returns;
    const unsigned animation_pickups = phone_animation_pickups, footsteps = phone_footsteps;
    unsigned frames = 0, wait_load = 0;
    bool skipped = false, held = false, picked_up = false, spoke = false;
    uint32_t seen = 0;
    sh_school_phone_assets(false, true);
    while (town.stage == SH_TOWN_SCRIPT && frames++ < 8000) {
        if (town.step >= 0 && town.step < 32) { seen |= 1u << town.step; }
        uint32_t down = 0;
        if (town.step == skip_step && !skipped) { down = KEY_START; skipped = true; }
        if (town.step == 12 && ++wait_load > 3) { sh_school_phone_assets(true, true); }
        const ShInputState input = {.down = down};
        bgm_flags = 0;
        fade_update(&town, dt);
        script_update(&town, &save, &x, &y, &z, &yaw, &input, dt);
        sh_border_update(&town.border, SH_Q12(dt));
        held |= town.school.phone.held && !town.school.phone.hidden;
        picked_up |= sh_save_event_get(&save, 99);
        spoke |= town.subtitle && strstr(town.subtitle, "Daddy");
    }
    assert(frames < 8000 && !town.controls_locked && !town.cam_active && !town.script_use_cam);
    assert(sh_save_event_get(&save, 98) && sh_save_event_get(&save, 122) && !sh_save_event_get(&save, 99));
    assert(!town.school.phone.held && !town.school.phone.locked && !town.subtitle);
    assert(phone_rings == rings + 1);
    if (skip_step < 0) {
        assert(held && picked_up && spoke && frames * dt > 25.0f);
        assert(phone_voices == voices + 1 && phone_pickups == pickups + 1 && phone_returns == returns + 1);
        assert(phone_animation_pickups == animation_pickups + 1 && phone_footsteps >= footsteps + 3);
        assert(phone_preloads && wait_load > 3 && (seen & (1u << 26)));
    } else if (skipped) {
        /* The retail player update can move once after the case28 warp. */
        const float distance = SH_Q12_TO_FLOAT(town.school.card_player.move_speed) * dt + 0.001f;
        assert(hypotf(x - SH_Q12_TO_FLOAT(SH_Q12(20.3f)), z + 143.5f) <= distance);
        assert(fabsf(sinf(yaw) - 1.0f) < 0.001f && fabsf(cosf(yaw)) < 0.001f);
        assert(town.school.phone.early_skip == (skip_step < 5));
    }
    sh_school_phone_assets(true, true);
}

static void arrival_scene_test(float dt, int skip_step)
{
    ShTown town = {.stage = SH_TOWN_EXPLORE};
    ShSavegame save = {.map_idx = SH_MAP_IDX_MAP1_S02};
    int count;
    const ShMapPoint *point = &sh_school_map_points(save.map_idx, &count)[1];
    float x = point->x, y = 0, z = -point->z, yaw = -point->param0 * PI_F / 128.0f;
    const ShInputState none = {0};
    town_events_update(&town, &save, x, z, yaw, &none);
    assert(town.stage == SH_TOWN_SCRIPT && town.school.callback == 5);
    const unsigned voices = arrival_voices;
    bool skipped = false, walked = false, animated = false;
    unsigned frames = 0;
    while (town.stage == SH_TOWN_SCRIPT && frames++ < 5000) {
        const bool skip = skip_step == town.step && !skipped;
        skipped |= skip;
        const ShInputState input = {.down = skip ? KEY_START : 0};
        bgm_flags = 0;
        fade_update(&town, dt);
        script_update(&town, &save, &x, &y, &z, &yaw, &input, dt);
        sh_border_update(&town.border, SH_Q12(dt));
        walked |= town.harry_state == 53;
        animated |= town.harry_state == 74;
    }
    assert(frames < 5000 && !town.controls_locked && !town.script_use_cam && !town.cam_active);
    assert(sh_save_event_get(&save, 91) && !sh_save_event_get(&save, 225));
    const ShAutosave *snapshot = sh_save_autosave_get();
    assert(snapshot->valid && sh_save_event_get(&snapshot->save, 225));
    assert(snapshot->position[0] == town.school.card_player.position.vx);
    assert(snapshot->save.map_idx == SH_MAP_IDX_MAP1_S02);
    assert(town.school.particle_mode == 6 && town.school.particle_reset);
    if (skip_step < 0) {
        assert(walked && animated && arrival_voices == voices + 3 && frames * dt > 10);
        assert(sh_save_event_get(&save, 92) && town.school.arrival_camera_stage == 1);
        assert(fabsf(x + 18.18f) < 0.2f && fabsf(z - 62.24f) < 0.2f);
    } else {
        assert(skipped && fabsf(x - SH_Q12_TO_FLOAT(SH_Q12(-18.18f))) < 0.001f);
        assert(z == -SH_Q12_TO_FLOAT(SH_Q12(-62.24f)) && fabsf(yaw - PI_F / 4) < 0.001f);
    }
}

int main(void)
{
    arrival_scene_test(1.0f / 30, -1);
    arrival_scene_test(1.0f / 60, -1);
    for (int step = 1; step < 10; step++) {
        arrival_scene_test(1.0f / 30, step);
        arrival_scene_test(1.0f / 60, step);
    }
    phone_scene_test(1.0f / 30, -1);
    phone_scene_test(1.0f / 60, -1);
    for (int step = 2; step < 26; step++) {
        phone_scene_test(1.0f / 30, step);
        phone_scene_test(1.0f / 60, step);
    }
    boss_solution_test(1.0f / 30);
    boss_solution_test(1.0f / 60);
    for (int point = 39; point <= 40; point++) {
        for (int direction = -1; direction < 2; direction++) {
            boss_scene_test(point, direction, 1.0f / 30);
            boss_scene_test(point, direction, 1.0f / 60);
        }
    }
    boss_world_test();
    bathroom_scene_test(1.0f / 30);
    bathroom_scene_test(1.0f / 60);
    locker_camera_test();
    school_routing_test();
    for (int skip = -1; skip <= 10; skip++) {
        if (skip >= 0 && skip < 3) { continue; }
        locker_open_test(1.0f / 30, skip);
        locker_open_test(1.0f / 60, skip);
    }
    locker_body_test(1.0f / 30);
    locker_body_test(1.0f / 60);
    for (int rate = 30; rate <= 60; rate *= 2) {
        ShTown town = {0};
        ShSavegame save = {0};
        save.map_idx = SH_MAP_IDX_MAP1_S03;
        sh_save_event_set(&save, 103);
        town.school.overlay = save.map_idx;
        town.school.valve.rotation = 1234;
        town.school.valve_active = true;
        int count;
        const ShMapEvent *events = sh_school_map_events(save.map_idx, &count), *event = NULL;
        for (int i = 0; i < count; i++) {
            if (events[i].sys_state == SH_SYS_EVENT_CALLBACK && events[i].event_param == 11) { event = &events[i]; }
        }
        assert(event && event->point_idx == 76 && sh_school_event_enabled(event, &save));
        assert(sh_school_begin(&town, &save, event));
        assert(town.school.valve.rotation == 1234 && town.school.valve_active);
        int frames = 0, waiting = 0;
        bool saw_image = false;
        while (town.stage == SH_TOWN_SCRIPT && frames++ < rate * 6) {
            uint32_t down = 0;
            if (town.school.substep == 4) {
                assert(town.subtitle || !waiting);
                assert(town.school.message == 43);
                if (++waiting > rate) { down = SH_BTN_ACTION; }
            }
            tick(&town, &save, 1.0f / rate, down);
            const char *image = sh_school_background(&town);
            if (image) { assert(!strcmp(image, "LOCKER1.TIM") && town.school.brightness == 128); saw_image = true; }
            if (town.stage == SH_TOWN_SCRIPT) { assert(town.fade_speed == 2.5f); }
        }
        assert(frames < rate * 6 && saw_image && waiting > rate && !town.controls_locked);
        assert(!sh_school_background(&town) && town.school.valve.rotation == 1234 && town.school.valve_active);
        for (unsigned flag = 100; flag <= 108; flag++) { assert(sh_save_event_get(&save, flag) == (flag == 103)); }
    }
    assert(!strcmp(sh_inv_item_name(69), "Library reserve key"));
    for (int rate = 30; rate <= 60; rate *= 2) {
        for (int accept = 0; accept <= 1; accept++) {
            ShTown town = {0};
            ShSavegame save = {0};
            save.map_idx = SH_MAP_IDX_MAP1_S03;
            int count;
            const ShMapEvent *events = sh_school_map_events(save.map_idx, &count), *event = NULL;
            for (int i = 0; i < count; i++) {
                if (events[i].sys_state == SH_SYS_EVENT_CALLBACK && events[i].event_param == 14) { event = &events[i]; }
            }
            assert(event && event->point_idx == 82);
            sh_save_event_set(&save, event->required_flag);
            assert(sh_school_event_enabled(event, &save) && sh_school_begin(&town, &save, event));
            bool crouch = false, stand = false;
            int frames = 0, waits = 0;
            for (; town.stage == SH_TOWN_SCRIPT && frames < rate * 10; frames++) {
                uint32_t down = 0;
                if (town.step == 4 && town.prompt_open) {
                    if (++waits >= 8) { down = accept ? SH_BTN_ACTION : SH_BTN_CANCEL; }
                }
                if (town.step < 4) { assert(!sh_save_event_get(&save, 108)); }
                tick(&town, &save, 1.0f / rate, down);
                if (town.harry_state == 59 || town.harry_state == 60) {
                    int keyframe;
                    assert(sh_school_harry_keyframe(&town, &keyframe));
                    assert(keyframe == town.school.card_player.model.keyframe);
                    crouch |= town.harry_state == 59;
                    stand |= town.harry_state == 60;
                }
                if (!accept && town.step == 6) { assert(!sh_save_event_get(&save, 108)); }
            }
            assert(frames < rate * 10 && waits >= 8 && crouch && stand);
            assert(sh_save_event_get(&save, 108) == (accept != 0));
            assert(sh_save_item_count(&save, 69) == accept);
            assert(!town.controls_locked && !town.pickup_active && !town.prompt_open);
            assert(sh_school_event_enabled(event, &save) == (accept == 0));
        }
    }
    for (int rate = 30; rate <= 60; rate *= 2) {
        int count, point_count;
        const ShMapEvent *events = sh_school_map_events(SH_MAP_IDX_MAP1_S03, &count);
        const ShMapPoint *points = sh_school_map_points(SH_MAP_IDX_MAP1_S03, &point_count);
        unsigned tested = 0;
        for (int i = 0; i < count; i++) {
            const ShMapEvent *event = &events[i];
            if (event->sys_state != SH_SYS_EVENT_CALLBACK || (event->event_param != 3 && event->event_param != 4)) { continue; }
            unsigned item = event->required_item;
            for (int j = 0; !item && j < count; j++) {
                if (events[j].event_param == event->event_param && events[j].point_idx == event->point_idx && events[j].required_item) { item = events[j].required_item; }
            }
            assert(item && event->point_idx < point_count);
            ShTown town = {0};
            ShSavegame save = {0};
            save.map_idx = SH_MAP_IDX_MAP1_S03;
            sh_save_event_set(&save, event->required_flag);
            sh_save_item_add(&save, item, 3);
            const unsigned before = roof_key_sounds;
            assert(sh_school_begin(&town, &save, event));
            tick(&town, &save, 1.0f / rate, 0);
            assert(sh_save_item_count(&save, item) == 2 && sh_save_event_get(&save, event->complete_flag));
            assert(roof_key_sounds == before + 1);
            assert(roof_key_sound_position[0] == points[event->point_idx].x && roof_key_sound_position[2] == -points[event->point_idx].z);
            tick(&town, &save, 1.0f / rate, 0);
            assert(sh_save_item_count(&save, item) == 1);
            for (int frame = 0; frame < rate; frame++) { tick(&town, &save, 1.0f / rate, 0); }
            assert(town.stage == SH_TOWN_SCRIPT && town.subtitle && !sh_save_item_count(&save, item));
            tick(&town, &save, 1.0f / rate, SH_BTN_ACTION);
            tick(&town, &save, 1.0f / rate, 0);
            assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && roof_key_sounds == before + 1);
            tested++;
        }
        assert(tested == 4);
    }
    for (int ball = 0; ball <= 1; ball++) {
        ShTown town = {0};
        ShSavegame save = {0};
        save.map_idx = SH_MAP_IDX_MAP1_S03;
        if (ball) { sh_save_event_set(&save, 111); }
        town.school.overlay = save.map_idx;
        town.school.callback = 8;
        town.stage = SH_TOWN_SCRIPT;
        town.script = SH_TOWN_SCR_SCHOOL;
        tick(&town, &save, 1.0f / 30, 0);
        town.step = 4;
        tick(&town, &save, 1.0f / 30, 0);
        town.step = 8;
        tick(&town, &save, 1.0f / 30, 0);
        assert(town.step == 9 && town.school.player_hidden && town.school.drain_light_override);
        const ShRoofWater *water = sh_school_roof_water(&town);
        assert(water && water->pool.count == SH_ROOF_WATER_POOL_COUNT);
        unsigned active = 0;
        for (int i = 0; i < water->pool.count; i++) { active += water->particles[i].field_A == 27 || water->particles[i].field_A == 28; }
        assert(active == (ball ? 290u : 160u));
        ShEnvState environment = {0};
        environment.preset.flags[1] = 128;
        environment.preset.world_light = 128 * 32;
        sh_school_roof_water_update(&town, 0, &environment, NULL, 1.0f / 30);
        assert(water->surface_count > 0 && water->surface_count <= SH_WATER_SURFACE_MAX_QUADS);
        assert(!sh_save_event_get(&save, 113));
        town.step = 11;
        tick(&town, &save, 1.0f / 30, 0);
        assert(town.step == 12 && !town.school.player_hidden && !town.school.drain_light_override);
        assert(!sh_school_roof_water(&town) && sh_save_event_get(&save, 113) == (ball != 0));
        int keyframe;
        assert(sh_school_harry_keyframe(&town, &keyframe));
        assert(keyframe == town.school.card_player.model.keyframe);
        int ticks = 0;
        while (town.stage == SH_TOWN_SCRIPT && ticks++ < 300) { tick(&town, &save, 1.0f / 30, 0); }
        assert(ticks < 300 && !town.controls_locked && town.stage == SH_TOWN_EXPLORE);
    }
    for (int step = 7; step <= 12; step += 5) {
        for (int phase = 0; phase <= 2; phase += 2) {
            ShTown town = {0};
            ShSavegame save = {0};
            const ShInputState input = {0};
            save.map_idx = SH_MAP_IDX_MAP1_S03;
            town.school.overlay = save.map_idx;
            town.school.callback = 8;
            town.stage = SH_TOWN_SCRIPT;
            town.script = SH_TOWN_SCR_SCHOOL;
            town.step = step;
            town.school.substep = 1;
            town.school.valve_animation_calls = 3;
            assert(sh_path_school_animation_start(&town.school.card_player, step == 7 ? 107 : 108));
            town.school.card_player.model.animation_status |= 1;
            int first;
            assert(sh_path_animation_first(&town.school.card_player, &first));
            town.school.card_player.model.time = (first + 12) * 4096;
            town.school.card_player.model.keyframe = first + 12;
            ShCharaBone hand = {0};
            for (int axis = 0; axis < 3; axis++) { hand.rotation[axis][axis] = 4096; }
            hand.translation[1] = -128;
            hand.translation[2] = -3000;
            sh_school_seed_harry_hand(&town, &hand);
            const int angle = sh_drain_valve_hand_angle(&hand);
            town.school.valve.previous_angle = step == 7 ? angle - 128 : angle + 128;
            town.school.valve.sound_phase = phase;
            town.school.valve.running = step == 12;
            town.school.valve.opening = 2048;
            ShDrainValve expected = town.school.valve;
            sh_drain_valve_hand(&expected, step, angle, 12);
            const unsigned squeaks = valve_squeaks, clicks = valve_clicks;
            const unsigned starts = valve_water_starts, stops = valve_water_stops;
            const unsigned volume_updates = valve_volume_updates;
            sh_school_update(&town, &save, &input, 0);
            assert(town.school.valve.rotation == expected.rotation && town.school.valve.opening == expected.opening);
            assert(town.school.valve.sound_phase == expected.sound_phase && town.school.valve.running == expected.running);
            assert(valve_squeaks == squeaks + (phase == 0) && valve_clicks == clicks + (phase == 2));
            assert(valve_water_starts == starts + (phase == 2 && step == 7));
            assert(valve_water_stops == stops + (phase == 2 && step == 12));
            assert(valve_volume_updates == volume_updates + (expected.running != 0));
            if (expected.running) {
                const ShPathPoint camera = {0};
                assert(valve_attenuation == sh_drain_water_attenuation(sh_drain_water_distance(town.school.player_position, camera), expected.opening));
            }
            assert(!sh_save_event_get(&save, 113));
        }
    }
    for (int rate = 30; rate <= 60; rate *= 2) {
        for (int accept = 0; accept <= 1; accept++) {
            ShTown town = {0};
            ShSavegame save = {0};
            save.map_idx = SH_MAP_IDX_MAP1_S03;
            town.stage = SH_TOWN_SCRIPT;
            town.script = SH_TOWN_SCR_SCHOOL;
            town.school.overlay = save.map_idx;
            town.school.callback = 8;
            const float dt = 1.0f / rate;
            tick(&town, &save, dt, 0);
            assert(town.step == 2 && town.controls_locked && sh_save_event_get(&save, 1364));
            tick(&town, &save, dt, 0);
            assert(town.step == 2 && town.prompt_open && town.subtitle);
            tick(&town, &save, dt, accept ? SH_BTN_ACTION : SH_BTN_CANCEL);
            assert(town.step == (accept ? 4 : 3) && !town.prompt_open);
            tick(&town, &save, dt, 0);
            if (!accept) {
                assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.school.player_warp);
            } else {
                assert(town.step == 5 && town.school.player_warp && town.school.empty_hand);
                assert(town.school.valve_active && town.cam_active && town.cam.fov_deg == 53.13f);
                int ticks = 0;
                while (town.step == 5 && ticks++ < rate * 2) { tick(&town, &save, dt, 0); }
                assert(town.step == 6 && ticks < rate * 2);
                tick(&town, &save, dt, 0);
                assert(town.step == 7 && town.harry_state == 107 && town.school.valve_animation_calls == 1);
            }
            assert(!sh_save_event_get(&save, 113));
            int count;
            const ShMapEvent *events = sh_school_map_events(save.map_idx, &count);
            bool found = false;
            for (int i = 0; i < count; i++) {
                if (events[i].event_param == 8 && events[i].point_idx == 21) {
                    ShTown fresh = {0};
                    assert(sh_school_begin(&fresh, &save, &events[i]));
                    found = true;
                }
            }
            assert(found);
        }
    }
    {
        ShTown town = {0};
        ShSavegame save = {0};
        save.map_idx = SH_MAP_IDX_MAP1_S03;
        int count;
        const ShSchoolProp *props = sh_school_map_props(save.map_idx, &count);
        assert(count == 8 && !strcmp(props[0].model, "VALVE_HI") && !strcmp(props[1].model, "BALL_HID"));
        for (int ball = 0; ball <= 1; ball++) {
            if (ball) { sh_save_event_set(&save, 111); }
            for (int x = -160; x <= 160; x++) {
                for (int z = -160; z <= 160; z++) {
                    const bool visible = x > -80 && x <= 0 && z >= 0 && z < 80;
                    assert(sh_school_prop_mask(&town, &save, x * 0.5f, z * 0.5f) == (visible ? (ball ? 3u : 1u) : 0u));
                }
            }
        }
        town.school.valve_active = town.school.locker_render_active = true;
        for (unsigned mask = 0; mask < 32; mask++) {
            town.school.locker_frame.draw_mask = mask;
            for (int seen = 0; seen <= 1; seen++) {
                for (int taken = 0; taken <= 1; taken++) {
                    if (seen) { sh_save_event_set(&save, 107); } else { sh_save_event_clear(&save, 107); }
                    if (taken) { sh_save_event_set(&save, 108); } else { sh_save_event_clear(&save, 108); }
                    const uint32_t expected = (mask << 2) | (seen && !taken ? props[7].mask : 0);
                    assert(sh_school_prop_mask(&town, &save, -60, -20) == expected);
                    assert(sh_school_roof_moving_mask(&town, &save, -60, -20) == ((mask & 7) << 2));
                    assert(sh_school_roof_moving_mask(&town, &save, -10, 10) == props[0].mask);
                    assert(!sh_school_prop_mask(&town, &save, -80, -20));
                    assert(!sh_school_prop_mask(&town, &save, -60, 0));
                }
            }
        }
        town.school.overlay = save.map_idx;
        town.school.locker.phase = 4;
        town.school.locker.speed = -123;
        town.school.locker.timer = 456;
        town.school.locker.rotation[1] = -789;
        const ShLockerWorld locker = town.school.locker;
        const uint32_t draw_mask = town.school.locker_frame.draw_mask;
        const ShMapEvent read = {.sys_state = SH_SYS_READ_MESSAGE};
        assert(sh_school_begin(&town, &save, &read));
        assert(!memcmp(&locker, &town.school.locker, sizeof(locker)) && town.school.locker_frame.draw_mask == draw_mask);
        assert(town.school.locker_render_active);
        sh_school_roof_world_reset(&town);
        assert(!town.school.locker.phase && !town.school.locker.speed && !town.school.locker.timer);
        assert(!town.school.locker.rotation[1] && town.school.locker_frame.draw_mask == 7);
        assert(!town.school.locker_render_active);
    }
    {
        ShTown town = {0};
        ShSavegame save = {0};
        save.map_idx = 4;
        int count;
        const ShSchoolProp *props = sh_school_map_props(4, &count);
        assert(count == 8 && strcmp(props[0].model, "HAND_HID") == 0);
        assert(strcmp(props[3].model, "ACID_HID") == 0);
        assert(sh_school_prop_mask(&town, &save, -59.2f, -99.5f) == 1u);
        sh_save_event_set(&save, 74);
        assert(sh_school_prop_mask(&town, &save, -59.2f, -99.5f) == 6u);
        sh_save_event_set(&save, 69);
        assert(sh_school_prop_mask(&town, &save, -59.2f, -99.5f) == 2u);
        assert(sh_school_prop_mask(&town, &save, -101.2f, -100.3f) == 8u);
        sh_save_event_set(&save, 68);
        assert(sh_school_prop_mask(&town, &save, -101.2f, -100.3f) == 0u);
        assert(sh_school_prop_mask(&town, &save, 0.0f, 0.0f) == 0u);
        assert(strcmp(props[4].model, "FUTA1_HI") == 0 && strcmp(props[5].model, "FUTA2_HI") == 0);
        assert(strcmp(props[6].model, "SILVER_H") == 0 && strcmp(props[7].model, "SILVER_H") == 0);
        sh_save_event_clear(&save, 75);
        for (unsigned flags = 0; flags < 8; flags++) {
            if (flags & 1) { sh_save_event_set(&save, 71); } else { sh_save_event_clear(&save, 71); }
            if (flags & 2) { sh_save_event_set(&save, 75); } else { sh_save_event_clear(&save, 75); }
            if (flags & 4) { sh_save_event_set(&save, 70); } else { sh_save_event_clear(&save, 70); }
            const uint32_t lid = flags & 1 ? 16u : 32u;
            const uint32_t silver = flags & 4 ? 0u : flags & 2 ? 128u : 64u;
            assert(sh_school_prop_mask(&town, &save, -100.0f, -20.0f) == (lid | silver));
            assert(sh_school_prop_mask(&town, &save, -79.0f, -20.0f) == 0u);
            assert(sh_school_prop_mask(&town, &save, -121.0f, -20.0f) == 0u);
        }
        sh_save_event_set(&save, 71);
        sh_save_event_set(&save, 75);
        sh_save_event_clear(&save, 70);
        town.school.silver_active = true;
        assert(sh_school_prop_mask(&town, &save, -100.0f, -20.0f) == (16u | 64u));
        town.school.silver_active = false;
        assert(sh_school_prop_mask(&town, &save, -100.0f, -20.0f) == (16u | 128u));
    }
    const int callbacks[] = {4, 5, 7, 8, 12, 13, 14, 17, 21};
    for (int rate = 30; rate <= 60; rate += 30) {
        for (int callback = 15; callback <= 16; callback++) {
            for (unsigned branch = 0; branch < 2; branch++) {
                ladder_test(callback, branch, -1, 1.0f / rate);
                ladder_test(callback, branch, callback == 15 ? 3 : 4, 1.0f / rate);
                ladder_test(callback, branch, callback == 15 ? 4 : 5, 1.0f / rate);
            }
        }
        for (int route = 0; route < 3; route++) { piano_scene_test(1.0f / rate, route, -1); }
        for (int skip = 10; skip <= 18; skip++) { piano_scene_test(1.0f / rate, 0, skip); }
        acid_test(1.0f / rate, -1);
        for (int skip = 1; skip < 8; skip++) { acid_test(1.0f / rate, skip); }
        for (unsigned i = 0; i < sizeof callbacks / sizeof callbacks[0]; i++) {
            inspection_test(callbacks[i], 1.0f / rate);
        }
        for (int callback = 0; callback <= 2; callback++) {
            door_test(callback, 1.0f / rate);
        }
        int count;
        const ShMapEvent *events = sh_school_events(&count);
        for (int i = 0; i < count; i++) {
            if (events[i].activation_type == SH_ACT_ITEM) {
                medallion_use_test(&events[i], 1.0f / rate);
            }
        }
        for (unsigned flags = 0; flags < 4; flags++) {
            clock_test(flags, false, 1.0f / rate);
        }
        clock_test(0, true, 1.0f / rate);
        map_take_test(false, 1.0f / rate);
        map_take_test(true, 1.0f / rate);
        upper_pickup_test(5, false, 1.0f / rate);
        upper_pickup_test(5, true, 1.0f / rate);
        upper_pickup_test(7, false, 1.0f / rate);
        upper_pickup_test(7, true, 1.0f / rate);
        upper_pickup_test(11, false, 1.0f / rate);
        upper_pickup_test(11, true, 1.0f / rate);
        upper_inspection_test(4, 1.0f / rate);
        upper_inspection_test(8, 1.0f / rate);
        upper_inspection_test(10, 1.0f / rate);
        upper_inspection_test(12, 1.0f / rate);
        for (int powered = 0; powered <= 1; powered++) {
            for (int accept = 0; accept <= 1; accept++) {
                boiler_test(powered, accept, 1.0f / rate);
            }
        }
    }
    message_test();
    alternative_message_test();
    alternative_inspections_test(1.0f / 30);
    alternative_inspections_test(1.0f / 60);
    alternative_book_test(1.0f / 30);
    alternative_book_test(1.0f / 60);
    alternative_lobby_key_test(1.0f / 30);
    alternative_lobby_key_test(1.0f / 60);
    alternative_card_test(1.0f / 30);
    alternative_card_test(1.0f / 60);
    for (int callback = 9; callback <= 10; callback++) {
        for (int seen = 0; seen <= 1; seen++) {
            roof_books_test(callback, seen, 1.0f / 30);
            roof_books_test(callback, seen, 1.0f / 60);
        }
    }
    for (int point = 19; point <= 20; point++) {
        roof_rubber_ball_test(point, 1.0f / 30);
        roof_rubber_ball_test(point, 1.0f / 60);
    }
    for (int callback = 6; callback <= 7; callback++) {
        for (int point = 17; point <= 20; point++) {
            for (unsigned flags = 0; flags < 8; flags++) {
                for (int item = 0; item <= 1; item++) {
                    roof_drain_inspection_test(callback, point, flags, item, 1.0f / 30);
                    roof_drain_inspection_test(callback, point, flags, item, 1.0f / 60);
                }
            }
        }
    }
    for (int accept = 0; accept <= 1; accept++) {
        alternative_animated_pickup_test(17, 162, 6, accept, 1.0f / 30);
        alternative_animated_pickup_test(17, 162, 6, accept, 1.0f / 60);
        alternative_animated_pickup_test(20, 70, 1, accept, 1.0f / 30);
        alternative_animated_pickup_test(20, 70, 1, accept, 1.0f / 60);
    }
    for (int seen = 0; seen <= 1; seen++) {
        alternative_slot_test(seen, 1.0f / 30);
        alternative_slot_test(seen, 1.0f / 60);
    }
    for (int callback = 12; callback <= 13; callback++) {
        for (int accept = 0; accept <= 1; accept++) {
            alternative_pickup_test(callback, accept, 1.0f / 30);
            alternative_pickup_test(callback, accept, 1.0f / 60);
        }
    }
    flags_and_transition_test();
    shade_test();
    dispatch_test();
    map_image_test();
    upper_transitions_test();
    assert(warps > 0);
    puts("PASS: school progression including alternative arrival/phone full and skip branches at 30/60 fps");
    return 0;
}
