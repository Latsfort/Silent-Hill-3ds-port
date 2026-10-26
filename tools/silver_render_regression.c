#include "sh3ds/silver.h"
#include "sh3ds/map.h"

#include <assert.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const size_t capacity = 120000;
    ShMapVertex *base = calloc(capacity, sizeof(*base));
    ShMapVertex *posed = calloc(capacity, sizeof(*posed));
    ShMapVertex *transformed = calloc(capacity, sizeof(*transformed));
    assert(base && posed && transformed);
    ShSilverMotion silver;
    sh_silver_init(&silver);
    float position[3], rotation[3];
    sh_silver_pose(&silver, position, rotation);
    DIR *directory = opendir("romfs/game/bg");
    assert(directory);
    struct dirent *entry;
    uint8_t *data = NULL;
    size_t size = 0, vertex_count = 0;
    while ((entry = readdir(directory))) {
        if (strncmp(entry->d_name, "SC", 2) || !strstr(entry->d_name, ".IPD")) { continue; }
        char path[512];
        snprintf(path, sizeof(path), "romfs/game/bg/%s", entry->d_name);
        FILE *file = fopen(path, "rb");
        assert(file && !fseek(file, 0, SEEK_END));
        size = (size_t)ftell(file);
        rewind(file);
        data = malloc(size);
        assert(data && fread(data, 1, size, file) == size);
        fclose(file);
        ShMapBuilder builder;
        sh_map_builder_init(&builder, base, NULL, capacity, NULL, NULL);
        if (sh_map_add_named_model(&builder, data, size, NULL, 0, "SILVER_H", position, rotation)) {
            assert(builder.count && !builder.overflow);
            vertex_count = builder.count;
            printf("SILVER_H resolved from %s: %zu vertices\n", entry->d_name, vertex_count);
            break;
        }
        free(data);
        data = NULL;
    }
    closedir(directory);
    assert(data && vertex_count);
    float largest_error = 0.0f;
    size_t compared = 0;
    bool hit = false;
    for (int frame = 0; frame < 100; frame++) {
        if (frame < 16) { sh_silver_swing(&silver, 136); }
        else if (!hit) { hit = sh_silver_fall(&silver, 136, 1338); }
        else { sh_silver_ground(&silver); }
        sh_silver_pose(&silver, position, rotation);
        ShMapBuilder builder;
        sh_map_builder_init(&builder, posed, NULL, capacity, NULL, NULL);
        assert(sh_map_add_named_model(&builder, data, size, NULL, 0, "SILVER_H", position, rotation));
        assert(builder.count == vertex_count && !builder.overflow);
        sh_silver_transform_vertices(&silver, base, transformed, vertex_count);
        for (size_t vertex = 0; vertex < vertex_count; vertex++) {
            for (int axis = 0; axis < 3; axis++) {
                const float actual = transformed[vertex].pos[axis];
                const float error = fabsf(actual - posed[vertex].pos[axis]);
                if (error > largest_error) { largest_error = error; }
                assert(error == 0.0f);
                compared++;
            }
            assert(memcmp(transformed[vertex].uv, base[vertex].uv, sizeof(base[vertex].uv)) == 0);
            assert(memcmp(transformed[vertex].color, base[vertex].color, sizeof(base[vertex].color)) == 0);
        }
    }
    assert(hit);
    printf("PASS: %zu exact coordinate comparisons against real-model pose baking; max error %.8f metres\n", compared, largest_error);
    free(data);
    free(base);
    free(posed);
    free(transformed);
    return 0;
}
