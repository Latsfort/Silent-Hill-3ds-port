#include "sh3ds/alley_progress.h"
#include "sh3ds/controls.h"
#include "sh3ds/fixed.h"
#include "sh3ds/snd.h"
#include "sh3ds/sound.h"
#include "sh3ds/coll.h"

#include <3ds.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

bool sh_input_down(const ShInputState *input, uint32_t key) { return (input->down & key) != 0; }
void sh_snd_play(uint16_t id, int balance, int attenuation) { (void)id; (void)balance; (void)attenuation; }
void sh_snd_stop(uint16_t id) { (void)id; }
void sh_snd_call(uint32_t command) { (void)command; }
void sh_bgm_status_set(unsigned flags) { (void)flags; }
bool sh_bgm_crossfade(int index) { (void)index; return true; }
void sh_xa_preload(uint16_t command) { (void)command; }
void sh_snd_at_pitch(uint16_t id, const float position[3], float volume, int pitch)
{ (void)id; (void)position; (void)volume; (void)pitch; }

static void run(float dt)
{
    ShAlley alley = {0};
    ShSavegame save;
    ShInputState input = {0};
    float x = -62.0f, y = 0.0f, z = -118.0f, yaw = 3.14159265358979323846f;
    sh_save_init(&save);
    sh_alley_reset(&alley);
    alley.stage = SH_ALLEY_ROAM;
    for (int i = 0; i < (int)(10.0f / dt); i++) {
        sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
        assert(alley.stage == SH_ALLEY_ROAM);
        assert(!sh_save_event_get(&save, 15));
    }
    input.down = KEY_Y;
    sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
    assert(alley.stage == SH_ALLEY_ROAM && !alley.go_to_cafe);
    assert(!*sh_alley_hint(&alley));
    input.down = 0;

    /* MAP_EVENTS[15], source point 18 -> destination 19, late completion 13. */
    x = -57.25f; z = -47.0f; yaw = -1.57079632679489661923f;
    assert(!sh_alley_room_begin(&alley, &save, x, z, yaw, &input));
    input.down = KEY_A;
    assert(!sh_alley_room_begin(&alley, &save, x, z, yaw + 3.14159265358979323846f, &input));
    assert(sh_alley_room_begin(&alley, &save, x, z, yaw, &input));
    assert(alley.stage == SH_ALLEY_ROOM && alley.room_complete_flag == 13);
    assert(!sh_save_event_get(&save, 13));
    assert(alley.room_x > -96.41f && alley.room_x < -96.39f);
    assert(alley.room_z == -247.0f);
    input.down = 0;
    for (int i = 0; i < (int)(2.0f / dt); i++) {
        sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
    }
    assert(alley.room_ready && sh_save_event_get(&save, 13));

    alley.stage = SH_ALLEY_ROAM;
    alley.controls_locked = false;
    sh_alley_progress_update(&alley, &save, -91.0f, -243.0f, dt);
    assert(alley.darkness == 0);
    sh_alley_progress_update(&alley, &save, -109.0f, -227.0f, dt);
    assert(alley.darkness == SH_Q12(50.0f));
    assert(!sh_save_event_get(&save, 15));
    sh_save_event_set(&save, 14);
    sh_alley_progress_update(&alley, &save, -190.0f, -224.0f, dt);
    assert(alley.darkness == SH_Q12(60.0f) && sh_save_event_get(&save, 18));
    sh_alley_progress_update(&alley, &save, -270.0f, -255.0f, dt);
    assert(sh_save_event_get(&save, 15) && !sh_save_event_get(&save, 16));

    alley.stage = SH_ALLEY_DARKER;
    alley.step = 0;
    for (int i = 0; i < (int)(6.0f / dt); i++) {
        sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
        assert(alley.step == 0);
        assert(strstr(sh_alley_subtitle(&alley), "getting darker"));
    }
    assert(sh_save_event_get(&save, 16));
    input.down = KEY_A;
    sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
    assert(alley.step == 1);
    input.down = 0;
    for (int i = 0; i < (int)(6.0f / dt); i++) {
        sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
        assert(alley.step == 1);
        assert(strstr(sh_alley_subtitle(&alley), "better than nothing"));
    }
    input.down = KEY_A;
    sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
    assert(alley.step == 2 && sh_save_event_get(&save, 17));
    input.down = 0;
    sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
    assert(sh_save_event_get(&save, 17));

    assert(alley.stage == SH_ALLEY_ROAM);
    assert(alley.grey_count == 3);
    for (int slot = 0; slot < alley.grey_count; slot++) {
        assert(sh_alley_actor_keyframe(&alley, slot, 1000) == 477);
    }
    assert(alley.grey[0].pos[0] == -252.0f && alley.grey[0].pos[2] == -223.0f);
    x = -252.0f; z = -222.0f;
    sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
    assert(alley.stage == SH_ALLEY_NIGHTMARE);
    for (int i = 0; i < (int)(10.0f / dt); i++) {
        sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
        if (alley.stage == SH_ALLEY_FIGHT) { break; }
    }
    assert(alley.stage == SH_ALLEY_FIGHT && sh_save_event_get(&save, 24));
    assert(!sh_alley_nightmare_trigger(&save, -62.0f, -118.0f));
    assert(!sh_alley_nightmare_trigger(&save, -252.0f, -222.0f));
    sh_alley_reset(&alley);
    alley.stage = SH_ALLEY_SPOTTED;
    alley.cheryl_visible = true;
    alley.cheryl_pos[0] = -29.5f;
    alley.cheryl_pos[2] = -128.7f;
    input.down = 0;
    sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, dt);
    ShCamera camera;
    assert(sh_alley_camera(&alley, &camera));
    const float dx = alley.cheryl_pos[0] - camera.pos[0];
    const float dz = alley.cheryl_pos[2] - camera.pos[2];
    const float distance = sqrtf(dx * dx + dz * dz);
    const float alignment = (-sinf(camera.yaw) * dx - cosf(camera.yaw) * dz) / distance;
    assert(alignment > 0.9999f && fabsf(camera.fov_deg - 53.13f) < 0.001f);
}

static void gate_collision(void)
{
    ShCollWorld world;
    void *assets[9] = {0};
    int count = 0;
    sh_coll_world_reset(&world);
    for (int z = 2; z <= 4; z++) {
        for (int x = -3; x <= -1; x++) {
            char path[128];
            snprintf(path, sizeof(path), "dump/assets/BG/THR%02X%02X.IPD", x & 255, z);
            FILE *file = fopen(path, "rb");
            if (!file) { continue; }
            assert(fseek(file, 0, SEEK_END) == 0);
            long size = ftell(file);
            assert(size > 0 && fseek(file, 0, SEEK_SET) == 0);
            void *data = malloc((size_t)size);
            assert(data && fread(data, 1, (size_t)size, file) == (size_t)size);
            fclose(file);
            assert(sh_coll_world_add(&world, data, (size_t)size));
            assets[count++] = data;
        }
    }
    assert(count > 0);
    float x = -48.0f, y = 1.6f, z = -126.0f;
    for (int i = 0; i < 120 && !sh_alley_gate_trigger(x, z); i++) {
        sh_coll_move(&world, &x, &y, &z, -0.1f, 0.0f, SH_COLL_PLAYER_RADIUS, 1.6f);
    }
    printf("Gate approach: %.3f %.3f\n", x, z);
    assert(sh_alley_gate_trigger(x, z));
    for (int i = 0; i < count; i++) { free(assets[i]); }
}

static void gameplay_camera(float dt)
{
    ShAlley alley = {0};
    sh_alley_reset(&alley);
    alley.stage = SH_ALLEY_STREET;
    float position[3] = {-6.19995117f, 0.0f, -160.5f};
    ShCamera camera;
    assert(sh_alley_gameplay_camera(&alley, position, 3.14159265f, dt, &camera));
    assert(alley.vc.cur >= 0 && isfinite(camera.yaw) && isfinite(camera.pitch));
    assert(fabsf(camera.fov_deg - 53.13f) < 0.001f);
    alley.cam_active = true;
    assert(!sh_alley_gameplay_camera(&alley, position, 0.0f, dt, &camera));
    alley.cam_active = false;
    position[0] = -252.0f; position[2] = -222.0f;
    assert(sh_alley_gameplay_camera(&alley, position, 0.0f, dt, &camera));
    ShVc fresh;
    ShCamera expected;
    sh_vc_init(&fresh, alley.vc.roads, alley.vc.road_count);
    const float ps1[3] = {position[0], -position[1], -position[2]};
    sh_vc_update(&fresh, ps1, 0.0f, false, dt, &expected);
    for (int axis = 0; axis < 3; axis++) {
        assert(fabsf(camera.pos[axis] - expected.pos[axis]) < 0.0001f);
    }
    assert(alley.vc.cur >= 0);
}

int main(void)
{
    assert(!sh_alley_gate_trigger(-49.0f, -126.0f));
    assert(!sh_alley_gate_trigger(-55.0f + 1.0f / 4096.0f, -126.0f));
    assert(sh_alley_gate_trigger(-55.0f, -132.0f));
    assert(sh_alley_gate_trigger(-57.0f, -120.0f));
    assert(sh_alley_gate_trigger(-58.0f, -112.0f));
    assert(!sh_alley_gate_trigger(-58.0f, -112.0f + 1.0f / 4096.0f));
    gate_collision();
    gameplay_camera(1.0f / 30.0f);
    gameplay_camera(1.0f / 60.0f);
    run(1.0f / 30.0f);
    run(1.0f / 60.0f);
    puts("PASS: alley entry, retail door/flags/darkness paths, persistent messages and Y at 30/60 fps");
    return 0;
}
