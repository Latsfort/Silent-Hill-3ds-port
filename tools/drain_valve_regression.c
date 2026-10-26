#include "sh3ds/drain.h"
#include "sh3ds/fixed.h"
#include "sh3ds/school.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/game/school_roof_props.h"

static unsigned reference(ShDrainValve *valve, int step, int hand, int frame)
{
    if (step != 7 && step != 12) { return 0; }
    int delta = (valve->previous_angle - hand) % 4096;
    if (delta < 0) { delta += 4096; }
    if (delta > 2048) { delta -= 4096; }
    unsigned sounds = 0;
    if (frame >= 6 && frame <= 36) {
        const int turning = step == 7 ? delta < 0 : delta > 0;
        if (turning) {
            const unsigned phase = valve->sound_phase;
            if (!phase) { sounds = SH_DRAIN_VALVE_SQUEAK; valve->sound_phase = 1; }
            else if (!(phase % 2) && phase < 8) {
                sounds = SH_DRAIN_VALVE_CLICK;
                valve->sound_phase++;
                if (phase == 2) {
                    valve->running = step == 7;
                    sounds |= step == 7 ? SH_DRAIN_WATER_START : SH_DRAIN_WATER_STOP;
                }
            }
            valve->rotation = (int16_t)(valve->rotation + delta);
            if (valve->running) {
                int opening = valve->opening - delta;
                valve->opening = opening < 0 ? 0 : opening > 4080 ? 4080 : opening;
            }
        } else if (valve->sound_phase % 2) { valve->sound_phase++; }
    }
    valve->previous_angle = hand;
    return sounds;
}

int main(void)
{
    unsigned volume_checks = 0;
    for (int distance = 0; distance <= 18 * 4096; distance++) {
        for (int opening = 0; opening <= 4080; opening += 17) {
            int volume = opening;
            if (distance > 17 * 4096) { volume = 0; }
            else if (distance >= 4096) {
                const int ratio = (int)((int64_t)(distance - 4096) * 4096 / (16 * 4096));
                volume = (int)((int64_t)opening * (4096 - ratio) / 4096);
            }
            assert(sh_drain_water_attenuation(distance, opening) == (uint8_t)~(volume / 16));
            volume_checks++;
        }
    }
    int16_t point[3];
    sh_drain_valve_hand_local(point);
    assert(sh_drain_valve_hand_bone() == 10);
    for (int angle = 0; angle < 4096; angle++) {
        const float *pivot = s_school_roof_props[0].pos;
        ShMapVertex source[4], transformed_mesh[4], inplace[4];
        memset(source, 0x5A, sizeof(source));
        for (int vertex = 0; vertex < 4; vertex++) {
            for (int axis = 0; axis < 3; axis++) {
                const int local = vertex * 17 + axis * 23 - 37;
                source[vertex].pos[axis] = (axis ? -1.0f : 1.0f) * ((int)(pivot[axis] * 256) + local) / 256.0f;
            }
        }
        memcpy(inplace, source, sizeof(source));
        sh_drain_valve_transform_vertices(angle, pivot, source, transformed_mesh, 4);
        sh_drain_valve_transform_vertices(angle, pivot, inplace, inplace, 4);
        assert(!memcmp(inplace, transformed_mesh, sizeof(inplace)));
        for (int vertex = 0; vertex < 4; vertex++) {
            const int x = vertex * 17 - 37, y = vertex * 17 + 23 - 37, z = vertex * 17 + 46 - 37;
            const int draw_angle = (angle / 4) * 4;
            const int64_t sy = (int64_t)sh_fixed_cos(draw_angle) * y - (int64_t)sh_fixed_sin(draw_angle) * z;
            const int64_t sz = (int64_t)sh_fixed_sin(draw_angle) * y + (int64_t)sh_fixed_cos(draw_angle) * z;
            const int ry = sy >= 0 ? sy / 4096 : -((-sy + 4095) / 4096);
            const int rz = sz >= 0 ? sz / 4096 : -((-sz + 4095) / 4096);
            assert(transformed_mesh[vertex].pos[0] == ((SH_Q12(pivot[0]) >> 4) + x) / 256.0f);
            assert(transformed_mesh[vertex].pos[1] == -((SH_Q12(pivot[1]) >> 4) + ry) / 256.0f);
            assert(transformed_mesh[vertex].pos[2] == -((SH_Q12(pivot[2]) >> 4) + rz) / 256.0f);
            assert(!memcmp(transformed_mesh[vertex].uv, source[vertex].uv, sizeof(source[vertex].uv)));
            assert(!memcmp(transformed_mesh[vertex].color, source[vertex].color, sizeof(source[vertex].color)));
        }
        ShCharaBone bone = {0};
        bone.rotation[0][0] = bone.rotation[2][2] = sh_fixed_cos(angle);
        bone.rotation[0][2] = sh_fixed_sin(angle);
        bone.rotation[2][0] = -bone.rotation[0][2];
        bone.rotation[1][1] = 4096;
        bone.translation[0] = angle - 2048;
        bone.translation[1] = angle - 1024;
        bone.translation[2] = -angle;
        const int32_t position[3] = {angle * 17 - 50000, -angle * 3, angle * 23 - 30000};
        const ShPathPoint player_position = {position[0], position[1], position[2]};
        const ShPathPoint camera_position = {angle * 7, -angle * 5, angle * 11};
        const int dx = ((((player_position.vx + camera_position.vx) >> 1) + SH_Q12(10.2f)) >> 6);
        const int dy = ((player_position.vy + camera_position.vy) >> 1) >> 6;
        const int dz = ((((player_position.vz + camera_position.vz) >> 1) + SH_Q12(13.35f)) >> 6);
        assert(sh_drain_water_distance(player_position, camera_position) == (int32_t)sh_fixed_sqrt0(dx * dx + dy * dy + dz * dz) * 64);
        ShCharaBone world, inplace_world = bone;
        sh_drain_valve_world_bone(&bone, position, angle, &world);
        sh_drain_valve_world_bone(&inplace_world, position, angle, &inplace_world);
        assert(!memcmp(&world, &inplace_world, sizeof(world)));
        int32_t root[3][3] = {{0}};
        root[0][0] = root[2][2] = sh_fixed_cos(angle);
        root[0][2] = sh_fixed_sin(angle);
        root[2][0] = -root[0][2];
        root[1][1] = 4096;
        for (int axis = 0; axis < 3; axis++) {
            int64_t translation = 0;
            for (int column = 0; column < 3; column++) {
                int64_t component = 0;
                for (int k = 0; k < 3; k++) { component += (int64_t)root[axis][k] * bone.rotation[k][column]; }
                assert(world.rotation[axis][column] == (component >= 0 ? component / 4096 : -((-component + 4095) / 4096)));
                translation += (int64_t)root[axis][column] * bone.translation[column];
            }
            const int32_t shifted = position[axis] >= 0 ? position[axis] / 16 : -((-position[axis] + 15) / 16);
            assert(world.translation[axis] == shifted + (translation >= 0 ? translation / 4096 : -((-translation + 4095) / 4096)));
        }
        int64_t transformed[3];
        for (int axis = 0; axis < 3; axis++) {
            int64_t sum = 0;
            for (int column = 0; column < 3; column++) { sum += (int64_t)bone.rotation[axis][column] * point[column]; }
            /* Floor division, including negative values, independently of a signed shift. */
            transformed[axis] = (sum >= 0 ? sum / 4096 : -((-sum + 4095) / 4096)) + bone.translation[axis];
        }
        assert(sh_drain_valve_hand_angle(&bone) == sh_fixed_atan2(transformed[1] * 16 + SH_Q12(0.7f),
            transformed[2] * 16 + SH_Q12(12.92f)));
    }
    unsigned checks = 0;
    for (int step = 7; step <= 12; step++) {
        for (int delta = 0; delta < 4096; delta++) {
            for (int phase = 0; phase <= 9; phase++) {
                for (int frame = 5; frame <= 37; frame += frame == 6 ? 30 : 1) {
                    for (int running = 0; running <= 1; running++) {
                        ShDrainValve actual = {4090, 32000, delta, running, phase};
                        ShDrainValve expected = actual;
                        const unsigned result = sh_drain_valve_hand(&actual, step, 4090 - delta, frame);
                        assert(result == reference(&expected, step, 4090 - delta, frame));
                        assert(actual.previous_angle == expected.previous_angle && actual.rotation == expected.rotation);
                        assert(actual.opening == expected.opening && actual.running == expected.running);
                        assert(actual.sound_phase == expected.sound_phase);
                        checks++;
                    }
                }
            }
        }
    }
    printf("PASS: %u valve signed-angle, frame-window, sound-phase and clamp cases\n", checks);
    printf("PASS: %u water attenuation cases and 4096 hand matrix transforms\n", volume_checks);
    return 0;
}
