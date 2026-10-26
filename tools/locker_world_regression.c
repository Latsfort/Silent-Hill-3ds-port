#include "sh3ds/locker_world.h"
#include "sh3ds/rng.h"
#include "sh3ds/fixed.h"
#include "sh3ds/school.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/game/school_roof_props.h"

int main(void)
{
    {
        ShMapVertex source = {0}, shifted, inplace;
        source.pos[0] = 54.0f; source.pos[1] = 0.25f; source.pos[2] = 60.0f;
        source.uv[0] = 0.5f;
        sh_world_vertical_vertices(257, &source, &shifted, 1);
        inplace = source;
        sh_world_vertical_vertices(257, &inplace, &inplace, 1);
        assert(!memcmp(&shifted, &inplace, sizeof(shifted)));
        assert(shifted.pos[0] == source.pos[0] && shifted.pos[2] == source.pos[2]);
        assert(shifted.pos[1] == source.pos[1] - 257.0f / 256 && shifted.uv[0] == source.uv[0]);
    }
    for (int angle = 0; angle < 4096; angle++) {
        for (int door = 0; door < 3; door++) {
            const float *pivot = s_school_roof_props[door + 2].pos;
            ShMapVertex source[4], transformed[4], inplace[4];
            memset(source, 0x5A, sizeof(source));
            for (int vertex = 0; vertex < 4; vertex++) {
                for (int axis = 0; axis < 3; axis++) {
                    const int local = vertex * 233 + axis * 307 - 467;
                    source[vertex].pos[axis] = (axis ? -1.0f : 1.0f) * ((int)(pivot[axis] * 256) + local) / 256.0f;
                }
            }
            memcpy(inplace, source, sizeof(source));
            sh_locker_transform_vertices(angle, pivot, source, transformed, 4);
            sh_locker_transform_vertices(angle, pivot, inplace, inplace, 4);
            assert(!memcmp(inplace, transformed, sizeof(inplace)));
            for (int vertex = 0; vertex < 4; vertex++) {
                const int x = vertex * 233 - 467, y = vertex * 233 + 307 - 467, z = vertex * 233 + 614 - 467;
                const int64_t rx = (int64_t)sh_fixed_cos(angle) * x + (int64_t)sh_fixed_sin(angle) * z;
                const int64_t rz = -(int64_t)sh_fixed_sin(angle) * x + (int64_t)sh_fixed_cos(angle) * z;
                const int ox = rx >= 0 ? rx / 4096 : -((-rx + 4095) / 4096);
                const int oz = rz >= 0 ? rz / 4096 : -((-rz + 4095) / 4096);
                assert(transformed[vertex].pos[0] == ((SH_Q12(pivot[0]) >> 4) + ox) / 256.0f);
                assert(transformed[vertex].pos[1] == -((SH_Q12(pivot[1]) >> 4) + y) / 256.0f);
                assert(transformed[vertex].pos[2] == -((SH_Q12(pivot[2]) >> 4) + oz) / 256.0f);
                assert(!memcmp(transformed[vertex].uv, source[vertex].uv, sizeof(source[vertex].uv)));
                assert(!memcmp(transformed[vertex].color, source[vertex].color, sizeof(source[vertex].color)));
            }
        }
    }
    ShLockerWorld world = {0};
    ShSavegame save = {0};
    ShLockerFrame frame;
    sh_save_event_set(&save, 101);
    sh_save_event_set(&save, 102);
    unsigned shakes = 0, impacts = 0, opening = 0;
    int frames = 0;
    for (; frames < 1000 && world.phase != 6; frames++) {
        sh_locker_world_step(&world, &save, 136, false, &frame);
        assert(world.rotation[0] == world.rotation[1]);
        assert(!sh_save_event_get(&save, 106) && !sh_save_event_get(&save, 107));
        if (sh_save_event_get(&save, 103)) { assert(world.phase == 5 || world.phase == 6); }
        for (unsigned i = 0; i < frame.command_count; i++) {
            const ShLockerCommand *command = &frame.commands[i];
            if (command->effect) { assert(command->id == 18 && command->volume == 0x60); shakes++; }
            else if (command->id == 1434) { impacts++; }
            else if (command->id == 1436) { opening++; }
            else { assert(false); }
        }
    }
    assert(frames < 1000 && sh_save_event_get(&save, 103));
    assert(shakes == 1 && impacts == 2 && opening == 1);
    sh_save_event_set(&save, 105);
    unsigned body_shakes = 0, body_falls = 0, body_impacts = 0;
    int body_frames = 0;
    for (; body_frames < 200 && !sh_save_event_get(&save, 106); body_frames++) {
        sh_locker_world_step(&world, &save, 136, false, &frame);
        assert(frame.draw_mask == 31);
        for (unsigned i = 0; i < frame.command_count; i++) {
            const ShLockerCommand *command = &frame.commands[i];
            if (command->effect) { assert(command->id == 18 && command->volume == 128); body_shakes++; }
            else {
                assert(command->volume == 255 && command->flags);
                if (command->id == 1460) { body_falls++; }
                else { assert(command->id == 1434); body_impacts++; }
            }
        }
    }
    assert(body_frames < 200 && !world.speed && !sh_save_event_get(&save, 107));
    assert(body_shakes == 1 && body_falls == 1 && body_impacts == 1);
    const ShLockerWorld before = world;
    const uint32_t seed = sh_rng_get_seed();
    sh_locker_world_step(&world, &save, 0, true, &frame);
    assert(!memcmp(&before, &world, sizeof(world)) && sh_rng_get_seed() == seed);
    assert(frame.draw_mask == 31 && !frame.command_count);
    ShLockerWorld idle = {0};
    ShSavegame idle_save = {0};
    sh_rng_set_seed(0x12345678);
    unsigned rattles = 0;
    for (int tick = 0; tick < 10000; tick++) {
        ShLockerWorld repeated = idle;
        ShSavegame repeated_save = idle_save;
        const uint32_t start = sh_rng_get_seed();
        sh_locker_world_step(&idle, &idle_save, 136, false, &frame);
        const uint32_t finish = sh_rng_get_seed();
        ShLockerFrame repeated_frame;
        sh_rng_set_seed(start);
        sh_locker_world_step(&repeated, &repeated_save, 136, false, &repeated_frame);
        assert(sh_rng_get_seed() == finish && !memcmp(&idle, &repeated, sizeof(idle)));
        assert(!memcmp(&frame, &repeated_frame, sizeof(frame)));
        assert(idle.rotation[0] == idle.rotation[1] && idle.rotation[1] <= 0 && idle.rotation[1] > -128);
        assert(!sh_save_event_get(&idle_save, 103) && !sh_save_event_get(&idle_save, 106));
        assert(frame.draw_mask == 7);
        for (unsigned i = 0; i < frame.command_count; i++) { assert(!frame.commands[i].effect && frame.commands[i].id == 1435); rattles++; }
    }
    assert(rattles);
    printf("PASS: locker opens in %d original frames, body settles in %d; 10000 deterministic rattle frames and paused invariants\n", frames, body_frames);
    return 0;
}
