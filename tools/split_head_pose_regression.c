#include "sh3ds/chara.h"
#include "sh3ds/fixed.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *load(const char *name, size_t *length)
{
    FILE *file = fopen(name, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    *length = (size_t)ftell(file);
    rewind(file);
    uint8_t *data = malloc(*length);
    assert(data && fread(data, 1, *length, file) == *length);
    fclose(file);
    return data;
}

int main(void)
{
    size_t model_size, animation_size, tim_size;
    uint8_t *model = load("romfs/game/chara/FAT.ILM", &model_size);
    uint8_t *animation = load("romfs/game/anim/FAT.ANM", &animation_size);
    uint8_t *image = load("romfs/game/chara/FAT.TIM", &tim_size);
    ShTim tim;
    assert(sh_tim_parse(image, tim_size, &tim));
    const int frames = sh_chara_anm_keyframe_count(animation, animation_size);
    assert(frames >= 202);
    ShCharaClutMap cluts;
    sh_chara_clut_map_init(&cluts, true);
    assert(sh_chara_collect_cluts(model, model_size, &cluts, 0));
    const size_t capacity = 32768;
    ShMapVertex *vertices = calloc(capacity, sizeof(*vertices));
    assert(vertices);
    size_t first_count = 0, second_count = 0;
    for (int frame = 0; frame < frames; frame++) {
        size_t all;
        assert(sh_chara_bake(vertices, capacity, &all, model, model_size, animation, animation_size,
            &tim, frame, &cluts, SH_CHARA_F_NONE, NULL, 0, 0, NULL, NULL, 0, -1));
        for (int variant = 1; variant <= 2; variant++) {
            const unsigned flags = variant == 1 ? SH_CHARA_F_SPLIT_HEAD_1 : SH_CHARA_F_SPLIT_HEAD_2;
            size_t count;
            assert(sh_chara_bake(vertices, capacity, &count, model, model_size, animation, animation_size,
                &tim, frame, &cluts, flags, NULL, 0, 0, NULL, NULL, 0, -1));
            assert(count && count < all && count + 3 < capacity);
            ShCharaBone plain;
            assert(sh_chara_last_bone(1, &plain));
            for (size_t vertex = 0; vertex < count; vertex++) {
                for (int axis = 0; axis < 3; axis++) { assert(isfinite(vertices[vertex].pos[axis])); }
                assert(vertices[vertex].uv[0] >= 0 && vertices[vertex].uv[0] <= 1);
                assert(vertices[vertex].uv[1] >= 0 && vertices[vertex].uv[1] <= 1);
            }
            if (!frame) { if (variant == 1) { first_count = count; } else { second_count = count; } }
            else { assert(count == (variant == 1 ? first_count : second_count)); }
            size_t posed;
            assert(sh_chara_bake_split_head(vertices, capacity, &posed, model, model_size, animation, animation_size,
                &tim, frame, &cluts, variant, 0) && posed == count);
            ShCharaBone neutral;
            assert(sh_chara_last_bone(1, &neutral));
            for (int row = 0; row < 3; row++) {
                assert(neutral.translation[row] == plain.translation[row]);
                for (int column = 0; column < 3; column++) { assert(neutral.rotation[row][column] == plain.rotation[row][column]); }
            }
            for (int angle = -192; angle <= 192; angle += 384) {
                assert(sh_chara_bake_split_head(vertices, capacity, &posed, model, model_size, animation, animation_size,
                    &tim, frame, &cluts, variant, angle));
                ShCharaBone turned;
                assert(sh_chara_last_bone(1, &turned));
                const int32_t sine = sh_fixed_sin(angle), cosine = sh_fixed_cos(angle);
                for (int row = 0; row < 3; row++) {
                    assert(turned.translation[row] == plain.translation[row]);
                    assert(turned.rotation[row][0] == (int32_t)(((int64_t)plain.rotation[row][0] * cosine - (int64_t)plain.rotation[row][2] * sine) >> 12));
                    assert(turned.rotation[row][2] == (int32_t)(((int64_t)plain.rotation[row][0] * sine + (int64_t)plain.rotation[row][2] * cosine) >> 12));
                }
                for (size_t vertex = 0; vertex < posed; vertex++) {
                    for (int axis = 0; axis < 3; axis++) { assert(isfinite(vertices[vertex].pos[axis])); }
                }
            }
        }
    }
    printf("PASS: all%d real FAT frames, both source mesh variants (%zu/%zu vertices), procedural twists and NPC atlas\n", frames, first_count, second_count);
    free(vertices); free(model); free(animation); free(image);
    return 0;
}
