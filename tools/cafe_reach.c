/* usage: cafe_reach <BG dir> — flood-fills where Harry can stand in Cafe 5to2 (THR0006)
 * and reports the closest reachable distance to each MAP_POINTS entry. */
#include "sh3ds/coll.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define GX 120
#define GZ 160
#define X0 (-2.0f)
#define Z0 (262.0f) /* PS1 z */
#define CELL 0.1f

static signed char seen[GX][GZ];
static float ys[GX][GZ];

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: cafe_reach <BG>\n");
        return 1;
    }
    ShCollWorld world;
    sh_coll_world_reset(&world);
    const char *names[] = {"THR0005.IPD", "THR0006.IPD", "THR0106.IPD", "THR0105.IPD", "THRFF06.IPD"};
    char path[512];
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        size_t size = 0;
        snprintf(path, sizeof(path), "%s/%s", argv[1], names[i]);
        uint8_t *ipd = read_file(path, &size);
        if (ipd) {
            sh_coll_world_add(&world, ipd, size);
        }
    }

    int qx[GX * GZ], qz[GX * GZ], qh = 0, qt = 0;
    const int sx = (int)((3.0f - X0) / CELL), sz = (int)((270.0f - Z0) / CELL);
    ShCollHit hit;
    sh_coll_ground(&world, 3.0f, -270.0f, &hit);
    seen[sx][sz] = 1;
    ys[sx][sz] = hit.height;
    qx[qt] = sx;
    qz[qt++] = sz;
    float max_y = -99.0f, max_x = 0, max_z = 0;
    while (qh < qt) {
        const int cx = qx[qh], cz = qz[qh++];
        for (int d = 0; d < 8; d++) {
            static const int dx8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
            static const int dz8[8] = {0, 0, 1, -1, 1, -1, 1, -1};
            float x = X0 + (cx + 0.5f) * CELL, z = -(Z0 + (cz + 0.5f) * CELL), y = ys[cx][cz];
            sh_coll_move(&world, &x, &y, &z, dx8[d] * CELL, -dz8[d] * CELL, SH_COLL_PLAYER_RADIUS, 0.0f);
            const int nx = (int)floorf((x - X0) / CELL), nz = (int)floorf((-z - Z0) / CELL);
            if (nx < 0 || nz < 0 || nx >= GX || nz >= GZ || seen[nx][nz]) {
                continue;
            }
            seen[nx][nz] = 1;
            ys[nx][nz] = y;
            if (y > max_y) {
                max_y = y;
                max_x = x;
                max_z = -z;
            }
            qx[qt] = nx;
            qz[qt++] = nz;
        }
    }
    printf("reachable cells: %d, max floor y=%.2f at PS1 (%.2f, %.2f)\n", qt, max_y, max_x, max_z);

    static const float pts[15][2] = {{1.0f, 260.0f}, {5.1f, 274.1f}, {5.0f, 267.3f}, {5.0f, 267.3f}, {2.3f, 273.9f},
                                     {3.0f, 270.5f}, {0.8f, 269.5f}, {22.2f, 56.5f}, {3.6f, 266.5f}, {4.8f, 274.5f},
                                     {4.9f, 265.9f}, {3.9f, 265.7f}, {3.3f, 265.7f}, {5.1f, 273.7f}, {4.95f, 267.5f}};
    for (int p = 1; p < 15; p++) {
        if (p == 7) {
            continue;
        }
        float best = 99.0f, bx = 0, bz = 0;
        for (int i = 0; i < GX; i++) {
            for (int k = 0; k < GZ; k++) {
                if (!seen[i][k]) {
                    continue;
                }
                const float x = X0 + (i + 0.5f) * CELL, z = Z0 + (k + 0.5f) * CELL;
                const float d = sqrtf((x - pts[p][0]) * (x - pts[p][0]) + (z - pts[p][1]) * (z - pts[p][1]));
                if (d < best) {
                    best = d;
                    bx = x;
                    bz = z;
                }
            }
        }
        printf("point %2d (%.2f, %.2f): closest reachable %.2f m at (%.2f, %.2f)\n", p, pts[p][0], pts[p][1], best, bx,
               bz);
    }

    int high = 0;
    for (int i = 0; i < GX; i++) {
        for (int k = 0; k < GZ; k++) {
            if (seen[i][k] && fabsf(ys[i][k]) > 0.1f) {
                if (high++ < 12) {
                    printf("raised floor y=%.2f at (%.2f, %.2f)\n", ys[i][k], X0 + (i + 0.5f) * CELL, Z0 + (k + 0.5f) * CELL);
                }
            }
        }
    }
    printf("raised cells: %d\n", high);
    return 0;
}
