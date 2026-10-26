#include "sh3ds/autotest.h"

#include "sh3ds/game.h"

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *s_script;
static FILE *s_log;
static int s_frames_left;
static uint32_t s_keys;
static uint32_t s_prev_keys;
static int s_cx, s_cy;
static int s_log_period = 30;
static unsigned s_frame;
static bool s_done;
static int s_fire_event = -1;
static bool s_probe;
static bool s_do_warp;
static float s_warp[3]; /* 3DS x, z, yaw degrees */

static uint32_t parse_keys(const char *tok)
{
    static const struct { const char *name; uint32_t key; } names[] = {
        {"A", KEY_A}, {"B", KEY_B}, {"X", KEY_X}, {"Y", KEY_Y}, {"L", KEY_L}, {"R", KEY_R},
        {"START", KEY_START}, {"SELECT", KEY_SELECT}, {"UP", KEY_DUP}, {"DOWN", KEY_DDOWN},
        {"LEFT", KEY_DLEFT}, {"RIGHT", KEY_DRIGHT},
    };
    uint32_t keys = 0;
    char buf[64];
    snprintf(buf, sizeof buf, "%s", tok);
    for (char *p = strtok(buf, "+"); p; p = strtok(NULL, "+")) {
        for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
            if (strcmp(p, names[i].name) == 0) {
                keys |= names[i].key;
            }
        }
    }
    return keys;
}

static void next_step(void)
{
    char line[128];
    while (s_script && fgets(line, sizeof line, s_script)) {
        char tok[64] = "";
        int n = 0, cx = 0, cy = 0;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        if (sscanf(line, "log %d", &n) == 1) {
            s_log_period = n > 0 ? n : 1;
            continue;
        }
        if (sscanf(line, "warp %f %f %f", &s_warp[0], &s_warp[1], &s_warp[2]) == 3) {
            s_do_warp = true;
            continue;
        }
        if (strncmp(line, "probe", 5) == 0) {
            s_probe = true;
            continue;
        }
        if (sscanf(line, "event %d", &n) == 1) {
            s_fire_event = n;
            continue;
        }
        if (strncmp(line, "quit", 4) == 0) {
            break;
        }
        const int got = sscanf(line, "%d %63s %d %d", &n, tok, &cx, &cy);
        if (got >= 2 && n > 0) {
            s_frames_left = n;
            s_keys = parse_keys(tok);
            s_cx = got >= 4 ? cx : 0;
            s_cy = got >= 4 ? cy : 0;
            return;
        }
    }
    if (!s_done && s_log) {
        fprintf(s_log, "AUTOTEST END\n");
        fflush(s_log);
    }
    s_done = true;
    s_keys = 0;
    s_cx = s_cy = 0;
}

bool sh_autotest_init(void)
{
    s_script = fopen("sdmc:/sh3ds_autotest.txt", "r");
    if (!s_script) {
        return false;
    }
    s_log = fopen("sdmc:/sh3ds_autotest.log", "w");
    next_step();
    return true;
}

bool sh_autotest_active(void)
{
    return s_script != NULL;
}

bool sh_autotest_done(void)
{
    return s_script && s_done;
}

void sh_autotest_input(ShInputState *io)
{
    if (!s_script || !io) {
        return;
    }
    if (!s_done && s_frames_left <= 0) {
        next_step();
    }
    io->held = s_keys;
    io->down = s_keys & ~s_prev_keys;
    io->up = s_prev_keys & ~s_keys;
    io->circle_x = (int16_t)s_cx;
    io->circle_y = (int16_t)s_cy;
    s_prev_keys = s_keys;
    s_frames_left--;
}

void sh_autotest_log(ShGame *g)
{
    if (!s_log || !g) {
        return;
    }
    if (s_fire_event >= 0) {
        const bool ok = sh_town_debug_fire_event(&g->town, s_fire_event);
        fprintf(s_log, "EVENT %d fired=%d\n", s_fire_event, ok);
        s_fire_event = -1;
    }
    if (s_do_warp) {
        s_do_warp = false;
        g->player_pos[0] = s_warp[0];
        g->player_pos[1] = 0.0f;
        g->player_pos[2] = s_warp[1];
        g->player_yaw = s_warp[2] / 57.2958f;
        g->town.need_ground_snap = true;
        sh_vc_warp(&g->town.vc);
        fprintf(s_log, "WARP %.2f,%.2f yaw=%.1f\n", s_warp[0], s_warp[1], s_warp[2]);
    }
    if (s_probe) {
        s_probe = false;
        fprintf(s_log, "PROBE ground around %.2f,%.2f (rows -z..+z, cols -x..+x, step 0.5)\n",
                g->player_pos[0], g->player_pos[2]);
        for (int iz = -8; iz <= 8; iz++) {
            for (int ix = -8; ix <= 8; ix++) {
                ShCollHit h = {0};
                const float px = g->player_pos[0] + ix * 0.5f;
                const float pz = g->player_pos[2] + iz * 0.5f;
                if (sh_coll_ground_near(&g->collision, px, pz, g->player_pos[1], &h) && h.valid) {
                    fprintf(s_log, "%6.2f", h.height);
                } else {
                    fprintf(s_log, "    --");
                }
            }
            fputc('\n', s_log);
        }
        fprintf(s_log, "PROBE walk reach (deg 3DS yaw -> metres, max 4):");
        for (int d = 0; d < 16; d++) {
            const float a = d * (6.2831853f / 16.0f);
            float x = g->player_pos[0], y = g->player_pos[1], z = g->player_pos[2];
            for (int s = 0; s < 40; s++) {
                sh_coll_move(&g->collision, &x, &y, &z, -sinf(a) * 0.1f, -cosf(a) * 0.1f,
                             SH_COLL_PLAYER_RADIUS, 0.0f);
            }
            const float rx = x - g->player_pos[0], rz = z - g->player_pos[2];
            fprintf(s_log, " %d:%.1f/y%.2f", d * 360 / 16, sqrtf(rx * rx + rz * rz), y);
        }
        fputc('\n', s_log);
    }
    if (s_frame++ % (unsigned)s_log_period != 0 && !s_done) {
        return;
    }
    ShCollHit hit = {0};
    const bool ground = sh_coll_ground_near(&g->collision, g->player_pos[0], g->player_pos[2],
                                            g->player_pos[1], &hit);
    const ShVc *vc = &g->town.vc;
    for (int i = 0; i < g->town.dog_count; i++) {
        const ShTownDog *d = &g->town.dogs[i];
        if (d->alive) {
            fprintf(s_log, "npc %d slot=%d pos=%.1f,%.1f st=%d\n", i, d->spawn, d->pos[0], d->pos[2], d->state);
        }
    }
    fprintf(s_log, "wall %.2f,%.2f top=%.2f s=%.0f/%.0f\n", sh_coll_last_wall[0], sh_coll_last_wall[1],
            sh_coll_last_wall[2], sh_coll_last_wall[3], sh_coll_last_wall[4]);
    fprintf(s_log,
            "timing light=%d bird_anim=%.3f bird_pos=%.3f,%.3f,%.3f play_time=%.3f weapon=%u\n",
            sh_save_flashlight_on(&g->save), g->cafe.bird_anim,
            g->cafe.bird_pos[0], g->cafe.bird_pos[1], g->cafe.bird_pos[2],
            SH_Q12_TO_FLOAT(g->save.gameplay_timer), (unsigned)g->save.equipped_weapon);
    fprintf(s_log,
            "status cafe=%d inv=%d map_open=%d hp=%.3f saturation=%.3f radio=%.3f powered=%d noise=%d\n",
            (int)g->cafe.stage, g->inventory.open,
            sh_cafe_map_open(&g->cafe) || sh_town_map_open(&g->town),
            sh_save_health_f(&g->save), SH_Q12_TO_FLOAT(g->save.health_saturation),
            g->radio.intensity, g->radio.powered, g->radio.noise_on);
    if (g->save.map_idx == SH_MAP_IDX_MAP1_S00 || g->save.map_idx == SH_MAP_IDX_MAP1_S01) {
        fprintf(s_log, "area=%s school=%d/%d image=%s shade=%u objects=%d mask=%u\n", g->area_names[g->area_index],
                g->town.school.callback, g->town.school.substep,
                g->town.school.image ? g->town.school.image : "-", g->town.school.brightness,
                g->job.object_count, (unsigned)sh_school_prop_mask(&g->town, &g->save, g->player_pos[0], g->player_pos[2]));
        fprintf(s_log, "paper_flags=%lu school_map_taken=%d\n", (unsigned long)g->save.paper_map_flags,
                sh_save_event_get(&g->save, 67));
        fprintf(s_log, "chemical=%u gold=%u\n", (unsigned)sh_save_item_count(&g->save, 96),
                (unsigned)sh_save_item_count(&g->save, 97));
        fprintf(s_log, "ladder=%d,%d,%d,%d,%d,%d,%d\n", sh_save_event_get(&g->save, 78),
                sh_save_event_get(&g->save, 79), sh_save_event_get(&g->save, 80), sh_save_event_get(&g->save, 81),
                sh_save_event_get(&g->save, 82), sh_save_event_get(&g->save, 184), sh_save_event_get(&g->save, 185));
        fprintf(s_log, "piano=%d notes=%d cursor=%d xy=%ld,%ld silver_motion=%d moving_mesh=%u\n",
                sh_save_event_get(&g->save, 75), g->town.school.piano.counter,
                g->town.school.piano_result.draw_cursor, (long)g->town.school.piano.x,
                (long)g->town.school.piano.y, g->town.school.silver_active,
                (unsigned)sh_gfx_moving_vertex_count());
        fprintf(s_log, "acid=%d smoke=%d mesh=%u border=%u/%ld harry=%d/%d\n", sh_save_event_get(&g->save, 74),
                g->town.school.smoke.active, (unsigned)sh_gfx_smoke_vertex_count(), g->town.border.state,
                (long)g->town.border.shade, g->town.harry_state, g->harry_anim_kf);
    }
    if (g->title_sel >= 9 && sh_town_active(&g->town)) {
        fprintf(s_log, "checkpoint=%d keys=%d,%d,%d chemical=%d gold=%d silver=%d clock=%d,%d boiler=%d eclipse=%d\n",
                g->title_sel - 9, sh_save_item_count(&g->save, 66), sh_save_item_count(&g->save, 67),
                sh_save_item_count(&g->save, 68), sh_save_item_count(&g->save, 96),
                sh_save_item_count(&g->save, 97), sh_save_item_count(&g->save, 98),
                sh_save_event_get(&g->save, 71), sh_save_event_get(&g->save, 72),
                sh_save_event_get(&g->save, 73), sh_save_event_get(&g->save, 159));
    }
    fprintf(s_log,
            "f=%u phase=%d map=%d stage=%d scr=%d/%d pos=%.2f,%.2f,%.2f yaw=%.1f ground=%d gy=%.2f "
            "cam=%.2f,%.2f,%.2f keys=%lx\n",
            s_frame - 1, (int)g->phase, (int)g->save.map_idx, (int)g->town.stage, (int)g->town.script, g->town.step, g->player_pos[0],
            g->player_pos[1], g->player_pos[2], g->player_yaw * 57.2958f, ground && hit.valid,
            hit.height, vc->cam_pos[0], vc->cam_pos[1], vc->cam_pos[2], (unsigned long)s_keys);
    fflush(s_log);
}

void sh_autotest_shutdown(void)
{
    if (s_log) {
        fclose(s_log);
    }
    if (s_script) {
        fclose(s_script);
    }
    s_log = s_script = NULL;
}
