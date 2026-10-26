#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Builds a flat triangle list from Silent Hill map chunks (.IPD) and the
 * area's shared model library (*_GLB.PLM). Formats follow the decomp headers
 * include/bodyprog/formats/{ipd,lm,model}.h. Output is in meters, in a
 * right-handed Y-up space: (x, -y, -z) of the PS1 coordinates.
 */

#define SH_MAP_NO_TEXTURE 0xFF

typedef struct ShMapVertex {
    float pos[3];
    float uv[2];      /* builder output: texels in the material's TIM; GPU: normalized atlas coords */
    uint8_t color[4]; /* r, g, b, a */
} ShMapVertex;

/*
 * Returns the texture slot for a material (TIM name, up to 8 chars, not
 * NUL-terminated) and PS1 CLUT id delta, or SH_MAP_NO_TEXTURE.
 */
typedef unsigned (*ShMapTextureResolver)(void *user, const char name[8], unsigned clut_delta);

typedef struct ShMapBuilder {
    ShMapVertex *verts;
    uint8_t *tri_slots; /* optional, one texture slot per emitted triangle */
    size_t count;       /* vertices */
    size_t capacity;    /* vertices */
    ShMapTextureResolver resolve;
    void *resolve_user;
    float min[3];
    float max[3];
    unsigned chunks;
    unsigned instances;
    unsigned missing_models;
    bool overflow;
} ShMapBuilder;

void sh_map_builder_init(ShMapBuilder *b, ShMapVertex *storage, uint8_t *tri_slots, size_t capacity,
                         ShMapTextureResolver resolve, void *resolve_user);

/* glb may be NULL when the area has no shared library. */
bool sh_map_add_chunk(ShMapBuilder *b, const uint8_t *ipd, size_t ipd_size,
                      const uint8_t *glb, size_t glb_size);

/*
 * World object (bodyprog WorldObjects_Add): draws LM model `name` (8 chars, space/NUL
 * padded) from the chunk's local library (or the GLB) at a pose given in PS1 units —
 * position in meters (PS1 axes), rotation in degrees (X, Y, Z). Returns false if the
 * model is not in this chunk.
 */
bool sh_map_add_named_model(ShMapBuilder *b, const uint8_t *ipd, size_t ipd_size, const uint8_t *glb,
                            size_t glb_size, const char *name, const float pos_ps1[3], const float rot_deg[3]);

#ifdef __cplusplus
}
#endif
