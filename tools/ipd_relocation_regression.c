#include "sh3ds/ipd.h"
#include "sh3ds/map.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *load(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    const long length = ftell(file);
    assert(length > 0);
    rewind(file);
    *size = (size_t)length;
    uint8_t *data = malloc(*size);
    assert(data && fread(data, 1, *size, file) == *size);
    assert(fclose(file) == 0);
    return data;
}

static int32_t read_s32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

int main(void)
{
    size_t size, glb_size;
    uint8_t *data = load("dump/assets/BG/THRF908.IPD", &size);
    uint8_t *glb = load("dump/assets/BG/THR_GLB.PLM", &glb_size);
    uint8_t *original = malloc(size);
    assert(original);
    memcpy(original, data, size);
    assert((int8_t)data[2] == -7 && data[3] == 8);
    assert(read_s32(data + 0x54) == -7 * 40 * 256);
    assert(read_s32(data + 0x58) == 8 * 40 * 256);

    const size_t capacity = 100000;
    ShMapVertex *before = calloc(capacity, sizeof(*before));
    ShMapVertex *after = calloc(capacity, sizeof(*after));
    assert(before && after);
    ShMapBuilder a, b;
    sh_map_builder_init(&a, before, NULL, capacity, NULL, NULL);
    assert(sh_map_add_chunk(&a, data, size, glb, glb_size));
    assert(a.count && !a.overflow && !a.missing_models);

    /* MapEvent_CutsceneAlleyNightmare: THRF908 replaces logical chunk (-7, 6). */
    assert(sh_ipd_relocate(data, size, -7, 6));
    assert(read_s32(data + 0x54) == -7 * 40 * 256);
    assert(read_s32(data + 0x58) == 6 * 40 * 256);
    sh_map_builder_init(&b, after, NULL, capacity, NULL, NULL);
    assert(sh_map_add_chunk(&b, data, size, glb, glb_size));
    assert(!b.overflow && b.count == a.count && b.instances == a.instances);
    for (size_t i = 0; i < a.count; i++) {
        assert(before[i].pos[0] == after[i].pos[0]);
        assert(before[i].pos[1] == after[i].pos[1]);
        assert(fabsf(after[i].pos[2] - before[i].pos[2] - 80.0f) < 0.0001f);
        assert(memcmp(before[i].uv, after[i].uv, sizeof(before[i].uv)) == 0);
        assert(memcmp(before[i].color, after[i].color, sizeof(before[i].color)) == 0);
    }
    uint8_t *relocated = malloc(size);
    assert(relocated);
    memcpy(relocated, data, size);
    assert(sh_ipd_relocate(data, size, -7, 6));
    assert(memcmp(data, relocated, size) == 0);
    for (size_t n = 0; n < 0x88; n++) {
        assert(!sh_ipd_relocate(data, n, -7, 8));
        assert(memcmp(data, relocated, size) == 0);
    }
    assert(!sh_ipd_relocate(NULL, size, 0, 0));
    assert(!sh_ipd_relocate(data, size, -129, 0));
    assert(!sh_ipd_relocate(data, size, 0, 128));
    assert(memcmp(data, relocated, size) == 0);
    assert(sh_ipd_relocate(data, size, -6, 5));
    assert(read_s32(data + 0x54) == -6 * 40 * 256);
    assert(read_s32(data + 0x58) == 5 * 40 * 256);
    assert(sh_ipd_relocate(data, size, -7, 8));
    assert(memcmp(data, original, size) == 0);
    printf("THRF908 relocation: %lu vertices, collision origins, round trip and invalid inputs passed\n",
           (unsigned long)a.count);
    free(relocated);
    free(before);
    free(after);
    free(original);
    free(data);
    free(glb);
    return 0;
}
