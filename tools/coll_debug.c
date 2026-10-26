/* coll_debug <BG> <area> <cx> <cz> — print walls near spawn + height grid + walk tests */
#include "sh3ds/coll.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p) { return p[0]|p[1]<<8|p[2]<<16|p[3]<<24; }
static int16_t s16(const uint8_t *p) { return (int16_t)(p[0]|p[1]<<8); }

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(*size);
    fread(d, 1, *size, f);
    fclose(f);
    return d;
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: coll_debug <BG> <area> <cx> <cz> [dir dx dz steps]\n");
        return 1;
    }
    const char *bg = argv[1], *area = argv[2];
    int cx = atoi(argv[3]), cz = atoi(argv[4]);
    float ddx = argc > 6 ? (float)atof(argv[5]) : 1.0f;
    float ddz = argc > 6 ? (float)atof(argv[6]) : 0.0f;
    int steps = argc > 7 ? atoi(argv[7]) : 80;

    ShCollWorld world;
    sh_coll_world_reset(&world);
    char path[512];
    uint8_t *keep[9] = {0};
    int ki = 0;
    for (int dz = -1; dz <= 1; dz++) {
        for (int dx = -1; dx <= 1; dx++) {
            size_t size = 0;
            snprintf(path, sizeof(path), "%s/%s%02X%02X.IPD", bg, area, (cx + dx) & 0xFF, (cz + dz) & 0xFF);
            uint8_t *ipd = read_file(path, &size);
            if (ipd && sh_coll_world_add(&world, ipd, size)) {
                keep[ki++] = ipd;
                /* List one-sided walls near chunk center */
                const uint8_t *c = ipd + 0x54;
                unsigned split_n = u32(c+8) & 0xFF;
                unsigned sub_n = (u32(c+8) >> 16) & 0xFF;
                int32_t posx = (int32_t)u32(c), posz = (int32_t)u32(c+4);
                const uint8_t *splits = c + u32(c+0xC);
                const uint8_t *subs = c + u32(c+0x14);
                int walls = 0;
                for (unsigned i = 0; i < sub_n; i++) {
                    const uint8_t *sub = subs + i*10;
                    if (sub[8] != 0xFF && sub[9] != 0xFF) continue;
                    walls++;
                    unsigned v0i = sub[7], v1i = sub[6];
                    if (v0i >= split_n || v1i >= split_n) continue;
                    float ax = (posx + s16(splits+v0i*6))/256.f;
                    float az = (posz + s16(splits+v0i*6+4))/256.f;
                    float bx = (posx + s16(splits+v1i*6))/256.f;
                    float bz = (posz + s16(splits+v1i*6+4))/256.f;
                    float ay = -s16(splits+v0i*6+2)/256.f;
                    float by = -s16(splits+v1i*6+2)/256.f;
                    /* 3DS z = -ps1 z */
                    if (walls <= 8)
                        printf("  wall %s: (%.1f,%.1f)-(%.1f,%.1f) topY=%.2f/%.2f\n",
                               path + strlen(bg)+1, ax, -az, bx, -bz, ay, by);
                }
                printf("%s: %d one-sided walls\n", path + strlen(bg)+1, walls);
            }
        }
    }

    float x = (cx + 0.5f) * 40.0f, z = -(cz + 0.5f) * 40.0f;
    ShCollHit hit;
    sh_coll_ground(&world, x, z, &hit);
    float y = hit.height + 1.6f;
    printf("spawn (%.1f, %.2f, %.1f) ground=%d\n", x, hit.height, z, hit.valid);

    printf("height grid (3DS xz around spawn):\n");
    for (int iz = -4; iz <= 4; iz++) {
        for (int ix = -4; ix <= 4; ix++) {
            float gx = x + ix * 2.0f, gz = z + iz * 2.0f;
            if (!sh_coll_ground(&world, gx, gz, &hit)) printf("  .  ");
            else printf("%5.1f", hit.height);
        }
        printf("\n");
    }

    float len = sqrtf(ddx*ddx + ddz*ddz);
    if (len < 1e-3f) { ddx = 1; len = 1; }
    ddx /= len; ddz /= len;
    printf("walk dir (%.2f,%.2f) %d steps of 0.25m:\n", ddx, ddz, steps);
    for (int i = 0; i < steps; i++) {
        float ox = x, oz = z, oy = y;
        sh_coll_move(&world, &x, &y, &z, ddx * 0.25f, ddz * 0.25f, SH_COLL_PLAYER_RADIUS, 1.6f);
        if (fabsf(x-ox)+fabsf(z-oz) < 0.01f) {
            printf("  blocked at step %d pos=(%.2f, %.2f, %.2f) ground=%.2f\n", i, x, y-1.6f, z, y-1.6f);
            break;
        }
        if (i % 10 == 9 || fabsf((y-1.6f) - (oy-1.6f)) > 0.05f)
            printf("  step %d -> (%.2f, %.2f, %.2f)\n", i+1, x, y-1.6f, z);
    }
    printf("end (%.2f, %.2f, %.2f)\n", x, y-1.6f, z);
    return 0;
}
