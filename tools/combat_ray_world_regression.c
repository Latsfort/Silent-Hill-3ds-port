#include "sh3ds/combat_ray.h"
#include "sh3ds/coll.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *add(ShCollWorld *world, const char *path)
{
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    size_t size = (size_t)ftell(file);
    rewind(file);
    uint8_t *bytes = malloc(size);
    assert(bytes && fread(bytes, 1, size, file) == size);
    fclose(file);
    assert(sh_coll_world_add(world, bytes, size));
    return bytes;
}

static void word(uint8_t *p, int value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)((unsigned)value >> 8);
}

static void fixtures(void)
{
    ShCollWorld world = {0};
    uint8_t vertices[12] = {0}, edge[10] = {0}, surface[12] = {0};
    uint8_t prop[10] = {0}, ranges[8] = {0}, indices[1] = {0};
    word(vertices, -2 * 256); word(vertices + 2, -2 * 256); word(vertices + 4, 5 * 256);
    word(vertices + 6, 2 * 256); word(vertices + 8, -2 * 256); word(vertices + 10, 5 * 256);
    edge[6] = 0; edge[7] = 1; edge[8] = 0; edge[9] = 255;
    surface[6] = 1;
    word(ranges + 4, 1);
    world.count = 1;
    world.chunks[0] = (ShCollChunk){.pos_x_q8 = -4 * 256, .cell_q8 = 16 * 256,
        .nx = 1, .nz = 1, .surf_n = 1, .sub_n = 1, .split_n = 2, .n28 = 1,
        .surfaces = surface, .subcells = edge, .split_vertices = vertices,
        .ptr18 = prop, .ranges = ranges, .ptr28 = indices};
    /* Raw vertices are chunk-relative. */
    word(vertices, 2 * 256); word(vertices + 6, 6 * 256);
    ShPathPoint origin = {0, -4096, 0}, delta = {0, 0, 10 * 4096};
    ShRayTrace trace;
    assert(sh_combat_ray_query(&trace, &world, &origin, &delta, NULL, 0, false, 1));
    assert(!trace.character && trace.hitDistance == 5 * 4096 && trace.target.vz == 5 * 4096);
    assert(!sh_combat_ray_query(&trace, &world, &origin, &delta, NULL, 0, false, 0));
    origin.vy = -3 * 4096;
    assert(!sh_combat_ray_query(&trace, &world, &origin, &delta, NULL, 0, false, 1));
    origin.vy = -4096;
    surface[6] = 0;
    assert(!sh_combat_ray_query(&trace, &world, &origin, &delta, NULL, 0, false, 1));
    surface[6] = 1;
    world.chunks[0].sub_n = 0; world.chunks[0].p18_n = 1;
    prop[0] = 1;
    word(prop + 2, 4 * 256); word(prop + 4, -2 * 256); word(prop + 6, 5 * 256); word(prop + 8, 256);
    assert(sh_combat_ray_query(&trace, &world, &origin, &delta, NULL, 0, false, 1));
    assert(trace.hitDistance == 4 * 4096 && trace.target.vz >= 4 * 4096 - 16 && trace.target.vz <= 4 * 4096);
    prop[1] = 3;
    assert(!sh_combat_ray_query(&trace, &world, &origin, &delta, NULL, 0, false, 1));
    assert(sh_combat_ray_query(&trace, &world, &origin, &delta, NULL, 0, false, 8));
    ShRayCharacter target = {0};
    target.position.vz = 7 * 4096;
    target.collision.cylinder.field_2 = 4096;
    target.collision.box.top = -2 * 4096;
    ShRayCharacter *characters[] = {&target};
    assert(sh_combat_ray_query(&trace, &world, &origin, &delta, characters, 1, false, 8));
    assert(!trace.character); /* Real prop intersection wins over the farther character. */
    target.position.vz = 2 * 4096;
    assert(sh_combat_ray_query(&trace, &world, &origin, &delta, characters, 1, false, 8));
    assert(trace.character == &target);
}

int main(void)
{
    fixtures();
    ShCollWorld world = {0};
    uint8_t *a = add(&world, "romfs/game/bg/SU0000.IPD");
    uint8_t *b = add(&world, "romfs/game/bg/SU00FF.IPD");
    unsigned hits = 0, cases = 0;
    for (int x = 4; x <= 28; x += 2) {
        for (int z = -28; z <= 28; z += 2) {
            for (int angle = 0; angle < 8; ++angle) {
                static const int dx[] = {1, 1, 0, -1, -1, -1, 0, 1};
                static const int dz[] = {0, 1, 1, 1, 0, -1, -1, -1};
                ShPathPoint origin = {x * 4096, -4096, z * 4096};
                ShPathPoint delta = {dx[angle] * 12 * 4096, 0, dz[angle] * 12 * 4096};
                ShRayTrace trace;
                bool hit = sh_combat_ray_query(&trace, &world, &origin, &delta, NULL, 0, true, -1);
                hits += hit;
                assert(!trace.character);
                assert(!sh_combat_ray_query(&trace, &world, &origin, &delta, NULL, 0, true, 0));
                cases++;
            }
        }
    }
    assert(hits > 0);
    printf("PASS: wall/prop/filter/height/character fixtures and %u real SU map rays (%u hits)\n", cases, hits);
    free(a); free(b);
    return 0;
}
