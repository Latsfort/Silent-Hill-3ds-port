/*
 * Host-side copy of the 3DS third-person frame: 3x3 map window, IPD collision
 * spawn, Harry baked by chara.c and the chase camera, software-rendered to PPM.
 * usage: harry_view <BG dir> <CHARA dir> <ANIM dir> <area> <cx> <cz> <yaw deg> <out.ppm>
 */
#include "sh3ds/chara.h"
#include "sh3ds/coll.h"
#include "sh3ds/map.h"
#include "sh3ds/maptex.h"
#include "sh3ds/tim.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 400
#define H 240
#define PAGES SH_MAPTEX_MAX_PAGES

static uint32_t g_atlas[PAGES][SH_ATLAS_SIZE * SH_ATLAS_SIZE];
static float g_depth[W * H];
static uint8_t g_rgb[W * H * 3];

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(*size);
    fread(data, 1, *size, f);
    fclose(f);
    return data;
}

static void fill_atlas(const char *bg, const ShMapTexSet *set)
{
    for (int i = 0; i < set->count; i++) {
        const ShMapTexPair *p = &set->pairs[i];
        if (p->page < 0) {
            continue;
        }
        char name[9] = {0}, path[512];
        memcpy(name, p->name, 8);
        snprintf(path, sizeof(path), "%s/%s.TIM", bg, name);
        size_t size = 0;
        uint8_t *file = read_file(path, &size);
        ShTim tim;
        if (!file || !sh_tim_parse(file, size, &tim)) {
            free(file);
            continue;
        }
        const int clut = sh_tim_clut_offset(&tim, p->clut_delta);
        for (int y = p->min_v; y <= p->max_v && y < tim.height; y++) {
            for (int x = p->min_u; x <= p->max_u && x < tim.width; x++) {
                g_atlas[p->page][(p->y + y - p->min_v) * SH_ATLAS_SIZE + p->x + x - p->min_u] =
                    sh_tim_pixel_rgba_clut(&tim, x, y, clut);
            }
        }
        free(file);
    }
}

static void raster(const float v[3][5], const uint8_t rgb[3], int page, bool fog_on)
{
    const float minx = fminf(v[0][0], fminf(v[1][0], v[2][0]));
    const float maxx = fmaxf(v[0][0], fmaxf(v[1][0], v[2][0]));
    const float miny = fminf(v[0][1], fminf(v[1][1], v[2][1]));
    const float maxy = fmaxf(v[0][1], fmaxf(v[1][1], v[2][1]));
    const int x0 = (int)fmaxf(0, floorf(minx)), x1 = (int)fminf(W - 1, ceilf(maxx));
    const int y0 = (int)fmaxf(0, floorf(miny)), y1 = (int)fminf(H - 1, ceilf(maxy));
    const float area = (v[1][0] - v[0][0]) * (v[2][1] - v[0][1]) - (v[2][0] - v[0][0]) * (v[1][1] - v[0][1]);
    if (fabsf(area) < 1e-6f) {
        return;
    }

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            const float w0 = ((v[1][0] - px) * (v[2][1] - py) - (v[2][0] - px) * (v[1][1] - py)) / area;
            const float w1 = ((v[2][0] - px) * (v[0][1] - py) - (v[0][0] - px) * (v[2][1] - py)) / area;
            const float w2 = 1.0f - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) {
                continue;
            }
            const float iw = w0 * v[0][2] + w1 * v[1][2] + w2 * v[2][2];
            if (iw <= g_depth[y * W + x]) {
                continue;
            }

            float r = rgb[0], g = rgb[1], b = rgb[2];
            if (page >= 0) {
                const float u = (w0 * v[0][3] + w1 * v[1][3] + w2 * v[2][3]) / iw;
                const float t = (w0 * v[0][4] + w1 * v[1][4] + w2 * v[2][4]) / iw;
                int tx = (int)(u * SH_ATLAS_SIZE), ty = (int)((1.0f - t) * SH_ATLAS_SIZE);
                tx = tx < 0 ? 0 : tx > SH_ATLAS_SIZE - 1 ? SH_ATLAS_SIZE - 1 : tx;
                ty = ty < 0 ? 0 : ty > SH_ATLAS_SIZE - 1 ? SH_ATLAS_SIZE - 1 : ty;
                const uint32_t c = g_atlas[page][ty * SH_ATLAS_SIZE + tx];
                if ((c & 0xFF) == 0) {
                    continue;
                }
                r = ((c >> 24) & 0xFF) * rgb[0] / 255.0f;
                g = ((c >> 16) & 0xFF) * rgb[1] / 255.0f;
                b = ((c >> 8) & 0xFF) * rgb[2] / 255.0f;
            }

            const float dist = 1.0f / iw;
            const float fog = fog_on ? expf(-powf(0.032f * dist, 1.7f)) : 1.0f;
            g_depth[y * W + x] = iw;
            g_rgb[(y * W + x) * 3 + 0] = (uint8_t)(r * fog + 0x90 * (1 - fog));
            g_rgb[(y * W + x) * 3 + 1] = (uint8_t)(g * fog + 0x8C * (1 - fog));
            g_rgb[(y * W + x) * 3 + 2] = (uint8_t)(b * fog + 0x8C * (1 - fog));
        }
    }
}

typedef struct View {
    float pos[3], yaw, pitch, focal;
} View;

static bool project(const View *view, const float p[3], float out[5], float u, float v)
{
    const float x = p[0] - view->pos[0], y = p[1] - view->pos[1], z = p[2] - view->pos[2];
    const float s = sinf(view->yaw), c = cosf(view->yaw);
    const float vx = c * x - s * z;
    const float vz0 = s * x + c * z;
    const float sp = sinf(view->pitch), cp = cosf(view->pitch);
    const float vy = cp * y + sp * vz0;
    const float vz = -sp * y + cp * vz0;
    const float depth = -vz;
    if (depth < 0.1f) {
        return false;
    }
    out[0] = W * 0.5f + vx / depth * view->focal;
    out[1] = H * 0.5f - vy / depth * view->focal;
    out[2] = 1.0f / depth;
    out[3] = u / depth;
    out[4] = v / depth;
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 9 || argc > 10) {
        fprintf(stderr, "usage: harry_view <BG> <CHARA> <ANIM> <area> <cx> <cz> <yaw deg> <out.ppm> [keyframe]\n");
        return 1;
    }
    const char *bg = argv[1], *area = argv[4];
    const int cx = atoi(argv[5]), cz = atoi(argv[6]);
    const float yaw = (float)atof(argv[7]) * 3.14159265f / 180.0f;

    char path[512];
    size_t glb_size = 0;
    snprintf(path, sizeof(path), "%s/%s_GLB.PLM", bg, area);
    uint8_t *glb = read_file(path, &glb_size);

    static ShMapVertex built[90000 * 3], verts[90000 * 3];
    static uint8_t slots[90000];
    static ShMapTexSet set;
    static ShCollWorld world;
    sh_maptex_reset(&set);
    sh_coll_world_reset(&world);
    ShMapBuilder b;
    sh_map_builder_init(&b, built, slots, sizeof(built) / sizeof(built[0]), sh_maptex_resolve, &set);

    for (int dz = -1; dz <= 1; dz++) {
        for (int dx = -1; dx <= 1; dx++) {
            size_t size = 0;
            snprintf(path, sizeof(path), "%s/%s%02X%02X.IPD", bg, area, (cx + dx) & 0xFF, (cz + dz) & 0xFF);
            uint8_t *ipd = read_file(path, &size);
            if (ipd) {
                sh_map_add_chunk(&b, ipd, size, glb, glb_size);
                sh_coll_world_add(&world, ipd, size);
            }
        }
    }

    sh_maptex_measure(&set, built, slots, b.count);
    const int pages = sh_maptex_pack(&set, PAGES);
    ShMapRange ranges[PAGES + 1];
    const int range_count = sh_maptex_finalize(&set, pages, built, slots, b.count, verts, ranges);
    fill_atlas(bg, &set);

    /* Harry */
    size_t ilm_size = 0, anm_size = 0;
    snprintf(path, sizeof(path), "%s/HERO.ILM", argv[2]);
    uint8_t *ilm = read_file(path, &ilm_size);
    snprintf(path, sizeof(path), "%s/HB_BASE.ANM", argv[3]);
    uint8_t *anm = read_file(path, &anm_size);
    static ShMapVertex harry[SH_CHARA_MAX_VERTS];
    size_t harry_count = 0;
    ShTim hero_tim;
    size_t tim_size = 0;
    snprintf(path, sizeof(path), "%s/HERO.TIM", argv[2]);
    uint8_t *tim_file = read_file(path, &tim_size);
    const ShTim *tim_ptr = (tim_file && sh_tim_parse(tim_file, tim_size, &hero_tim)) ? &hero_tim : NULL;
    int keyframe = SH_CHARA_KF_IDLE_START;
    if (argc >= 10) {
        keyframe = atoi(argv[9]);
    }
    const bool baked = ilm && anm && sh_chara_bake_harry(harry, SH_CHARA_MAX_VERTS, &harry_count, ilm, ilm_size,
                                                         anm, anm_size, tim_ptr, keyframe, NULL);
    float hmin[3] = {1e9f, 1e9f, 1e9f}, hmax[3] = {-1e9f, -1e9f, -1e9f};
    for (size_t i = 0; i < harry_count; i++) {
        for (int k = 0; k < 3; k++) {
            hmin[k] = fminf(hmin[k], harry[i].pos[k]);
            hmax[k] = fmaxf(hmax[k], harry[i].pos[k]);
        }
    }
    printf("harry baked=%d verts=%zu bounds x[%.2f %.2f] y[%.2f %.2f] z[%.2f %.2f]\n", baked, harry_count, hmin[0],
           hmax[0], hmin[1], hmax[1], hmin[2], hmax[2]);

    /* Spawn like game.c */
    float player[3] = {(cx + 0.5f) * 40.0f, 0.0f, -(cz + 0.5f) * 40.0f};
    ShCollHit hit;
    const bool ground = sh_coll_ground(&world, player[0], player[2], &hit);
    if (ground) {
        player[1] = hit.height;
    }
    printf("spawn ground=%d at (%.2f %.2f %.2f)\n", ground, player[0], player[1], player[2]);

    /* Chase camera like game.c. */
    const float cam_h = 1.55f, look_h = 1.20f, reach = 2.35f + 0.3f;
    const float t = sh_coll_ray(&world, player[0], player[2], player[0] + sinf(yaw) * reach,
                                player[2] + cosf(yaw) * reach, player[1], 0.6f);
    float dist = 2.35f;
    if (t * reach - 0.3f < dist) {
        dist = fmaxf(0.6f, t * reach - 0.3f);
    }
    printf("camera ray t=%.2f dist=%.2f\n", t, dist);
    View view;
    view.pos[0] = player[0] + sinf(yaw) * dist;
    view.pos[1] = player[1] + cam_h;
    view.pos[2] = player[2] + cosf(yaw) * dist;
    const float ddx = player[0] - view.pos[0], ddy = player[1] + look_h - view.pos[1], ddz = player[2] - view.pos[2];
    view.yaw = atan2f(-ddx, -ddz);
    view.pitch = atan2f(ddy, sqrtf(ddx * ddx + ddz * ddz));
    view.focal = (H * 0.5f) / tanf(48.0f * 3.14159265f / 360.0f);
    printf("camera (%.2f %.2f %.2f) yaw %.2f pitch %.2f\n", view.pos[0], view.pos[1], view.pos[2], view.yaw,
           view.pitch);

    for (int i = 0; i < W * H; i++) {
        g_depth[i] = 0.0f;
        g_rgb[i * 3 + 0] = 0x90;
        g_rgb[i * 3 + 1] = 0x8C;
        g_rgb[i * 3 + 2] = 0x8C;
    }

    for (int r = 0; r < range_count; r++) {
        for (size_t t = ranges[r].first; t < ranges[r].first + ranges[r].count; t += 3) {
            float sv[3][5];
            bool ok = true;
            for (int k = 0; k < 3 && ok; k++) {
                ok = project(&view, verts[t + k].pos, sv[k], verts[t + k].uv[0], verts[t + k].uv[1]);
            }
            if (ok) {
                static const uint8_t magenta[3] = {255, 0, 255};
                const bool mark = getenv("MARK_UNTEXTURED") && ranges[r].page < 0;
                raster(sv, mark ? magenta : verts[t].color, ranges[r].page, !mark);
            }
        }
    }

    int harry_px = 0;
    const float s = sinf(yaw), c = cosf(yaw);
    for (size_t t = 0; t + 2 < harry_count; t += 3) {
        float sv[3][5];
        bool ok = true;
        for (int k = 0; k < 3 && ok; k++) {
            /* Model matrix: translate(player) * rotateY(yaw). */
            const float *p = harry[t + k].pos;
            const float wp[3] = {player[0] + c * p[0] + s * p[2], player[1] + p[1], player[2] - s * p[0] + c * p[2]};
            ok = project(&view, wp, sv[k], 0, 0);
        }
        if (ok) {
            raster(sv, harry[t].color, -1, false);
            harry_px++;
        }
    }
    printf("harry triangles projected: %d\n", harry_px);

    FILE *out = fopen(argv[8], "wb");
    fprintf(out, "P6\n%d %d\n255\n", W, H);
    fwrite(g_rgb, 1, sizeof(g_rgb), out);
    fclose(out);
    return 0;
}
