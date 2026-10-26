/* usage: coll_reach <BG dir> <area> <x0> <z0> <x1> <z1>  (3DS coords; flood fill 0.5 m grid) */
#include "sh3ds/coll.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CELL 0.5f
#define N 400

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
    if (fread(data, 1, *size, f) != *size) {
        *size = 0;
    }
    fclose(f);
    return data;
}

static unsigned char seen[N][N];
static float feet[N][N];
static int queue[N * N];

int main(int argc, char **argv)
{
    if (argc != 7) {
        fprintf(stderr, "usage: coll_reach <BG> <area> <x0> <z0> <x1> <z1>\n");
        return 1;
    }
    const float sx = (float)atof(argv[3]), sz = (float)atof(argv[4]);
    const float tx = (float)atof(argv[5]), tz = (float)atof(argv[6]);
    static ShCollWorld world;
    sh_coll_world_reset(&world);
    const int ccx = (int)floorf(sx / 40.0f), ccz = (int)floorf(-sz / 40.0f);
    char path[512];
    for (int dz = -6; dz <= 6; dz++) {
        for (int dx = -6; dx <= 6; dx++) {
            size_t size = 0;
            snprintf(path, sizeof(path), "%s/%s%02X%02X.IPD", argv[1], argv[2], (ccx + dx) & 0xFF, (ccz + dz) & 0xFF);
            uint8_t *ipd = read_file(path, &size);
            if (ipd && size) {
                sh_coll_world_add(&world, ipd, size);
            }
        }
    }
    if (getenv("CHUNKS")) {
        for (int i = 0; i < world.count; i++) {
            const ShCollChunk *c = &world.chunks[i];
            const float x0 = c->pos_x_q8 / 256.0f, z0 = c->pos_z_q8 / 256.0f, cs = c->cell_q8 / 256.0f;
            printf("chunk %d: ps1 x %.1f..%.1f z %.1f..%.1f (%dx%d cell %.1f)\n", i, x0, x0 + c->nx * cs, z0,
                   z0 + c->nz * cs, c->nx, c->nz, cs);
        }
        return 0;
    }
    if (getenv("GRID")) {
        for (float z = sz; z <= tz; z += 0.5f) {
            printf("%7.1f ", z);
            for (float x = sx; x <= tx; x += 0.5f) {
                ShCollHit h;
                printf(sh_coll_ground(&world, x, z, &h) ? "%5.2f" : "   --", h.height);
            }
            printf("\n");
        }
        return 0;
    }
    if (getenv("SCAN")) {
        for (float z = sz; z <= tz; z += 1.0f) {
            for (float x = sx; x <= tx; x += 1.0f) {
                ShCollHit h;
                if (sh_coll_ground(&world, x, z, &h) && fabsf(h.height) > 0.1f && fabsf(h.height) < 4.0f) {
                    printf("(%.0f,%.0f)=%.2f ", x, z, h.height);
                }
            }
        }
        printf("\n");
        return 0;
    }
    const float ox = sx - N * CELL / 2, oz = sz - N * CELL / 2;
    ShCollHit hit;
    sh_coll_ground_near(&world, sx, sz, getenv("Y") ? (float)atof(getenv("Y")) : 0.0f, &hit);
    int head = 0, tail = 0;
    const int si = N / 2, sj = N / 2;
    seen[si][sj] = 1;
    feet[si][sj] = hit.height;
    queue[tail++] = si * N + sj;
    float best_d = 1e9f, bx = sx, bz = sz, by = hit.height;
    static const int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    while (head < tail) {
        const int c = queue[head++], i = c / N, j = c % N;
        const float cx = ox + i * CELL, cz = oz + j * CELL;
        const float d = hypotf(cx - tx, cz - tz);
        if (d < best_d) {
            best_d = d, bx = cx, bz = cz, by = feet[i][j];
        }
        for (int k = 0; k < 8; k++) {
            const int ni = i + dirs[k][0], nj = j + dirs[k][1];
            if (ni < 0 || nj < 0 || ni >= N || nj >= N || seen[ni][nj]) {
                continue;
            }
            float x = cx, y = feet[i][j] + 1.6f, z = cz;
            const float ddx = dirs[k][0] * CELL / 5, ddz = dirs[k][1] * CELL / 5;
            for (int s = 0; s < 5; s++) {
                sh_coll_move(&world, &x, &y, &z, ddx, ddz, SH_COLL_PLAYER_RADIUS, 1.6f);
            }
            if (fabsf(x - (ox + ni * CELL)) < 0.08f && fabsf(z - (oz + nj * CELL)) < 0.08f) {
                seen[ni][nj] = 1;
                feet[ni][nj] = y - 1.6f;
                queue[tail++] = ni * N + nj;
            }
        }
    }
    printf("chunks=%d reached=%d cells; closest to target: (%.1f, %.1f) feet=%.2f dist=%.1f\n", world.count, tail, bx,
           bz, by, best_d);
    return 0;
}
