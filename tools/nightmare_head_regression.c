#include "sh3ds/alley.h"
#include "sh3ds/controls.h"
#include "sh3ds/drain.h"
#include "sh3ds/fixed.h"
#include "sh3ds/snd.h"
#include <3ds.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned voice_count, loading;
bool sh_input_down(const ShInputState *input, uint32_t key) { return (input->down & key) != 0; }
void sh_snd_stop(uint16_t id) { assert(id == 1358); }
void sh_snd_play(uint16_t id, int balance, int attenuation)
{ assert(id == 1359 && balance == 0 && attenuation == 208); }
void sh_bgm_status_set(unsigned flags) { assert(flags == SH_BGM_STATUS_VOICE_DIALOG); }
void sh_snd_call(uint32_t command)
{
    if (command != 19) {
        assert(command == (voice_count ? 4119 : 4126));
        voice_count++;
        loading = 3;
    }
}
uint8_t sh_sound_streaming(void)
{
    if (loading) { loading--; return 4; }
    return 0;
}

static uint8_t *read_asset(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    const long length = ftell(file);
    assert(length > 0);
    *size = (size_t)length;
    rewind(file);
    uint8_t *bytes = malloc(*size);
    assert(bytes && fread(bytes, 1, *size, file) == *size);
    fclose(file);
    return bytes;
}

int main(void)
{
    size_t ilm_size, anm_size, tim_size;
    uint8_t *ilm = read_asset("romfs/game/chara/HERO.ILM", &ilm_size);
    uint8_t *anm = read_asset("romfs/game/anim/HB_BASE.ANM", &anm_size);
    uint8_t *tim_bytes = read_asset("romfs/game/chara/HERO.TIM", &tim_size);
    ShTim tim;
    assert(sh_tim_parse(tim_bytes, tim_size, &tim));
    ShMapVertex *vertices = calloc(SH_CHARA_MAX_VERTS, sizeof(*vertices));
    assert(vertices);
    for (int fps = 30; fps <= 60; fps += 30) {
        for (int held = 0; held < 2; held++) {
            ShAlley alley = {0};
            ShSavegame save = {0};
            ShInputState input = {0};
            input.held = held ? KEY_START : 0;
            alley.stage = SH_ALLEY_NIGHTMARE;
            alley.torch_player.model.keyframe = 503;
            sh_vc_init(&alley.vc, NULL, 0);
            float position[3] = {-252.0f, 0.0f, -222.0f}, yaw = 0.0f;
            voice_count = loading = 0;
            unsigned head_frames = 0, visible_frames = 0;
            for (int tick = 0; tick < 25 * fps && alley.stage == SH_ALLEY_NIGHTMARE; tick++) {
                const bool head_camera = alley.step == 4 && alley.nightmare.camera_phase == 2;
                ShCharaBone expected;
                if (head_camera) {
                    const ShPathPoint p = alley.nightmare.player.position;
                    const int32_t root[3] = {p.vx, p.vy, p.vz};
                    sh_drain_valve_world_bone(&alley.nightmare.head, root, alley.nightmare.player.rotation.vy, &expected);
                }
                sh_border_update(&alley.border, SH_Q12(1.0f / fps));
                sh_alley_nightmare_update(&alley, &save, position, &yaw, &input, 1.0f / fps);
                if (head_camera) {
                    for (unsigned axis = 0; axis < 3; axis++) {
                        const int32_t target = (expected.translation[axis] +
                            (int32_t)(((int64_t)expected.rotation[axis][1] * -38) >> 12)) * 16;
                        assert(fabsf(alley.vc.cam_tgt[axis] - SH_Q12_TO_FLOAT(target)) < 0.00001f);
                    }
                    assert(alley.nightmare.hidden);
                    assert(isfinite(alley.cam.yaw) && isfinite(alley.cam.pitch));
                    head_frames++;
                }
                if (alley.nightmare.hidden) {
                    assert(alley.nightmare.path.waypoint == 2);
                    assert(alley.cam.fov_deg == SH_VC_FOV_DEG);
                } else { visible_frames++; }
                size_t count;
                assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, ilm, ilm_size, anm, anm_size,
                    &tim, alley.nightmare.player.model.keyframe, NULL,
                    SH_CHARA_F_HARRY_HANDS | SH_CHARA_F_HARRY_MATCH_HAND | SH_CHARA_F_HARRY_MATCH_ARM,
                    NULL, 0, 0, NULL, NULL, 0, -1));
                assert(count > 0);
                alley.nightmare.head_valid = sh_chara_last_bone((unsigned)sh_alley_nightmare_head_bone(), &alley.nightmare.head);
                assert(alley.nightmare.head_valid);
            }
            assert(head_frames > (unsigned)(4 * fps) && visible_frames > 0);
            assert(voice_count == 2 && sh_save_event_get(&save, SH_EV_NIGHTMARE));
            assert(alley.stage == SH_ALLEY_FIGHT && !alley.nightmare.hidden && !alley.cam_active);
            printf("PASS: actual Harry head camera, original voice loading, path and held skip at %d FPS (%d)\n", fps, held);
        }
    }
    free(vertices);
    free(tim_bytes);
    free(anm);
    free(ilm);
    return 0;
}
