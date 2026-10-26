#include "sh3ds/chara.h"
#include "sh3ds/town.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct ShSchoolAnimation { int state, first, last; float fps; } ShSchoolAnimation;
#include "../source/game/school_ladder_anims.h"
#include "../source/game/school_arrival_anim.h"
typedef struct ShPathAnimDef {
    int status, playback;
    bool variable;
    ShAnimClipTiming clip;
    int32_t duration;
} ShPathAnimDef;
#include "../source/game/roof_animation_data.h"

static uint8_t *load(const char *path, size_t *size)
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
    size_t base_size, map_size, model_size, tim_size;
    uint8_t *base = load("romfs/game/anim/HB_BASE.ANM", &base_size);
    uint8_t *map = load("romfs/game/anim/HB_M1S01.ANM", &map_size);
    uint8_t *model = load("romfs/game/chara/HERO.ILM", &model_size);
    uint8_t *tim_file = load("romfs/game/chara/HERO.TIM", &tim_size);
    ShTim tim;
    assert(sh_tim_parse(tim_file, tim_size, &tim));
    ShMapVertex *vertices = calloc(SH_CHARA_MAX_VERTS, sizeof(*vertices));
    assert(vertices);
    ShTown town = {0};
    town.stage = SH_TOWN_SCRIPT;
    town.script = SH_TOWN_SCR_SCHOOL;
    town.school.overlay = SH_MAP_IDX_MAP1_S01;
    unsigned poses = 0;
    for (int state = 0; state < 256; state++) {
        town.harry_state = state;
        town.harry_time = 0.0f;
        int first, last;
        if (!sh_school_harry_keyframe(&town, &first)) {
            assert(state != 59 && state != 60 && state != 83);
            continue;
        }
        town.harry_time = 100.0f;
        assert(sh_school_harry_keyframe(&town, &last));
        assert(first >= SH_CHARA_MAP_KF_BASE && last >= first);
        for (int frame = first; frame <= last; frame++) {
            ShCharaClutMap cluts = {0};
            size_t count;
            assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size,
                                  base, base_size, &tim, frame, &cluts, SH_CHARA_F_HARRY_HANDS,
                                  map, map_size, SH_CHARA_MAP_KF_BASE, NULL, NULL, 0, -1));
            assert(count > 0 && count % 3 == 0);
            for (size_t i = 0; i < count; i++) {
                assert(isfinite(vertices[i].pos[0]) && isfinite(vertices[i].pos[1]) && isfinite(vertices[i].pos[2]));
            }
            poses++;
        }
    }
    assert(poses == 76);
    free(map);
    map = load("romfs/game/anim/HB_M1S00.ANM", &map_size);
    town.school.overlay = SH_MAP_IDX_MAP1_S00;
    unsigned ladder_poses = 0;
    for (unsigned i = 0; i < sizeof(s_school_ladder_animations) / sizeof(s_school_ladder_animations[0]); i++) {
        const ShSchoolAnimation *clip = &s_school_ladder_animations[i];
        town.harry_state = clip->state;
        for (int frame = clip->first; frame <= clip->last; frame++) {
            ShCharaClutMap cluts = {0};
            size_t count;
            assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size,
                                  base, base_size, &tim, frame, &cluts, SH_CHARA_F_HARRY_HANDS,
                                  map, map_size, SH_CHARA_MAP_KF_BASE, NULL, NULL, 0, -1));
            assert(count && count % 3 == 0);
            for (size_t vertex = 0; vertex < count; vertex++) {
                for (int axis = 0; axis < 3; axis++) { assert(isfinite(vertices[vertex].pos[axis])); }
            }
            ladder_poses++;
        }
        for (int cycle = 0; cycle < 4; cycle++) {
            town.harry_time = (cycle * (clip->last - clip->first + 1) + 0.25f) / clip->fps;
            int frame;
            assert(sh_school_harry_keyframe(&town, &frame) && frame == clip->first);
        }
    }
    assert(ladder_poses == 74);
    free(map);
    map = load("romfs/game/anim/HB_M1S02.ANM", &map_size);
    size_t phone_size, phone_tim_size;
    uint8_t *phone = load("romfs/game/item/PHONE.PLM", &phone_size);
    uint8_t *phone_file = load("romfs/game/item/PHONE.TIM", &phone_tim_size);
    ShTim phone_tim;
    assert(sh_tim_parse(phone_file, phone_tim_size, &phone_tim));
    ShCharaClutMap phone_cluts;
    sh_chara_clut_map_init(&phone_cluts, false);
    assert(sh_chara_collect_cluts(model, model_size, &phone_cluts, 0));
    size_t item_size;
    uint8_t *item = load("romfs/game/item/HANDGUN.PLM", &item_size);
    assert(sh_chara_collect_cluts(item, item_size, &phone_cluts, 0));
    free(item);
    item = load("romfs/game/item/KNIFE.PLM", &item_size);
    assert(sh_chara_collect_cluts(item, item_size, &phone_cluts, 0));
    free(item);
    assert(sh_chara_collect_cluts(phone, phone_size, &phone_cluts, 1));
    const int phone_bone = sh_phone_held_bone();
    for (int frame = 1090; frame <= 1147; frame++) {
        size_t count, plain_count;
        assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &plain_count, model, model_size,
            base, base_size, &tim, frame, &phone_cluts, SH_CHARA_F_HARRY_HANDS,
            map, map_size, SH_CHARA_MAP_KF_BASE, NULL, NULL, 0, -1));
        assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size,
            base, base_size, &tim, frame, &phone_cluts, SH_CHARA_F_HARRY_HANDS,
            map, map_size, SH_CHARA_MAP_KF_BASE, &phone_tim, phone, phone_size, phone_bone));
        assert(count > plain_count);
        for (size_t v = plain_count; v < count; v++) {
            for (int axis = 0; axis < 3; axis++) { assert(isfinite(vertices[v].pos[axis])); }
            assert(vertices[v].uv[0] >= 0 && vertices[v].uv[0] <= 1);
            assert(vertices[v].uv[1] >= 0 && vertices[v].uv[1] <= 1);
        }
    }
    free(phone); free(phone_file);
    town.school.overlay = SH_MAP_IDX_MAP1_S02;
    for (unsigned clip = 0; clip < sizeof(s_school_arrival_animations) / sizeof(s_school_arrival_animations[0]); clip++) {
    const ShSchoolAnimation *arrival = &s_school_arrival_animations[clip];
    town.harry_state = arrival->state;
    const int direction = arrival->fps < 0 ? -1 : 1;
    for (int frame = arrival->first; direction > 0 ? frame <= arrival->last : frame >= arrival->last; frame += direction) {
        town.harry_time = frame == arrival->first ? 0 : (frame - arrival->first + 0.25f) / arrival->fps;
        int selected;
        assert(sh_school_harry_keyframe(&town, &selected) && selected == frame);
        ShCharaClutMap cluts = {0};
        size_t count;
        assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size,
                              base, base_size, &tim, selected, &cluts, SH_CHARA_F_HARRY_HANDS,
                              map, map_size, SH_CHARA_MAP_KF_BASE, NULL, NULL, 0, -1));
        assert(count && count % 3 == 0);
        for (size_t vertex = 0; vertex < count; vertex++) {
            for (int axis = 0; axis < 3; axis++) { assert(isfinite(vertices[vertex].pos[axis])); }
        }
    }
    town.harry_time = 100.0f;
    int selected;
    assert(sh_school_harry_keyframe(&town, &selected) && selected == arrival->last);
    }
    /* MAP1_S02 animation106: original clip117 spans960..1016. */
    for (int frame = 960; frame <= 1016; frame++) {
        ShCharaClutMap cluts = {0};
        size_t count;
        assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size,
            base, base_size, &tim, frame, &cluts, SH_CHARA_F_HARRY_HANDS,
            map, map_size, SH_CHARA_MAP_KF_BASE, NULL, NULL, 0, -1));
        assert(count && count % 3 == 0);
        for (size_t vertex = 0; vertex < count; vertex++) {
            for (int axis = 0; axis < 3; axis++) { assert(isfinite(vertices[vertex].pos[axis])); }
        }
    }
    free(map);
    map = load("romfs/game/anim/HB_M1S03.ANM", &map_size);
    for (unsigned clip = 0; clip < sizeof(s_roof_animation_defs) / sizeof(s_roof_animation_defs[0]); clip++) {
        const ShPathAnimDef *animation = &s_roof_animation_defs[clip];
        if (animation->playback != 2) { continue; }
        for (int frame = animation->clip.first; frame <= animation->clip.last; frame++) {
            ShCharaClutMap cluts = {0};
            size_t count;
            assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size,
                base, base_size, &tim, frame, &cluts, SH_CHARA_F_HARRY_HANDS,
                map, map_size, SH_CHARA_MAP_KF_BASE, NULL, NULL, 0, -1));
            assert(count && count % 3 == 0);
            for (size_t vertex = 0; vertex < count; vertex++) {
                for (int axis = 0; axis < 3; axis++) { assert(isfinite(vertices[vertex].pos[axis])); }
            }
        }
    }
    for (unsigned clip = 0; clip < sizeof(s_school_arrival_animations) / sizeof(s_school_arrival_animations[0]); clip++) {
        const ShSchoolAnimation *animation = &s_school_arrival_animations[clip];
        if (animation->state != 59 && animation->state != 60) { continue; }
        for (int frame = animation->first; frame <= animation->last; frame++) {
            ShCharaClutMap cluts = {0};
            size_t count;
            assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &count, model, model_size,
                                  base, base_size, &tim, frame, &cluts, SH_CHARA_F_HARRY_HANDS,
                                  map, map_size, SH_CHARA_MAP_KF_BASE, NULL, NULL, 0, -1));
            assert(count && count % 3 == 0);
            for (size_t vertex = 0; vertex < count; vertex++) {
                for (int axis = 0; axis < 3; axis++) { assert(isfinite(vertices[vertex].pos[axis])); }
            }
        }
    }
    town.stage = SH_TOWN_EXPLORE;
    assert(!sh_school_harry_keyframe(&town, NULL));
    free(vertices);
    free(tim_file);
    free(model);
    free(map);
    free(base);
    puts("PASS: school poses/end holds and real PHONE.PLM on the source left-hand bone with HERO/weapon/PHONE texture slots");
    return 0;
}
