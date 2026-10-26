/* Host-side estimate of the 3x3 chunk window size the 3DS has to draw. */
#include "sh3ds/map.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Chunk {
    int x, z;
    uint8_t *data;
    size_t size;
} Chunk;

typedef struct Pair {
    char name[8];
    unsigned clut;
} Pair;

static Pair g_pairs[1024];
static int g_pair_count;

static unsigned count_pairs(void *user, const char name[8], unsigned clut_delta)
{
    (void)user;
    for (int i = 0; i < g_pair_count; i++) {
        if (memcmp(g_pairs[i].name, name, 8) == 0 && g_pairs[i].clut == clut_delta) {
            return 0;
        }
    }
    if (g_pair_count < 1024) {
        memcpy(g_pairs[g_pair_count].name, name, 8);
        g_pairs[g_pair_count++].clut = clut_delta;
    }
    return 0;
}

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
    char path[512];
    static Chunk chunks[256];
    static ShMapVertex verts[90000 * 3];
    int count = 0;
    size_t glb_size = 0;

    snprintf(path, sizeof(path), "%s/%s_GLB.PLM", argv[1], argv[2]);
    uint8_t *glb = read_file(path, &glb_size);

    const size_t prefix_len = strlen(argv[2]);
    DIR *dir = opendir(argv[1]);
    struct dirent *e;
    while ((e = readdir(dir)) != NULL && count < 256) {
        const size_t len = strlen(e->d_name);
        if (len != prefix_len + 8 || strncmp(e->d_name, argv[2], prefix_len) != 0 ||
            strcmp(e->d_name + len - 4, ".IPD") != 0) {
            continue;
        }
        char hex[3] = {0};
        memcpy(hex, e->d_name + prefix_len, 2);
        chunks[count].x = (int8_t)strtol(hex, NULL, 16);
        memcpy(hex, e->d_name + prefix_len + 2, 2);
        chunks[count].z = (int8_t)strtol(hex, NULL, 16);
        snprintf(path, sizeof(path), "%s/%s", argv[1], e->d_name);
        chunks[count].data = read_file(path, &chunks[count].size);
        count++;
    }
    closedir(dir);

    size_t worst = 0;
    int worst_x = 0, worst_z = 0, overflows = 0, worst_pairs = 0, center_pairs = 0;
    for (int c = 0; c < count; c++) {
        ShMapBuilder b;
        g_pair_count = 0;
        sh_map_builder_init(&b, verts, NULL, sizeof(verts) / sizeof(verts[0]), NULL, NULL);
        b.resolve = count_pairs;
        sh_map_add_chunk(&b, chunks[c].data, chunks[c].size, glb, glb_size);
        if (g_pair_count > center_pairs) {
            center_pairs = g_pair_count;
        }
        g_pair_count = 0;
        sh_map_builder_init(&b, verts, NULL, sizeof(verts) / sizeof(verts[0]), count_pairs, NULL);
        for (int i = 0; i < count; i++) {
            if (abs(chunks[i].x - chunks[c].x) <= 1 && abs(chunks[i].z - chunks[c].z) <= 1) {
                sh_map_add_chunk(&b, chunks[i].data, chunks[i].size, glb, glb_size);
            }
        }
        overflows += b.overflow;
        if (b.count / 3 > worst) {
            worst = b.count / 3;
            worst_x = chunks[c].x;
            worst_z = chunks[c].z;
        }
        if (g_pair_count > worst_pairs) {
            worst_pairs = g_pair_count;
        }
    }
    printf("%-4s worst 3x3 window: %zu triangles at (%d,%d), overflows=%d, texture pairs: 3x3 max %d, 1 chunk max %d\n",
           argv[2], worst, worst_x, worst_z, overflows, worst_pairs, center_pairs);
    return 0;
}
