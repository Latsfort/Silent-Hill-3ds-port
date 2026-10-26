/*
 * Host-side check of the textured map pipeline (map.c + maptex.c + tim.c):
 * builds the 3x3 chunks around a point, packs the atlas like the 3DS build
 * and software-renders the result to a PPM.
 * usage: map_render <BG dir> <area> <chunk x> <chunk z> <yaw deg> <out.ppm> [atlas.ppm]
 */
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
            fprintf(stderr, "missing texture %s\n", path);
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

static float g_depth[W * H];
static uint8_t g_rgb[W * H * 3];

static void raster(const float v[3][5], const uint8_t shade, int page)
{
    /* v[i] = screen x, screen y, 1/w, u/w, v/w */
    float minx = fminf(v[0][0], fminf(v[1][0], v[2][0]));
    float maxx = fmaxf(v[0][0], fmaxf(v[1][0], v[2][0]));
    float miny = fminf(v[0][1], fminf(v[1][1], v[2][1]));
    float maxy = fmaxf(v[0][1], fmaxf(v[1][1], v[2][1]));
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

            float r = shade, g = shade, b = shade;
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
                r = ((c >> 24) & 0xFF) * shade / 255.0f;
                g = ((c >> 16) & 0xFF) * shade / 255.0f;
                b = ((c >> 8) & 0xFF) * shade / 255.0f;
            }

            /* Same exponential fog as the 3DS build. */
            const float dist = 1.0f / iw;
            const float fog = expf(-powf(0.032f * dist, 1.7f));
            g_depth[y * W + x] = iw;
            g_rgb[(y * W + x) * 3 + 0] = (uint8_t)(r * fog + 0x90 * (1 - fog));
            g_rgb[(y * W + x) * 3 + 1] = (uint8_t)(g * fog + 0x8C * (1 - fog));
            g_rgb[(y * W + x) * 3 + 2] = (uint8_t)(b * fog + 0x8C * (1 - fog));
        }
    }
}

static void write_atlas(const char *path, int pages)
{
    FILE *out = fopen(path, "wb");
    fprintf(out, "P6\n%d %d\n255\n", SH_ATLAS_SIZE / 2 * pages, SH_ATLAS_SIZE / 2);
    for (int y = 0; y < SH_ATLAS_SIZE; y += 2) {
        for (int p = 0; p < pages; p++) {
            for (int x = 0; x < SH_ATLAS_SIZE; x += 2) {
                const uint32_t c = g_atlas[p][y * SH_ATLAS_SIZE + x];
                const uint8_t rgb[3] = {c >> 24, c >> 16, c >> 8};
                fwrite(rgb, 1, 3, out);
            }
        }
    }
    fclose(out);
}

int main(int argc, char **argv)
{
    if (argc != 7 && argc != 8) {
        fprintf(stderr, "usage: map_render <BG dir> <area> <cx> <cz> <yaw deg> <out.ppm> [atlas.ppm]\n");
        return 1;
    }
    const char *bg = argv[1];
    const char *area = argv[2];
    const int cx = atoi(argv[3]), cz = atoi(argv[4]);
    const float yaw = (float)atof(argv[5]) * 3.14159265f / 180.0f;

    char path[512];
    size_t glb_size = 0;
    snprintf(path, sizeof(path), "%s/%s_GLB.PLM", bg, area);
    uint8_t *glb = read_file(path, &glb_size);

    static ShMapVertex built[90000 * 3], verts[90000 * 3];
    static uint8_t slots[90000];
    static ShMapTexSet set;
    sh_maptex_reset(&set);
    ShMapBuilder b;
    sh_map_builder_init(&b, built, slots, sizeof(built) / sizeof(built[0]), sh_maptex_resolve, &set);

    for (int ring = 0; ring <= 1; ring++) {
        for (int dz = -ring; dz <= ring; dz++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (abs(dx) != ring && abs(dz) != ring) {
                    continue;
                }
                size_t size = 0;
                snprintf(path, sizeof(path), "%s/%s%02X%02X.IPD", bg, area, (cx + dx) & 0xFF, (cz + dz) & 0xFF);
                uint8_t *ipd = read_file(path, &size);
                if (ipd) {
                    sh_map_add_chunk(&b, ipd, size, glb, glb_size);
                }
            }
        }
    }

    sh_maptex_measure(&set, built, slots, b.count);
    const int pages = sh_maptex_pack(&set, PAGES);
    ShMapRange ranges[PAGES + 1];
    const int range_count = sh_maptex_finalize(&set, pages, built, slots, b.count, verts, ranges);
    fill_atlas(bg, &set);

    int unplaced = 0;
    long texels = 0;
    for (int i = 0; i < set.count; i++) {
        const ShMapTexPair *p = &set.pairs[i];
        unplaced += p->page < 0 && p->max_u >= p->min_u;
        texels += (long)(p->max_u - p->min_u + 1) * (p->max_v - p->min_v + 1);
    }
    printf("%s (%d,%d): %zu triangles, %d pairs, %ld texels, %d pages, %d unplaced, %d ranges\n", area, cx, cz,
           b.count / 3, set.count, texels, pages, unplaced, range_count);

    for (int i = 0; i < W * H; i++) {
        g_depth[i] = 0.0f;
        g_rgb[i * 3 + 0] = 0x90;
        g_rgb[i * 3 + 1] = 0x8C;
        g_rgb[i * 3 + 2] = 0x8C;
    }

    const float cam[3] = {(cx + 0.5f) * 40.0f, 1.6f, -(cz + 0.5f) * 40.0f};
    const float s = sinf(yaw), c = cosf(yaw);
    const float focal = (H * 0.5f) / tanf(62.0f * 3.14159265f / 360.0f);

    for (int r = 0; r < range_count; r++) {
        for (size_t t = ranges[r].first; t < ranges[r].first + ranges[r].count; t += 3) {
            float sv[3][5];
            bool ok = true;
            for (int k = 0; k < 3 && ok; k++) {
                const ShMapVertex *v = &verts[t + k];
                const float x = v->pos[0] - cam[0], y = v->pos[1] - cam[1], z = v->pos[2] - cam[2];
                /* View space for a camera rotated by yaw around Y, looking down -Z. */
                const float vx = c * x - s * z;
                const float vz = s * x + c * z;
                const float depth = -vz;
                if (depth < 0.1f) {
                    ok = false;
                    break;
                }
                sv[k][0] = W * 0.5f + vx / depth * focal;
                sv[k][1] = H * 0.5f - y / depth * focal;
                sv[k][2] = 1.0f / depth;
                sv[k][3] = v->uv[0] / depth;
                sv[k][4] = v->uv[1] / depth;
            }
            if (ok) {
                raster(sv, verts[t].color[0], ranges[r].page);
            }
        }
    }

    FILE *out = fopen(argv[6], "wb");
    fprintf(out, "P6\n%d %d\n255\n", W, H);
    fwrite(g_rgb, 1, sizeof(g_rgb), out);
    fclose(out);
    if (argc == 8 && pages > 0) {
        write_atlas(argv[7], pages);
    }
    return 0;
}
