#define main school_common_main
#include "school_regression.c"
#undef main

static uint8_t *valve_asset(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    *size = ftell(file);
    rewind(file);
    uint8_t *data = malloc(*size);
    assert(data && fread(data, 1, *size, file) == *size);
    fclose(file);
    return data;
}

int main(void)
{
    assert(school_common_main() == 0);
    size_t base_size, map_size, model_size, tim_size;
    uint8_t *base = valve_asset("romfs/game/anim/HB_BASE.ANM", &base_size);
    uint8_t *map = valve_asset("romfs/game/anim/HB_M1S03.ANM", &map_size);
    uint8_t *model = valve_asset("romfs/game/chara/HERO.ILM", &model_size);
    uint8_t *tim_data = valve_asset("romfs/game/chara/HERO.TIM", &tim_size);
    ShTim tim;
    assert(sh_tim_parse(tim_data, tim_size, &tim));
    ShMapVertex *vertices = calloc(SH_CHARA_MAX_VERTS, sizeof(*vertices));
    assert(vertices);
    for (int rate = 30; rate <= 60; rate *= 2) {
        for (int ball = 0; ball <= 1; ball++) {
            ShTown town = {0};
            ShSavegame save = {0};
            save.map_idx = SH_MAP_IDX_MAP1_S03;
            sh_save_event_set(&save, 112);
            if (ball) { sh_save_event_set(&save, 111); sh_save_item_add(&save, 99, 1); }
            sh_school_roof_world_reset(&town);
            int count;
            const ShMapEvent *events = sh_school_map_events(save.map_idx, &count), *event = NULL;
            for (int i = 0; i < count; i++) {
                if (events[i].event_param == 8 && events[i].point_idx == 21) { event = &events[i]; }
            }
            assert(event && sh_school_event_enabled(event, &save));
            assert(sh_school_begin(&town, &save, event));
            const ShMapPoint *points = sh_school_map_points(save.map_idx, &count);
            float x = points[event->point_idx].x, y = 0, z = -points[event->point_idx].z, yaw = 0;
            const unsigned starts = valve_water_starts, stops = valve_water_stops;
            const unsigned squeaks = valve_squeaks, clicks = valve_clicks;
            bool visited[14] = {false}, saw_water = false;
            int frames = 0, wrong_hole_wait = 0;
            for (; frames < rate * 40 && town.stage == SH_TOWN_SCRIPT; frames++) {
                if (town.step >= 0 && town.step < 14) { visited[town.step] = true; }
                int keyframe = SH_CHARA_KF_IDLE_START;
                sh_school_harry_keyframe(&town, &keyframe);
                ShCharaClutMap cluts = {0};
                size_t vertex_count;
                assert(sh_chara_bake(vertices, SH_CHARA_MAX_VERTS, &vertex_count, model, model_size,
                    base, base_size, &tim, keyframe, &cluts, SH_CHARA_F_HARRY_HANDS,
                    map, map_size, SH_CHARA_MAP_KF_BASE, NULL, NULL, 0, -1));
                ShCharaBone hand, world;
                assert(sh_chara_last_bone(sh_drain_valve_hand_bone(), &hand));
                const int32_t position[3] = {SH_Q12(x), -SH_Q12(y), -SH_Q12(z)};
                const int angle = SH_Q12(-yaw / (2.0f * 3.14159265358979323846f));
                sh_drain_valve_world_bone(&hand, position, angle, &world);
                sh_school_seed_harry_hand(&town, &world);
                sh_school_seed_harry_position(&town, x, y, z);
                sh_school_seed_harry_keyframe(&town, keyframe);
                uint32_t down = town.step == 2 ? SH_BTN_ACTION : 0;
                if (town.step == 10 && !ball) {
                    if (++wrong_hole_wait >= 8) { down = SH_BTN_ACTION; }
                }
                tick(&town, &save, 1.0f / rate, down);
                sh_school_take_player_warp(&town, &x, &z, &yaw);
                const ShRoofWater *water = sh_school_roof_water(&town);
                if (water) {
                    ShEnvState environment = {0};
                    environment.preset.flags[1] = 128;
                    environment.preset.world_light = 128 * 32;
                    sh_school_roof_water_update(&town, 0, &environment, NULL, 1.0f / rate);
                    assert(town.school.player_hidden && !sh_save_event_get(&save, 113));
                    assert(water->surface_count > 0);
                    saw_water = true;
                }
            }
            assert(frames < rate * 40 && saw_water && !town.controls_locked);
            assert(sh_save_event_get(&save, 1364) && sh_save_event_get(&save, 113) == (ball != 0));
            assert(!town.school.player_hidden && !town.school.drain_light_override && !sh_school_roof_water(&town));
            assert(!town.school.empty_hand && !town.cam_active);
            assert(visited[0] && visited[2] && visited[4] && visited[5] && visited[6] && visited[7]);
            assert(visited[8] && visited[9] && visited[10] && visited[11] && visited[12] && visited[13]);
            assert(ball || wrong_hole_wait >= 8);
            printf("Valve scene %dfps ball=%d: %d frames, squeaks=%u clicks=%u starts=%u stops=%u\n", rate, ball,
                frames, valve_squeaks - squeaks, valve_clicks - clicks, valve_water_starts - starts, valve_water_stops - stops);
            assert(valve_squeaks > squeaks && valve_clicks > clicks);
            assert(valve_water_starts == starts + 1 && valve_water_stops == stops + 1);
            if (ball) { assert(sh_save_item_count(&save, 99) == 1); }
            const int16_t final_rotation = town.school.valve.rotation;
            const ShMapEvent message_event = {.sys_state = SH_SYS_READ_MESSAGE, .event_param = 0};
            assert(sh_school_begin(&town, &save, &message_event));
            assert(town.school.valve.rotation == final_rotation && town.school.valve_active);
            sh_school_roof_world_reset(&town);
            assert(!town.school.valve.rotation && !town.school.valve_active && !town.school.hand_world_valid);
        }
    }
    free(vertices); free(base); free(map); free(model); free(tim_data);
    puts("PASS: complete valve event at 30/60fps with original Harry poses, both water circuits and control restoration");
    return 0;
}
