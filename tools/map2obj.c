/* Host-side check of source/game/map.c: builds one area and writes an OBJ. */
#include "sh3ds/map.h"

#include <dirent.h>
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
    if (argc != 4) {
        fprintf(stderr, "usage: map2obj <BG dir> <area prefix> <out.obj>\n");
        return 1;
    }

    char path[512];
    size_t glb_size = 0;
    snprintf(path, sizeof(path), "%s/%s_GLB.PLM", argv[1], argv[2]);
    uint8_t *glb = read_file(path, &glb_size);

    static ShMapVertex verts[2000000];
    ShMapBuilder b;
    sh_map_builder_init(&b, verts, NULL, sizeof(verts) / sizeof(verts[0]), NULL, NULL);

    const size_t prefix_len = strlen(argv[2]);
    size_t ipd_bytes = 0;
    DIR *dir = opendir(argv[1]);
    struct dirent *e;
    while ((e = readdir(dir)) != NULL) {
        const size_t len = strlen(e->d_name);
        if (len != prefix_len + 8 || strncmp(e->d_name, argv[2], prefix_len) != 0 ||
            strcmp(e->d_name + len - 4, ".IPD") != 0) {
            continue;
        }
        size_t size = 0;
        snprintf(path, sizeof(path), "%s/%s", argv[1], e->d_name);
        uint8_t *ipd = read_file(path, &size);
        if (!sh_map_add_chunk(&b, ipd, size, glb, glb_size)) {
            fprintf(stderr, "rejected %s\n", e->d_name);
        }
        ipd_bytes += size;
        free(ipd);
    }
    closedir(dir);

    printf("%s: chunks=%u instances=%u missing=%u triangles=%zu ipd=%zuKB glb=%zuKB overflow=%d\n",
           argv[2], b.chunks, b.instances, b.missing_models, b.count / 3, ipd_bytes / 1024,
           glb_size / 1024, b.overflow);
    printf("bounds: x[%.1f %.1f] y[%.1f %.1f] z[%.1f %.1f]\n",
           b.min[0], b.max[0], b.min[1], b.max[1], b.min[2], b.max[2]);

    FILE *out = fopen(argv[3], "w");
    for (size_t i = 0; i < b.count; i++) {
        fprintf(out, "v %.3f %.3f %.3f %u\n", verts[i].pos[0], verts[i].pos[1], verts[i].pos[2],
                verts[i].color[0]);
    }
    for (size_t i = 0; i < b.count; i += 3) {
        fprintf(out, "f %zu %zu %zu\n", i + 1, i + 2, i + 3);
    }
    fclose(out);
    return 0;
}
