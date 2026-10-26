#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sh3ds/map.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Map textures are 4-bit TIMs with many palettes; each (TIM, palette) pair
 * only colors a small region. Pairs used by the current chunk window are
 * cropped to their UV bounds and packed into square atlas pages.
 */

#define SH_ATLAS_SIZE 1024
#define SH_MAPTEX_MAX_PAIRS 254 /* slot ids are u8, SH_MAP_NO_TEXTURE is reserved */
#define SH_MAPTEX_MAX_PAGES 4
#define SH_MAP_MAX_OBJECTS 32
#define SH_MAP_MAX_RANGES ((SH_MAP_MAX_OBJECTS + 1) * (SH_MAPTEX_MAX_PAGES + 1))

typedef struct ShMapTexPair {
    char name[8]; /* TIM name, not NUL-terminated when 8 chars long */
    unsigned clut_delta;
    int min_u, min_v, max_u, max_v; /* inclusive texel rect used by the geometry */
    int page;                       /* -1 when it did not fit */
    int x, y;                       /* position in the page */
} ShMapTexPair;

typedef struct ShMapTexSet {
    ShMapTexPair pairs[SH_MAPTEX_MAX_PAIRS];
    int count;
} ShMapTexSet;

typedef struct ShMapRange {
    int page; /* -1 for untextured geometry */
    size_t first;
    size_t count;
    uint32_t visibility_mask; /* zero: static geometry; otherwise WorldObjects_Add selection */
} ShMapRange;

void sh_maptex_reset(ShMapTexSet *set);

/* ShMapTextureResolver registering pairs in a ShMapTexSet passed as `user`. */
unsigned sh_maptex_resolve(void *user, const char name[8], unsigned clut_delta);

/* Computes each pair's UV rect from the builder output. */
void sh_maptex_measure(ShMapTexSet *set, const ShMapVertex *verts, const uint8_t *tri_slots, size_t vert_count);

/* Shelf-packs pair rects into up to `page_count` pages; returns the number of pages used. */
int sh_maptex_pack(ShMapTexSet *set, int page_count);

/* Where each atlas page's triangles go in the final buffer; lets the copy run in slices. */
typedef struct ShMapTexLayout {
    int page_count;
    size_t cursor[SH_MAPTEX_MAX_PAGES + 1]; /* last bucket is untextured */
    ShMapRange ranges[SH_MAPTEX_MAX_PAGES + 1];
    int range_count;
} ShMapTexLayout;

void sh_maptex_layout(const ShMapTexSet *set, int page_count, const uint8_t *tri_slots, size_t vert_count,
                      ShMapTexLayout *layout);

/* Copies triangles [first_tri, first_tri + tri_count) of `src` into `dst` with normalized atlas UVs. */
void sh_maptex_emit(const ShMapTexSet *set, ShMapTexLayout *layout, const ShMapVertex *src,
                    const uint8_t *tri_slots, size_t first_tri, size_t tri_count, ShMapVertex *dst);

/*
 * Layout + emit in one go. `ranges` needs room for page_count + 1 entries;
 * returns the number written.
 */
int sh_maptex_finalize(const ShMapTexSet *set, int page_count, const ShMapVertex *src, const uint8_t *tri_slots,
                       size_t vert_count, ShMapVertex *dst, ShMapRange *ranges);

#ifdef __cplusplus
}
#endif
