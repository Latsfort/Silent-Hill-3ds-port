#include "sh3ds/chara.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_asset(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    *size = (size_t)ftell(file);
    rewind(file);
    uint8_t *data = malloc(*size);
    assert(data && fread(data, 1, *size, file) == *size);
    fclose(file);
    return data;
}

int main(void)
{
    size_t model_size, base_size, tim_size;
    uint8_t *model = read_asset("romfs/game/chara/HERO.ILM", &model_size);
    uint8_t *base = read_asset("romfs/game/anim/HB_BASE.ANM", &base_size);
    uint8_t *tim_data = read_asset("romfs/game/chara/HERO.TIM", &tim_size);
    ShTim tim;
    assert(sh_tim_parse(tim_data, tim_size, &tim));
    ShMapVertex *vertices = calloc(SH_CHARA_MAX_VERTS, sizeof(*vertices));
    assert(vertices);
    const int frames = sh_chara_anm_keyframe_count(base, base_size);
    unsigned moved = 0;
    for (int frame = 0; frame < frames; frame++) {
        ShCharaBone left, shoulder, hand, actual;
        size_t count;
        assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size, base, base_size,
            &tim, frame, NULL, SH_CHARA_F_HARRY_HANDS, NULL, 0, 0, NULL, NULL, 0, -1));
        assert(sh_chara_last_bone(6, &left) && sh_chara_last_bone(8, &shoulder) && sh_chara_last_bone(10, &hand));
        assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size, base, base_size,
            &tim, frame, NULL, SH_CHARA_F_HARRY_HANDS | SH_CHARA_F_HARRY_MATCH_HAND,
            NULL, 0, 0, NULL, NULL, 0, -1));
        assert(sh_chara_last_bone(10, &actual) && memcmp(&actual, &hand, sizeof(hand)) == 0);
        assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size, base, base_size,
            &tim, frame, NULL, SH_CHARA_F_HARRY_HANDS | SH_CHARA_F_HARRY_MATCH_HAND | SH_CHARA_F_HARRY_MATCH_ARM,
            NULL, 0, 0, NULL, NULL, 0, -1));
        assert(count > 0 && count % 3 == 0);
        for (size_t vertex = 0; vertex < count; vertex++) {
            for (int axis = 0; axis < 3; axis++) { assert(isfinite(vertices[vertex].pos[axis])); }
        }
        assert(sh_chara_last_bone(6, &actual) && memcmp(&actual, &left, sizeof(left)) == 0);
        assert(sh_chara_last_bone(8, &actual));
        assert(memcmp(actual.translation, shoulder.translation, sizeof(actual.translation)) == 0);
        assert(sh_chara_last_bone(10, &actual));
        moved += memcmp(actual.translation, hand.translation, sizeof(actual.translation)) != 0;
        assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size, base, base_size,
            &tim, frame, NULL, SH_CHARA_F_HARRY_HANDS, NULL, 0, 0, NULL, NULL, 0, -1));
        assert(sh_chara_last_bone(10, &actual) && memcmp(&actual, &hand, sizeof(hand)) == 0);
    }
    assert(frames > 0 && moved == (unsigned)frames);
    printf("PASS: match pose over %d original Harry frames, fixed shoulder, unchanged left arm and clean reset\n", frames);
    free(vertices);
    free(tim_data);
    free(base);
    free(model);
    return 0;
}
