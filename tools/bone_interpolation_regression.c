#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/game/chara.c"

static uint8_t *load(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    *size = (size_t)ftell(file);
    rewind(file);
    uint8_t *data = malloc(*size);
    assert(data && fread(data, 1, *size, file) == *size);
    fclose(file);
    return data;
}

/* Independent signed floor division for the retail arithmetic shifts. */
static int floor_div(int value, unsigned divisor)
{
    return value >= 0 ? value / (int)divisor : -(int)(((unsigned)-value + divisor - 1) / divisor);
}

int main(void)
{
    size_t size, model_size, tim_size;
    uint8_t *anm = load("romfs/game/anim/FAT.ANM", &size);
    uint8_t *model = load("romfs/game/chara/FAT.ILM", &model_size);
    uint8_t *tim_data = load("romfs/game/chara/FAT.TIM", &tim_size);
    ShTim tim;
    assert(sh_tim_parse(tim_data, tim_size, &tim));
    unsigned frame_size = anm[4] | ((unsigned)anm[5] << 8), translations = anm[3], scale = anm[18];
    assert(rd_u32(anm + 8) == 0xffffffffu && scale <= 12);
    const int alphas[] = {0, 1, 2048, 4095, 4096};
    unsigned cases = 0;
    for (int first = 0; first < 202; ++first) {
        for (int distance = 1; distance <= 17; distance += 16) {
            int second = (first + distance) % 202;
            const uint8_t *a = anm_keyframe_ptr(anm, size, frame_size, first);
            const uint8_t *b = anm_keyframe_ptr(anm, size, frame_size, second);
            for (unsigned alpha_index = 0; alpha_index < sizeof(alphas) / sizeof(alphas[0]); ++alpha_index) {
                ShAnimPoseTiming pose = {first, second, alphas[alpha_index]};
                int bone_count;
                assert(build_skeleton(&bone_count, anm, size, first, NULL, 0, 0, false, 0, &pose));
                for (int bone = 1; bone < bone_count; ++bone) {
                    const uint8_t *bind = anm + 20 + bone * 6;
                    int translation = (int8_t)bind[2], rotation = (int8_t)bind[1];
                    for (int axis = 0; axis < 3; ++axis) {
                        int value = (int8_t)bind[3 + axis] * (1 << scale);
                        if (translation >= 0) {
                            int av = (int8_t)a[translation * 3 + axis], bv = (int8_t)b[translation * 3 + axis];
                            value = av * (1 << scale) + floor_div((bv - av) * pose.alpha, 1u << (12 - scale));
                            if (translation == 0 && axis == 1) { value -= anm[19]; }
                        }
                        assert(s_local[bone].t[axis] == value);
                    }
                    for (int row = 0; row < 3; ++row) {
                        for (int col = 0; col < 3; ++col) {
                            int value = row == col ? 4096 : 0;
                            if (rotation >= 0) {
                                unsigned offset = translations * 3 + rotation * 9 + row * 3 + col;
                                int av = (int8_t)a[offset], bv = (int8_t)b[offset];
                                value = av * 32 + floor_div((bv - av) * pose.alpha, 128);
                            }
                            assert(s_local[bone].m[row][col] == value);
                        }
                    }
                }
                cases++;
            }
        }
    }
    ShMapVertex *single = calloc(4096, sizeof(*single)), *mixed = calloc(4096, sizeof(*mixed));
    assert(single && mixed);
    for (int variant = 1; variant <= 2; ++variant) {
        size_t count_single, count_mixed;
        ShAnimPoseTiming pose = {100, 101, 0};
        assert(sh_chara_bake_split_head(single, 4096, &count_single, model, model_size, anm, size, &tim, 100, NULL, variant, 192));
        assert(sh_chara_bake_split_head_blend(mixed, 4096, &count_mixed, model, model_size, anm, size, &tim, &pose, NULL, variant, 192));
        assert(count_single == count_mixed && !memcmp(single, mixed, count_single * sizeof(*single)));
        pose.alpha = 4096;
        assert(sh_chara_bake_split_head(single, 4096, &count_single, model, model_size, anm, size, &tim, 101, NULL, variant, 192));
        assert(sh_chara_bake_split_head_blend(mixed, 4096, &count_mixed, model, model_size, anm, size, &tim, &pose, NULL, variant, 192));
        assert(count_single == count_mixed && !memcmp(single, mixed, count_single * sizeof(*single)));
    }
    printf("PASS: %u real FAT two-frame/alpha fixtures across every bone, signed floor math and exact mesh endpoints/both variants\n", cases);
    free(single); free(mixed); free(anm); free(model); free(tim_data);
    return 0;
}
