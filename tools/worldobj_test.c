/* Checks that a WorldObjects_Add model resolves from an LM library and reports its triangles/textures.
 *   gcc -O1 -std=c11 -Iinclude tools/worldobj_test.c source/game/map.c -lm
 *   worldobj_test <chunk.IPD> <library.PLM> <MODEL_NAME>
 */
#include "sh3ds/map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *load(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(*size);
    fread(b, 1, *size, f);
    fclose(f);
    return b;
}

static unsigned resolve(void *user, const char name[8], unsigned clut_delta)
{
    (void)user;
    printf("  material %.8s clut_delta=%u\n", name, clut_delta);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 4) return 1;
    size_t isz = 0, gsz = 0;
    uint8_t *ipd = load(argv[1], &isz), *glb = load(argv[2], &gsz);
    if (!ipd || !glb) return 1;
    static ShMapVertex verts[30000];
    static uint8_t slots[10000];
    ShMapBuilder b;
    sh_map_builder_init(&b, verts, slots, 30000, resolve, NULL);
    const float pos[3] = {5.1f, -1.0f, 273.7f}, rot[3] = {0.0f, 51.9f, 0.0f};
    const bool ok = sh_map_add_named_model(&b, ipd, isz, glb, gsz, argv[3], pos, rot);
    printf("%s: found=%d verts=%zu\n", argv[3], ok, b.count);
    float mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};
    for (size_t i = 0; i < b.count; i++)
        for (int k = 0; k < 3; k++) {
            if (verts[i].pos[k] < mn[k]) mn[k] = verts[i].pos[k];
            if (verts[i].pos[k] > mx[k]) mx[k] = verts[i].pos[k];
        }
    if (b.count) printf("bbox 3DS (%.3f %.3f %.3f)..(%.3f %.3f %.3f)\n", mn[0], mn[1], mn[2], mx[0], mx[1], mx[2]);
    return ok ? 0 : 2;
}
