/* usage: coll_probe <BG dir> <area> <cx> <cz> */
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

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: coll_probe <BG> <area> <cx> <cz>\n");
        return 1;
    }
    const char *bg = argv[1], *area = argv[2];
    const int cx = atoi(argv[3]), cz = atoi(argv[4]);
    ShCollWorld world;
    sh_coll_world_reset(&world);

    char path[512];
    for (int dz = -1; dz <= 1; dz++) {
        for (int dx = -1; dx <= 1; dx++) {
            size_t size = 0;
            snprintf(path, sizeof(path), "%s/%s%02X%02X.IPD", bg, area, (cx + dx) & 0xFF, (cz + dz) & 0xFF);
            uint8_t *ipd = read_file(path, &size);
            if (ipd) {
                sh_coll_world_add(&world, ipd, size);
            }
        }
    }

    const float x0 = (cx + 0.5f) * 40.0f;
    const float z0 = -(cz + 0.5f) * 40.0f;
    ShCollHit hit;
    sh_coll_ground(&world, x0, z0, &hit);
    printf("center (%.1f, %.1f): h=%.3f type=%d chunks=%d\n", x0, z0, hit.height, hit.ground_type, world.count);

    /* Walk +X until blocked (expect a building wall). */
    float x = x0, y = hit.height + 1.6f, z = z0;
    int moved = 0, blocked = 0;
    for (int i = 0; i < 400; i++) {
        const float ox = x, oz = z;
        sh_coll_move(&world, &x, &y, &z, 0.15f, 0.0f, SH_COLL_PLAYER_RADIUS, 1.6f);
        if (fabsf(x - ox) < 0.001f && fabsf(z - oz) < 0.001f) {
            blocked++;
        } else {
            moved++;
            blocked = 0;
        }
        if (blocked > 5) {
            break;
        }
    }
    printf("walk +X: moved=%d stop=(%.2f, %.2f, %.2f) ground=%.2f\n", moved, x, y, z, y - 1.6f);

    /* Walk -Z (toward +PS1 Z / into chunk). */
    x = x0;
    y = hit.height + 1.6f;
    z = z0;
    moved = blocked = 0;
    for (int i = 0; i < 400; i++) {
        const float ox = x, oz = z;
        sh_coll_move(&world, &x, &y, &z, 0.0f, -0.15f, SH_COLL_PLAYER_RADIUS, 1.6f);
        if (fabsf(x - ox) < 0.001f && fabsf(z - oz) < 0.001f) {
            blocked++;
        } else {
            moved++;
            blocked = 0;
        }
        if (blocked > 5) {
            break;
        }
    }
    printf("walk -Z: moved=%d stop=(%.2f, %.2f, %.2f) ground=%.2f\n", moved, x, y, z, y - 1.6f);

    /* Sample a known outdoor wall from THR0000: edge x=26.5 from z=14..40 in PS1 -> 3DS z=-14..-40 */
    if (strcmp(area, "THR") == 0 && cx == 0 && cz == 0) {
        float wx = 26.5f, wz = -20.0f;
        sh_coll_ground(&world, wx - 1.0f, wz, &hit);
        float px = wx - 1.0f, py = hit.height + 1.6f, pz = wz;
        for (int i = 0; i < 30; i++) {
            sh_coll_move(&world, &px, &py, &pz, 0.1f, 0.0f, SH_COLL_PLAYER_RADIUS, 1.6f);
        }
        printf("toward wall x=26.5: stop x=%.2f (expect ~26.15)\n", px);
    }

    /* School: walk into the room wall at PS1 z=15.8 -> 3DS z=-15.8 */
    if (strcmp(area, "SC") == 0) {
        float px = 20.0f, pz = -20.0f;
        sh_coll_ground(&world, px, pz, &hit);
        float py = hit.height + 1.6f;
        for (int i = 0; i < 40; i++) {
            sh_coll_move(&world, &px, &py, &pz, 0.0f, 0.15f, SH_COLL_PLAYER_RADIUS, 1.6f);
        }
        printf("school toward z=-15.8 wall: stop z=%.2f (expect ~-16.15)\n", pz);
    }
    return 0;
}
