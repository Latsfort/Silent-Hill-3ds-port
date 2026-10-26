#include "sh3ds/school.h"
#include "sh3ds/maptex.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    FILE *file = fopen("romfs/game/bg/SC00FF.IPD", "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    const size_t size = (size_t)ftell(file);
    rewind(file);
    uint8_t *ipd = malloc(size);
    assert(ipd && fread(ipd, 1, size, file) == size);
    fclose(file);
    const size_t capacity = 120000;
    ShMapVertex *vertices = calloc(capacity, sizeof(*vertices));
    ShMapVertex *output = calloc(capacity, sizeof(*output));
    ShMapVertex *baseline = calloc(capacity, sizeof(*baseline));
    uint8_t *slots = calloc(capacity / 3, 1);
    assert(vertices && output && baseline && slots);
    ShMapTexSet textures;
    sh_maptex_reset(&textures);
    ShMapBuilder builder;
    sh_map_builder_init(&builder, vertices, slots, capacity, sh_maptex_resolve, &textures);
    sh_map_add_chunk(&builder, ipd, size, NULL, 0);
    assert(builder.count > 0 && !builder.overflow);
    const size_t static_count = builder.count;
    int count;
    const ShSchoolProp *props = sh_school_props(&count);
    assert(count == 4);
    count = 3; /* Boiler models belong to SC00FF; the map belongs to the lobby chunk. */
    size_t first[3], length[3];
    for (int i = 0; i < count; i++) {
        first[i] = builder.count;
        assert(sh_map_add_named_model(&builder, ipd, size, NULL, 0, props[i].model, props[i].pos, props[i].rotation));
        length[i] = builder.count - first[i];
        assert(length[i] && !builder.overflow && length[i] % 3 == 0);
        printf("%s: %zu triangles\n", props[i].model, length[i] / 3);
    }
    sh_maptex_measure(&textures, vertices, slots, builder.count);
    const int pages = sh_maptex_pack(&textures, SH_MAPTEX_MAX_PAGES);
    ShMapRange static_ranges[SH_MAPTEX_MAX_PAGES + 1];
    const int static_ranges_count = sh_maptex_finalize(&textures, pages, vertices, slots, static_count, baseline, static_ranges);
    ShMapTexLayout layout;
    sh_maptex_layout(&textures, pages, slots, static_count, &layout);
    /* The incremental window emitter and the full emitter must preserve static geometry identically. */
    for (size_t first_tri = 0; first_tri < static_count / 3; first_tri += 7) {
        const size_t remaining = static_count / 3 - first_tri;
        sh_maptex_emit(&textures, &layout, vertices, slots, first_tri, remaining < 7 ? remaining : 7, output);
    }
    assert(layout.range_count == static_ranges_count);
    assert(memcmp(baseline, output, static_count * sizeof(*output)) == 0);
    for (int r = 0; r < static_ranges_count; r++) {
        assert(static_ranges[r].visibility_mask == 0);
    }
    size_t total = static_count;
    for (int i = 0; i < count; i++) {
        ShMapRange ranges[SH_MAPTEX_MAX_PAGES + 1];
        const int range_count = sh_maptex_finalize(&textures, pages, vertices + first[i], slots + first[i] / 3,
                                                   length[i], output + total, ranges);
        size_t covered = 0;
        for (int r = 0; r < range_count; r++) {
            assert(ranges[r].first + ranges[r].count <= length[i] && ranges[r].visibility_mask == 0);
            covered += ranges[r].count;
        }
        assert(covered == length[i]);
        total += covered;
    }
    assert(total == builder.count && memcmp(baseline, output, static_count * sizeof(*output)) == 0);
    puts("PASS: retail boiler models resolve; independent object atlas ranges preserve static geometry and cover every object triangle");
    free(ipd);
    free(vertices);
    free(output);
    free(baseline);
    free(slots);
    return 0;
}
