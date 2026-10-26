#include "sh3ds/maptex.h"

#include <string.h>

void sh_maptex_reset(ShMapTexSet *set)
{
    set->count = 0;
}

unsigned sh_maptex_resolve(void *user, const char name[8], unsigned clut_delta)
{
    ShMapTexSet *set = user;

    for (int i = 0; i < set->count; i++) {
        if (set->pairs[i].clut_delta == clut_delta && memcmp(set->pairs[i].name, name, 8) == 0) {
            return (unsigned)i;
        }
    }
    if (set->count >= SH_MAPTEX_MAX_PAIRS) {
        return SH_MAP_NO_TEXTURE;
    }

    ShMapTexPair *pair = &set->pairs[set->count];
    memcpy(pair->name, name, 8);
    pair->clut_delta = clut_delta;
    pair->min_u = pair->min_v = 255;
    pair->max_u = pair->max_v = 0;
    pair->page = -1;
    pair->x = pair->y = 0;
    return (unsigned)set->count++;
}

void sh_maptex_measure(ShMapTexSet *set, const ShMapVertex *verts, const uint8_t *tri_slots, size_t vert_count)
{
    for (size_t t = 0; t < vert_count / 3; t++) {
        const unsigned slot = tri_slots[t];
        if (slot >= (unsigned)set->count) {
            continue;
        }
        ShMapTexPair *pair = &set->pairs[slot];
        for (int k = 0; k < 3; k++) {
            const int u = (int)verts[t * 3 + k].uv[0];
            const int v = (int)verts[t * 3 + k].uv[1];
            if (u < pair->min_u) {
                pair->min_u = u;
            }
            if (u > pair->max_u) {
                pair->max_u = u;
            }
            if (v < pair->min_v) {
                pair->min_v = v;
            }
            if (v > pair->max_v) {
                pair->max_v = v;
            }
        }
    }
}

int sh_maptex_pack(ShMapTexSet *set, int page_count)
{
    int order[SH_MAPTEX_MAX_PAIRS];

    for (int i = 0; i < set->count; i++) {
        set->pairs[i].page = -1;
        order[i] = i;
    }

    /* Tallest first keeps shelves tight. */
    for (int i = 1; i < set->count; i++) {
        const int idx = order[i];
        const int h = set->pairs[idx].max_v - set->pairs[idx].min_v;
        int j = i - 1;
        while (j >= 0 && set->pairs[order[j]].max_v - set->pairs[order[j]].min_v < h) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = idx;
    }

    int page = 0, shelf_y = 0, shelf_h = 0, cursor_x = 0, used = 0;
    for (int i = 0; i < set->count && page < page_count; i++) {
        ShMapTexPair *pair = &set->pairs[order[i]];
        if (pair->max_u < pair->min_u) {
            continue; /* registered but never referenced by a triangle */
        }
        const int w = pair->max_u - pair->min_u + 1;
        const int h = pair->max_v - pair->min_v + 1;

        if (cursor_x + w > SH_ATLAS_SIZE) {
            shelf_y += shelf_h;
            cursor_x = 0;
            shelf_h = 0;
        }
        if (shelf_y + h > SH_ATLAS_SIZE) {
            page++;
            shelf_y = 0;
            cursor_x = 0;
            shelf_h = 0;
            if (page >= page_count) {
                break;
            }
        }

        pair->page = page;
        pair->x = cursor_x;
        pair->y = shelf_y;
        cursor_x += w;
        if (h > shelf_h) {
            shelf_h = h;
        }
        used = page + 1;
    }
    return used;
}

static int bucket_of(const ShMapTexSet *set, int page_count, unsigned slot)
{
    if (slot < (unsigned)set->count && set->pairs[slot].page >= 0) {
        return set->pairs[slot].page;
    }
    return page_count; /* untextured bucket */
}

void sh_maptex_layout(const ShMapTexSet *set, int page_count, const uint8_t *tri_slots, size_t vert_count,
                      ShMapTexLayout *layout)
{
    size_t counts[SH_MAPTEX_MAX_PAGES + 1] = {0};
    const size_t tri_count = vert_count / 3;

    if (page_count > SH_MAPTEX_MAX_PAGES) {
        page_count = SH_MAPTEX_MAX_PAGES;
    }
    for (size_t t = 0; t < tri_count; t++) {
        counts[bucket_of(set, page_count, tri_slots[t])] += 3;
    }

    layout->page_count = page_count;
    layout->range_count = 0;
    size_t first = 0;
    for (int b = 0; b <= page_count; b++) {
        layout->cursor[b] = first;
        if (counts[b] > 0) {
            ShMapRange *range = &layout->ranges[layout->range_count++];
            range->page = b < page_count ? b : -1;
            range->first = first;
            range->count = counts[b];
            range->visibility_mask = 0;
        }
        first += counts[b];
    }
}

void sh_maptex_emit(const ShMapTexSet *set, ShMapTexLayout *layout, const ShMapVertex *src,
                    const uint8_t *tri_slots, size_t first_tri, size_t tri_count, ShMapVertex *dst)
{
    const int page_count = layout->page_count;
    const float inv = 1.0f / SH_ATLAS_SIZE;

    for (size_t t = first_tri; t < first_tri + tri_count; t++) {
        const unsigned slot = tri_slots[t];
        const int b = bucket_of(set, page_count, slot);
        ShMapVertex *out = &dst[layout->cursor[b]];
        memcpy(out, &src[t * 3], sizeof(ShMapVertex) * 3);
        layout->cursor[b] += 3;

        if (b == page_count) {
            continue;
        }
        const ShMapTexPair *pair = &set->pairs[slot];
        for (int k = 0; k < 3; k++) {
            /* Sample texel centers; PICA stores texture row 0 at t = 1. */
            const float u = pair->x + (out[k].uv[0] - pair->min_u) + 0.5f;
            const float v = pair->y + (out[k].uv[1] - pair->min_v) + 0.5f;
            out[k].uv[0] = u * inv;
            out[k].uv[1] = 1.0f - v * inv;
        }
    }
}

int sh_maptex_finalize(const ShMapTexSet *set, int page_count, const ShMapVertex *src, const uint8_t *tri_slots,
                       size_t vert_count, ShMapVertex *dst, ShMapRange *ranges)
{
    ShMapTexLayout layout;

    sh_maptex_layout(set, page_count, tri_slots, vert_count, &layout);
    sh_maptex_emit(set, &layout, src, tri_slots, 0, vert_count / 3, dst);
    memcpy(ranges, layout.ranges, sizeof(ShMapRange) * (size_t)layout.range_count);
    return layout.range_count;
}
