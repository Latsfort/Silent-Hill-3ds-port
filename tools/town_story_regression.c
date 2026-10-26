#include "../source/game/town.c"
#include "sh3ds/event_image.h"

#include <assert.h>
#include <stdio.h>

bool sh_input_down(const ShInputState *input, uint32_t key)
{
    return input && (input->down & key) != 0;
}

void sh_vc_warp(ShVc *vc)
{
    (void)vc;
}

static unsigned lock_sounds;

void sh_snd_play(uint16_t sfx, int balance, int atten)
{
    assert(sfx == 1390 && balance == 0 && atten == 128);
    lock_sounds++;
}

void sh_snd_at(uint16_t sfx, const float pos[3], float volume)
{
    (void)sfx;
    (void)pos;
    (void)volume;
}

static void woodman_test(bool accept, bool marked, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    ShInputState input = {0};
    sh_save_init(&save);
    if (marked) {
        sh_save_event_set(&save, SH_RETAIL_EventFlag_MapMark_OldTown_DogYardKeyLine);
    }
    script_begin(&town, SH_TOWN_SCR_WOODMAN_KEY, 0);
    for (int frame = 0; frame < 600 && !town.prompt_open; frame++) {
        if (town.harry_state) {
            town.harry_time += dt;
        }
        woodman_update(&town, &save, &input, dt);
    }
    assert(town.step == 5 && town.prompt_open && town.pickup_active && town.controls_locked);
    assert(sh_save_event_get(&save, SH_RETAIL_EventFlag_M2S00_PickupKeyOfWoodman));
    assert(!sh_save_event_get(&save, SH_RETAIL_EventFlag_MapMark_55));
    int keyframe;
    assert(sh_town_harry_keyframe(&town, &keyframe) && keyframe == SH_TOWN_STATE_59_LAST);
    for (int frame = 0; frame < 600; frame++) {
        woodman_update(&town, &save, &input, dt);
    }
    assert(town.step == 5 && sh_save_item_count(&save, SH_RETAIL_InvItemId_KeyOfWoodman) == 0);
    input.down = accept ? SH_BTN_ACTION : SH_BTN_CANCEL;
    woodman_update(&town, &save, &input, dt);
    assert(town.step == (accept ? 6 : 7));
    input.down = 0;
    for (int frame = 0; frame < 600 && town.stage == SH_TOWN_SCRIPT; frame++) {
        if (town.harry_state) {
            town.harry_time += dt;
        }
        woodman_update(&town, &save, &input, dt);
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.pickup_active);
    assert(sh_save_item_count(&save, SH_RETAIL_InvItemId_KeyOfWoodman) == accept);
    assert(sh_save_event_get(&save, SH_RETAIL_EventFlag_M2S00_PickupKeyOfWoodman) == accept);
    assert(sh_save_event_get(&save, SH_RETAIL_EventFlag_MapMark_55) == (accept && marked));
}

static void locks_test(unsigned keys, float dt)
{
    ShTown town = {0};
    ShSavegame save;
    ShInputState input = {0};
    sh_save_init(&save);
    if (keys & 4u) {
        sh_save_event_set(&save, SH_RETAIL_EventFlag_M2S00_PickupKeyOfLion);
        sh_save_item_add(&save, SH_RETAIL_InvItemId_KeyOfLion, 1);
    }
    if (keys & 2u) {
        sh_save_event_set(&save, SH_RETAIL_EventFlag_M2S00_PickupKeyOfWoodman);
        sh_save_item_add(&save, SH_RETAIL_InvItemId_KeyOfWoodman, 1);
    }
    if (keys & 1u) {
        sh_save_event_set(&save, SH_RETAIL_EventFlag_M2S00_PickupKeyOfScarecrow);
        sh_save_item_add(&save, SH_RETAIL_InvItemId_KeyOfScarecrow, 1);
    }
    lock_sounds = 0;
    script_begin(&town, SH_TOWN_SCR_ECLIPSE_LOCKS, 0);
    bool final_message = false;
    for (int frame = 0; frame < 2400 && town.stage == SH_TOWN_SCRIPT; frame++) {
        fade_update(&town, dt);
        input.down = town.step == 7 || town.step == 11 ? SH_BTN_ACTION : 0;
        if (town.step == 11) {
            final_message = true;
            assert(eclipse_lock_image(&save) == keys);
        }
        eclipse_locks_update(&town, &save, &input, dt);
    }
    assert(final_message && town.stage == SH_TOWN_EXPLORE && !town.controls_locked);
    assert(sh_save_event_get(&save, SH_RETAIL_EventFlag_M2S00_DoorOfEclypseOpen) == (keys == 7));
    assert(sh_save_event_get(&save, SH_RETAIL_EventFlag_MapMark_OldTown_DoghouseArrowsOnly) == (keys == 7));
    assert(sh_save_item_count(&save, SH_RETAIL_InvItemId_KeyOfLion) == 0);
    assert(sh_save_item_count(&save, SH_RETAIL_InvItemId_KeyOfWoodman) == 0);
    assert(sh_save_item_count(&save, SH_RETAIL_InvItemId_KeyOfScarecrow) == 0);
    assert(lock_sounds == ((keys >> 2) & 1u) + ((keys >> 1) & 1u) + (keys & 1u));
}

static void image_test(void)
{
    const uint8_t red[] = {31, 128};
    const uint8_t blue[] = {0, 252};
    const ShTim from = {.bpp = 16, .width = 1, .height = 1, .stride = 2, .pixels = red};
    const ShTim to = {.bpp = 16, .width = 1, .height = 1, .stride = 2, .pixels = blue};
    ShTim output;
    uint8_t pixel[2];
    assert(sh_event_image_transition(&output, pixel, sizeof pixel, &from, &to, 0.0f));
    assert(sh_tim_pixel_rgba(&output, 0, 0) == sh_tim_pixel_rgba(&from, 0, 0));
    assert(sh_event_image_transition(&output, pixel, sizeof pixel, &from, &to, 1.0f));
    assert(sh_tim_pixel_rgba(&output, 0, 0) == sh_tim_pixel_rgba(&to, 0, 0));
    assert(sh_event_image_transition(&output, pixel, sizeof pixel, &from, &to, 0.5f));
    assert((pixel[0] & 31u) == 16u && ((pixel[1] >> 2) & 31u) == 15u);
    assert(!sh_event_image_transition(&output, pixel, 1, &from, &to, 0.5f));
}

static void image_assets_test(void)
{
    uint8_t data[80u * 1024u];
    for (unsigned image = 0; image < sizeof(s_town_lock_images) / sizeof(s_town_lock_images[0]); image++) {
        char path[80];
        snprintf(path, sizeof path, "romfs/game/tim/%s", s_town_lock_images[image]);
        FILE *file = fopen(path, "rb");
        assert(file);
        const size_t size = fread(data, 1, sizeof data, file);
        assert(!ferror(file));
        fclose(file);
        ShTim tim;
        assert(sh_tim_parse(data, size, &tim));
        assert(tim.width == 320 && tim.height == 240);
    }
}

static void door_priority_test(void)
{
    ShTown town = {0};
    ShSavegame save;
    ShInputState input = {.down = SH_BTN_ACTION};
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP2_S00;
    const ShMapEvent *exit = &s_map_events_map2_s00[72];
    const ShMapPoint *point = &s_map_points_map2_s00[exit->point_idx];
    sh_save_event_set(&save, (unsigned)exit->required_flag);
    town_events_update(&town, &save, point->x, -point->z, 0.0f, &input);
    assert(town.stage == SH_TOWN_LEAVING);
    assert(town.overlay_spawn_pt == exit->event_param && town.overlay_complete_flag == exit->complete_flag);
    assert(!sh_save_event_get(&save, (unsigned)exit->complete_flag));

    memset(&town, 0, sizeof town);
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP2_S00;
    town_events_update(&town, &save, point->x, -point->z, 0.0f, &input);
    assert(town.stage == SH_TOWN_SCRIPT && town.script == SH_TOWN_SCR_ECLIPSE_LOCKS);
}

static void eclipse_tick(ShTown *town, ShSavegame *save, float *x, float *z, float *yaw,
                          ShInputState *input, float dt)
{
    fade_update(town, dt);
    eclipse_enter_update(town, save, x, z, yaw, input, dt);
    sh_env_update(&town->environment, sh_save_flashlight_on(save), dt);
}

static void eclipse_enter_test(float dt, bool early_message)
{
    ShTown town = {0};
    ShSavegame save;
    ShInputState input = {0};
    float x = 0.0f;
    float z = 0.0f;
    float yaw = 0.0f;
    sh_save_init(&save);
    save.map_idx = SH_MAP_IDX_MAP2_S00;
    sh_save_item_add(&save, SH_ITEM_FLASHLIGHT, 1);
    sh_save_flashlight_set(&save, true);
    sh_save_event_set(&save, SH_RETAIL_EventFlag_158);
    town_events_update(&town, &save, x, z, yaw, &input);
    assert(town.script == SH_TOWN_SCR_ECLIPSE_ENTER && town.stage == SH_TOWN_SCRIPT);
    town.fade = 1.0f;
    town.fade_target = 1.0f;
    eclipse_tick(&town, &save, &x, &z, &yaw, &input, dt);
    assert(town.step == 1 && town.controls_locked && town.letterbox);
    assert(x == SH_Q12_TO_FLOAT(SH_Q12(147.7f)) && z == -SH_Q12_TO_FLOAT(SH_Q12(376.5f)));
    assert(yaw == PI_F * 0.5f && town.environment.active && town.environment.transitioning);
    assert(!sh_save_event_get(&save, SH_RETAIL_EventFlag_159));
    float elapsed = dt;
    bool acknowledged = false;
    for (int frame = 0; frame < 1000 && town.step != 3; frame++) {
        if (early_message && town.subtitle && !acknowledged) {
            assert(strcmp(town.subtitle, map_msg_text(35)) == 0);
            input.down = SH_BTN_ACTION;
            acknowledged = true;
        } else {
            input.down = 0;
        }
        eclipse_tick(&town, &save, &x, &z, &yaw, &input, dt);
        elapsed += dt;
    }
    assert(town.step == 3 && town.read_msg_idx == 36 && !sh_save_flashlight_on(&save));
    assert(town.environment.channel[0].preset.world_light == sh_env_preset(6)->world_light);
    assert(early_message ? (acknowledged && elapsed < 3.0f) : elapsed > 6.2f);
    input.down = 0;
    for (int frame = 0; frame < 300; frame++) {
        eclipse_tick(&town, &save, &x, &z, &yaw, &input, dt);
    }
    assert(town.step == 3 && town.read_msg_idx == 36 && town.environment.flashlight == 0);
    input.down = SH_BTN_ACTION;
    eclipse_tick(&town, &save, &x, &z, &yaw, &input, dt);
    assert(town.step == 3 && town.read_msg_idx == 37);
    assert(strcmp(town.subtitle, map_msg_text(37)) == 0);
    input.down = SH_BTN_CANCEL;
    eclipse_tick(&town, &save, &x, &z, &yaw, &input, dt);
    assert(town.step == 3 && town.read_msg_idx == 37);
    input.down = SH_BTN_ACTION;
    eclipse_tick(&town, &save, &x, &z, &yaw, &input, dt);
    assert(town.step == 4 && !sh_save_event_get(&save, SH_RETAIL_EventFlag_159));
    input.down = 0;
    eclipse_tick(&town, &save, &x, &z, &yaw, &input, dt);
    assert(town.step == 5 && sh_save_flashlight_on(&save));
    assert(town.environment.channel[1].preset.spot_light == sh_env_preset(3)->spot_light);
    for (int frame = 0; frame < 300 && town.stage == SH_TOWN_SCRIPT; frame++) {
        eclipse_tick(&town, &save, &x, &z, &yaw, &input, dt);
    }
    assert(town.stage == SH_TOWN_EXPLORE && !town.controls_locked && !town.letterbox);
    assert(sh_save_event_get(&save, SH_RETAIL_EventFlag_159));
    assert(town.environment.flashlight == SH_Q12_ONE);
    town_events_update(&town, &save, x, z, yaw, &input);
    assert(town.stage == SH_TOWN_EXPLORE && town.script == SH_TOWN_SCR_NONE);
}

int main(void)
{
    for (int rate = 30; rate <= 60; rate += 30) {
        for (int accept = 0; accept <= 1; accept++) {
            for (int marked = 0; marked <= 1; marked++) {
                woodman_test(accept, marked, 1.0f / rate);
            }
        }
        for (unsigned keys = 0; keys < 8; keys++) {
            locks_test(keys, 1.0f / rate);
        }
        eclipse_enter_test(1.0f / rate, false);
        eclipse_enter_test(1.0f / rate, true);
    }
    image_test();
    image_assets_test();
    door_priority_test();
    puts("PASS: Woodman/locks/Eclipse timing and dialogue at 30/60 fps, image blending, and door priority");
    return 0;
}
