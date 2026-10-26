#include "sh3ds/coll.h"

#include <math.h>
#include <string.h>

#define IPD_MAGIC 20
#define COLL_OFF 0x54
#define SCORE_NONE 0x7FFFFFFF
/*
 * Building walls often store the edge at eave/roof height (2–6 m), not at the
 * curb. Treat any wall whose top is above the feet as solid; only ignore walls
 * we have already climbed over, or ledges far below the current floor.
 */
#define WALL_ABOVE_CLEARANCE 0.40f
#define WALL_BELOW_SLACK 1.20f

/* map0_s01 back counter (knife / map / light) — dining chairs must not seal the aisle. */
static bool coll_cafe_back_aisle(float x, float z)
{
    return x >= 2.4f && x <= 6.5f && z <= -269.8f && z >= -275.8f;
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int16_t rd_s16(const uint8_t *p)
{
    return (int16_t)(p[0] | (p[1] << 8));
}

static int32_t rd_s32(const uint8_t *p)
{
    return (int32_t)rd_u32(p);
}

static int32_t to_q8(float meters)
{
    return (int32_t)(meters * 256.0f + (meters >= 0.0f ? 0.5f : -0.5f));
}

void sh_coll_world_reset(ShCollWorld *world)
{
    world->count = 0;
    world->trigger_filter = false;
    world->trigger_flags = 0;
}

void sh_coll_trigger_flags_set(ShCollWorld *world, int flags)
{
    world->trigger_filter = flags >= 0;
    world->trigger_flags = flags >= 0 ? (uint16_t)flags : 0;
}

static bool collision_group(const ShCollWorld *world, unsigned group)
{
    return !world->trigger_filter || (world->trigger_flags & (1u << group)) != 0;
}

/* Collision_SubcellGet: (field_0_14 * 4) | field_2_14. */
static bool subcell_enabled(const ShCollWorld *world, const uint8_t *sub)
{
    const unsigned group = ((uint16_t)rd_s16(sub) >> 14) * 4u | ((uint16_t)rd_s16(sub + 2) >> 14);
    return collision_group(world, group);
}

bool sh_coll_world_add(ShCollWorld *world, const uint8_t *ipd, size_t ipd_size)
{
    if (!world || !ipd || ipd_size < COLL_OFF + 0x34 || ipd[0] != IPD_MAGIC || world->count >= SH_COLL_MAX_CHUNKS) {
        return false;
    }

    const uint8_t *c = ipd + COLL_OFF;
    const uint32_t counts = rd_u32(c + 8);
    const unsigned split_n = counts & 0xFFu;
    const unsigned surf_n = (counts >> 8) & 0xFFu;
    const unsigned sub_n = (counts >> 16) & 0xFFu;
    const unsigned p18_n = (counts >> 24) & 0xFFu;
    const int16_t cell = rd_s16(c + 0x1C);
    const unsigned nx = c[0x1E];
    const unsigned nz = c[0x1F];

    if (cell <= 0 || nx == 0 || nz == 0) {
        return false;
    }

    const uint32_t off_split = rd_u32(c + 0xC);
    const uint32_t off_surf = rd_u32(c + 0x10);
    const uint32_t off_sub = rd_u32(c + 0x14);
    const uint32_t off_18 = rd_u32(c + 0x18);
    const uint32_t off_ranges = rd_u32(c + 0x20);
    const uint16_t n28 = (uint16_t)rd_s16(c + 0x24);
    const uint16_t n2c = (uint16_t)rd_s16(c + 0x26);
    const uint32_t off_28 = rd_u32(c + 0x28);
    const uint32_t off_2c = rd_u32(c + 0x2C);

    if (COLL_OFF + off_ranges + (size_t)nx * nz * 4u > ipd_size) {
        return false;
    }
    if (surf_n && COLL_OFF + off_surf + surf_n * 12u > ipd_size) {
        return false;
    }
    if (split_n && COLL_OFF + off_split + split_n * 6u > ipd_size) {
        return false;
    }

    ShCollChunk *chunk = &world->chunks[world->count];
    chunk->pos_x_q8 = rd_s32(c + 0);
    chunk->pos_z_q8 = rd_s32(c + 4);
    chunk->cell_q8 = cell;
    chunk->nx = (uint8_t)nx;
    chunk->nz = (uint8_t)nz;
    chunk->surf_n = (uint8_t)surf_n;
    chunk->sub_n = (uint8_t)sub_n;
    chunk->split_n = (uint8_t)split_n;
    chunk->p18_n = (uint8_t)p18_n;
    chunk->n28 = n28;
    chunk->n2c = n2c;
    chunk->split_vertices = c + off_split;
    chunk->surfaces = c + off_surf;
    chunk->subcells = c + off_sub;
    chunk->ptr18 = c + off_18;
    chunk->ranges = c + off_ranges;
    chunk->ptr28 = c + off_28;
    chunk->ptr2c = c + off_2c;
    world->count++;
    return true;
}

float sh_coll_last_wall[5];

#ifndef SPLIT_SIDE_OP
#define SPLIT_SIDE_OP >
#endif

static int64_t closest_on_segment(int32_t ax, int32_t az, int32_t bx, int32_t bz, int32_t px, int32_t pz,
                                  int32_t *cx, int32_t *cz);

static int32_t surface_height_q8(const uint8_t *surf, int32_t ox, int32_t oz)
{
    const uint16_t flags = (uint16_t)(surf[6] | (surf[7] << 8));
    const unsigned ground_type = flags & 31u;
    const unsigned disable = (flags >> 5) & 7u;
    if (disable || ground_type == 12u) {
        return SCORE_NONE;
    }

    const int16_t base = rd_s16(surf + 2);
    const int16_t sx = rd_s16(surf + 0);
    const int16_t sz = rd_s16(surf + 4);
    const int16_t tx = rd_s16(surf + 8);
    const int16_t tz = rd_s16(surf + 10);
    int64_t h = base;
    if (tx) {
        h += ((int64_t)tx * (ox - sx)) >> 12;
    }
    if (tz) {
        h += ((int64_t)tz * (oz - sz)) >> 12;
    }
    return (int32_t)h;
}

static void consider_surface(const ShCollChunk *chunk, unsigned si, int32_t ox, int32_t oz, int32_t *best_h,
                             int *best_type)
{
    if (si == 0xFFu || si >= chunk->surf_n) {
        return;
    }
    const uint8_t *surf = chunk->surfaces + (size_t)si * 12u;
    const int32_t h = surface_height_q8(surf, ox, oz);
    if (h < *best_h) {
        *best_h = h;
        *best_type = (int)(surf[6] & 31);
    }
}

static void collect_cell_heights(const ShCollWorld *world, const ShCollChunk *chunk, int32_t ox, int32_t oz, int32_t *heights, int *types, int *count,
                                 int max_count)
{
    if (chunk->cell_q8 <= 0 || chunk->surf_n == 0) {
        return;
    }
    const int ix = ox / chunk->cell_q8;
    const int iz = oz / chunk->cell_q8;
    if (ix < 0 || iz < 0 || ix >= chunk->nx || iz >= chunk->nz) {
        return;
    }

    const unsigned idx = (unsigned)iz * chunk->nx + (unsigned)ix;
    const unsigned next = idx + 1u;
    const uint8_t *range = chunk->ranges + idx * 4u;
    const int start28 = rd_s16(range + 0);
    const int start2c = rd_s16(range + 2);
    int end28 = chunk->n28;
    int end2c = chunk->n2c;
    if (next < (unsigned)chunk->nx * chunk->nz) {
        const uint8_t *nrange = chunk->ranges + next * 4u;
        end28 = rd_s16(nrange + 0);
        end2c = rd_s16(nrange + 2);
    }

    if (start2c >= 0 && end2c > start2c && end2c <= chunk->n2c) {
        for (int i = start2c; i < end2c && *count < max_count; i++) {
            const unsigned si = chunk->ptr2c[i];
            if (si >= chunk->surf_n || !collision_group(world, (rd_u32(chunk->surfaces + si * 12u + 4) >> 27) & 15u)) { continue; }
            int32_t best = SCORE_NONE;
            int type = 0;
            consider_surface(chunk, chunk->ptr2c[i], ox, oz, &best, &type);
            if (best != SCORE_NONE) {
                heights[(*count)] = best;
                types[(*count)] = type;
                (*count)++;
            }
        }
    }

    /* func_8006B318: a split line separates surfaceIdx0 / surfaceIdx1; only the side the point is on counts. */
    if (start28 >= 0 && end28 > start28 && end28 <= chunk->n28) {
        int64_t near_d2 = INT64_MAX, open_d2 = INT64_MAX;
        unsigned near_si = 0xFFu, open_si = 0xFFu;
        for (int i = start28; i < end28; i++) {
            const unsigned sub_i = chunk->ptr28[i];
            if (sub_i >= chunk->sub_n) {
                continue;
            }
            const uint8_t *sub = chunk->subcells + (size_t)sub_i * 10u;
            if (!subcell_enabled(world, sub)) { continue; }
            const unsigned v0i = sub[7];
            const unsigned v1i = sub[6];
            if (v0i >= chunk->split_n || v1i >= chunk->split_n) {
                continue;
            }
            const uint8_t *va = chunk->split_vertices + (size_t)v0i * 6u;
            const uint8_t *vb = chunk->split_vertices + (size_t)v1i * 6u;
            const int32_t ax = rd_s16(va + 0), az = rd_s16(va + 4);
            const int32_t bx = rd_s16(vb + 0), bz = rd_s16(vb + 4);
            int32_t cx, cz;
            const int64_t d2 = closest_on_segment(ax, az, bx, bz, ox, oz, &cx, &cz);
            const int64_t cross = (int64_t)(bx - ax) * (oz - az) - (int64_t)(bz - az) * (ox - ax);
            const unsigned si = cross SPLIT_SIDE_OP 0 ? sub[8] : sub[9];
            if (si == 0xFFu) {
                /* Open side of a wall/overhang edge: only a fallback if no split has a floor on our side. */
                if (d2 < open_d2) {
                    open_d2 = d2;
                    open_si = cross SPLIT_SIDE_OP 0 ? sub[9] : sub[8];
                }
                continue;
            }
            if (d2 < near_d2) {
                near_d2 = d2;
                near_si = si;
            }
        }
        if (near_si == 0xFFu) {
            near_si = open_si;
        }
        if (near_si != 0xFFu && *count < max_count) {
            int32_t best = SCORE_NONE;
            int type = 0;
            consider_surface(chunk, near_si, ox, oz, &best, &type);
            if (best != SCORE_NONE) {
                heights[(*count)] = best;
                types[(*count)] = type;
                (*count)++;
            }
        }
    }
}

bool sh_coll_ground_near(const ShCollWorld *world, float x, float z, float prefer_y, ShCollHit *out)
{
    const int32_t x_q8 = to_q8(x);
    const int32_t z_q8 = to_q8(-z);
    const int32_t prefer_q8 = to_q8(-prefer_y);
    int32_t heights[32];
    int types[32];
    int count = 0;

    out->valid = false;
    out->height = 0.0f;
    out->ground_type = 0;

    for (int i = 0; i < world->count && count < 32; i++) {
        const ShCollChunk *chunk = &world->chunks[i];
        const int32_t ox = x_q8 - chunk->pos_x_q8;
        const int32_t oz = z_q8 - chunk->pos_z_q8;
        if (ox < 0 || oz < 0) {
            continue;
        }
        collect_cell_heights(world, chunk, ox, oz, heights, types, &count, 32);
    }

    if (count == 0) {
        return false;
    }

    int best_i = 0;
    int32_t best_dist = SCORE_NONE;
    for (int i = 0; i < count; i++) {
        int32_t d = heights[i] - prefer_q8;
        if (d < 0) {
            d = -d;
        }
        if (d < best_dist) {
            best_dist = d;
            best_i = i;
        }
    }

    out->valid = true;
    out->height = -(float)heights[best_i] / 256.0f;
    out->ground_type = types[best_i];
    return true;
}

bool sh_coll_ground(const ShCollWorld *world, float x, float z, ShCollHit *out)
{
    return sh_coll_ground_near(world, x, z, 0.0f, out);
}

/* Closest point on segment AB to P; returns squared distance in Q8^2 units. */
static int64_t closest_on_segment(int32_t ax, int32_t az, int32_t bx, int32_t bz, int32_t px, int32_t pz,
                                  int32_t *cx, int32_t *cz)
{
    const int32_t abx = bx - ax;
    const int32_t abz = bz - az;
    const int64_t ab2 = (int64_t)abx * abx + (int64_t)abz * abz;
    if (ab2 <= 0) {
        *cx = ax;
        *cz = az;
        const int32_t dx = px - ax;
        const int32_t dz = pz - az;
        return (int64_t)dx * dx + (int64_t)dz * dz;
    }

    int64_t t = (int64_t)(px - ax) * abx + (int64_t)(pz - az) * abz;
    if (t <= 0) {
        *cx = ax;
        *cz = az;
    } else if (t >= ab2) {
        *cx = bx;
        *cz = bz;
    } else {
        *cx = ax + (int32_t)((t * abx) / ab2);
        *cz = az + (int32_t)((t * abz) / ab2);
    }
    const int32_t dx = px - *cx;
    const int32_t dz = pz - *cz;
    return (int64_t)dx * dx + (int64_t)dz * dz;
}

static bool wall_height_relevant_at(float feet_y, float wx, float wz, int16_t y0_q8, int16_t y1_q8)
{
    /* Wall top in 3DS Y (PS1 Y-down). Matches decomp: collide when not above the ledge. */
    const float top0 = -(float)y0_q8 / 256.0f;
    const float top1 = -(float)y1_q8 / 256.0f;
    const float top = top0 > top1 ? top0 : top1;
    if (feet_y > top + WALL_ABOVE_CLEARANCE) {
        return false; /* already above this wall (roof / jumped a low curb) */
    }
    if (top < feet_y - WALL_BELOW_SLACK) {
        return false; /* ledge belongs to a floor below us */
    }
    /* Cafe booth seats etc. — short walls Harry can walk past in the original. */
    float furn_max = SH_COLL_FURNITURE_WALL_MAX;
    if (coll_cafe_back_aisle(wx, wz)) {
        furn_max = 0.42f; /* seat lips only; keep tall backs solid */
    }
    if (top <= feet_y + furn_max) {
        return false;
    }
    return true;
}

/* Gather candidate ground heights (3DS Y meters) at (x,z). */
static int gather_heights(const ShCollWorld *world, float x, float z, float *out, int max_out)
{
    const int32_t x_q8 = to_q8(x);
    const int32_t z_q8 = to_q8(-z);
    int32_t heights[32];
    int types[32];
    int count = 0;
    int n = 0;

    for (int i = 0; i < world->count && count < 32; i++) {
        const ShCollChunk *chunk = &world->chunks[i];
        const int32_t ox = x_q8 - chunk->pos_x_q8;
        const int32_t oz = z_q8 - chunk->pos_z_q8;
        if (ox < 0 || oz < 0) {
            continue;
        }
        collect_cell_heights(world, chunk, ox, oz, heights, types, &count, 32);
    }
    for (int i = 0; i < count && n < max_out; i++) {
        out[n++] = -(float)heights[i] / 256.0f;
    }
    (void)types;
    return n;
}

/*
 * True if the far side has a *next* tread just above the feet (stair / curb lip).
 * Cap is STEP_UP only — a balcony 1 m up must stay a solid wall.
 * Do not use ground_near(): the lower tread often still exists past the lip.
 */
/* A lip is an edge whose top is the tread beyond it; a wall rises above the floor behind it. */
static bool lip_onto(const ShCollWorld *world, float x, float z, float feet_y, float wall_top)
{
    float hs[32];
    const int n = gather_heights(world, x, z, hs, 32);
    for (int i = 0; i < n; i++) {
        const float rise = hs[i] - feet_y;
        if (rise > 0.03f && rise <= SH_COLL_STEP_UP && wall_top <= hs[i] + 0.1f) {
            return true;
        }
    }
    return false;
}

/* Lowest surface in (feet_y, feet_y + max_up] — the next stair tread, not a floor above. */
static bool next_tread(const ShCollWorld *world, float x, float z, float feet_y, float max_up, float *out_y)
{
    float hs[32];
    const int n = gather_heights(world, x, z, hs, 32);
    bool found = false;
    float best = feet_y + max_up + 1.0f;
    for (int i = 0; i < n; i++) {
        const float rise = hs[i] - feet_y;
        if (rise > 0.03f && rise <= max_up && hs[i] < best) {
            best = hs[i];
            found = true;
        }
    }
    if (found) {
        *out_y = best;
    }
    return found;
}

/*
 * Pushes (x,z) out of solid walls and prop cylinders. Returns true if still
 * intersecting after the iteration budget (caller should reject the move).
 */
static bool resolve_solids(const ShCollWorld *world, float *x, float *z, float feet_y, float radius)
{
    const int32_t r_q8 = to_q8(radius);
    const int64_t r2 = (int64_t)r_q8 * r_q8;
    bool stuck = false;

    for (int iter = 0; iter < 4; iter++) {
        int32_t px = to_q8(*x);
        int32_t pz = to_q8(-*z);
        const float p_wx = *x;
        const float p_wz = *z;
        const bool cafe_aisle = coll_cafe_back_aisle(p_wx, p_wz);
        int32_t best_cx = 0, best_cz = 0;
        float best_nx = 0.0f, best_nz = 0.0f;
        int64_t best_d2 = r2;
        bool hit = false;

        for (int ci = 0; ci < world->count; ci++) {
            const ShCollChunk *chunk = &world->chunks[ci];

            for (unsigned i = 0; i < chunk->p18_n; i++) {
                const uint8_t *e = chunk->ptr18 + (size_t)i * 10u;
                const uint16_t flags = (uint16_t)(e[0] | (e[1] << 8));
                if (!collision_group(world, (flags >> 8) & 15u)) { continue; }
                if ((flags >> 5) & 7) {
                    continue;
                }
                const int32_t prop_r = rd_s16(e + 8);
                /* Dining chairs near the kitchen counter — walkable like PS1. */
                if (cafe_aisle && prop_r > 0 && prop_r < to_q8(1.05f)) {
                    continue;
                }
                /* Props/signs: always collide in XZ (they are tall thin cylinders). */
                const int32_t cx = chunk->pos_x_q8 + rd_s16(e + 2);
                const int32_t cz = chunk->pos_z_q8 + rd_s16(e + 6);
                const int32_t pr = prop_r + r_q8;
                const int32_t dx = px - cx;
                const int32_t dz = pz - cz;
                const int64_t d2 = (int64_t)dx * dx + (int64_t)dz * dz;
                const int64_t pr2 = (int64_t)pr * pr;
                if (d2 < pr2) {
                    /* Fake a closer hit so the push uses the prop radius. */
                    const int64_t rank = d2 < 1 ? 0 : (d2 * r2) / pr2;
                    if (rank < best_d2) {
                        best_d2 = rank;
                        best_cx = cx;
                        best_cz = cz;
                        best_nx = best_nz = 0.0f;
                        hit = true;
                        sh_coll_last_wall[0] = (float)cx / 256.0f;
                        sh_coll_last_wall[1] = -(float)cz / 256.0f;
                        sh_coll_last_wall[2] = -99.0f;
                        sh_coll_last_wall[3] = (float)prop_r / 256.0f;
                        /* Expand push target so we end up `pr` from the center. */
                        if (d2 == 0) {
                            best_cx = px - pr;
                            best_cz = pz;
                        } else {
                            /* Move the reference point so |P - ref| == r implies |P - C| == pr. */
                            const float scale = (float)r_q8 / (float)pr;
                            best_cx = px - (int32_t)((float)(px - cx) * scale);
                            best_cz = pz - (int32_t)((float)(pz - cz) * scale);
                        }
                    }
                }
            }

            for (unsigned i = 0; i < chunk->sub_n; i++) {
                const uint8_t *sub = chunk->subcells + (size_t)i * 10u;
                if (!subcell_enabled(world, sub)) { continue; }
                const unsigned s0 = sub[8];
                const unsigned s1 = sub[9];
                if (s0 != 0xFFu && s1 != 0xFFu) {
                    continue; /* two-sided floor split, not a wall */
                }
                const unsigned v0i = sub[7]; /* decomp swaps idx0/idx1 into splitVertex0/1 */
                const unsigned v1i = sub[6];
                if (v0i >= chunk->split_n || v1i >= chunk->split_n) {
                    continue;
                }
                const uint8_t *va = chunk->split_vertices + (size_t)v0i * 6u;
                const uint8_t *vb = chunk->split_vertices + (size_t)v1i * 6u;
                const int32_t ax = chunk->pos_x_q8 + rd_s16(va + 0);
                const int32_t az = chunk->pos_z_q8 + rd_s16(va + 4);
                const int32_t bx = chunk->pos_x_q8 + rd_s16(vb + 0);
                const int32_t bz = chunk->pos_z_q8 + rd_s16(vb + 4);
                int32_t cx, cz;
                const int64_t d2 = closest_on_segment(ax, az, bx, bz, px, pz, &cx, &cz);
                const float edge_wx = (float)cx / 256.0f;
                const float edge_wz = -(float)cz / 256.0f;
                if (!wall_height_relevant_at(feet_y, edge_wx, edge_wz, rd_s16(va + 2), rd_s16(vb + 2))) {
                    continue;
                }
                if (d2 < r2 && d2 < best_d2) {
                    /*
                     * Ramp / curb edges: if the far side of this wall has ground we can
                     * step onto, treat the edge as a walkable lip instead of a building wall.
                     */
                    const float csx = (float)cx / 256.0f;
                    const float csz = (float)cz / 256.0f;
                    const float psx = (float)px / 256.0f;
                    const float psz = (float)pz / 256.0f;
                    float nnx = psx - csx, nnz = psz - csz;
                    float nlen = sqrtf(nnx * nnx + nnz * nnz);
                    const float wall_top =
                        -(float)(rd_s16(va + 2) < rd_s16(vb + 2) ? rd_s16(va + 2) : rd_s16(vb + 2)) / 256.0f;
                    if (nlen > 1e-4f) {
                        /* Probe just past the lip — too far and a balcony reads as a step. */
                        const float far_x = csx - nnx / nlen * 0.35f;
                        const float far_z3 = -(csz - nnz / nlen * 0.35f);
                        if (lip_onto(world, far_x, far_z3, feet_y, wall_top)) {
                            continue;
                        }
                    }
                    best_d2 = d2;
                    best_cx = cx;
                    best_cz = cz;
                    /* func_8006B318/8006B8F8: keep the valid surface side of a one-sided split. */
                    const float side = s0 != 0xFFu ? 1.0f : s1 != 0xFFu ? -1.0f : 0.0f;
                    best_nx = -(float)(bz - az) * side;
                    best_nz = (float)(bx - ax) * side;
                    hit = true;
                    sh_coll_last_wall[0] = csx;
                    sh_coll_last_wall[1] = -csz;
                    sh_coll_last_wall[2] = wall_top;
                    sh_coll_last_wall[3] = (float)(sub[8] == 0xFFu ? -1 : (int)sub[8]);
                    sh_coll_last_wall[4] = (float)(sub[9] == 0xFFu ? -1 : (int)sub[9]);
                }
            }
        }

        if (!hit) {
            return false;
        }

        /* Push to exactly `radius` away from the closest solid point. */
        const float psx = (float)px / 256.0f;
        const float psz = (float)pz / 256.0f;
        const float csx = (float)best_cx / 256.0f;
        const float csz = (float)best_cz / 256.0f;
        float nx = psx - csx;
        float nz = psz - csz;
        float len = sqrtf(nx * nx + nz * nz);
        if (len < 1e-4f) {
            nx = best_nx;
            nz = best_nz;
            len = sqrtf(nx * nx + nz * nz);
            if (len < 1e-4f) { nx = 1.0f; nz = 0.0f; len = 1.0f; }
            stuck = true;
        }
        const float push = radius + 0.02f;
        const float npsx = csx + nx / len * push;
        const float npsz = csz + nz / len * push;
        *x = npsx;
        *z = -npsz;
    }
    return stuck;
}

static bool ground_ok(const ShCollWorld *world, float x, float z, float old_ground, float move_len, ShCollHit *hit)
{
    if (!sh_coll_ground_near(world, x, z, old_ground, hit)) {
        return false;
    }
    const float rise = hit->height - old_ground;
    /* Allow steep ramps: up to STEP_UP absolutely, or a slope ratio over the step length. */
    const float max_up = SH_COLL_STEP_UP + move_len * SH_COLL_MAX_SLOPE;
    return rise <= max_up && rise >= -SH_COLL_STEP_DOWN;
}

float sh_coll_ray(const ShCollWorld *world, float x0, float z0, float x1, float z1, float feet_y, float min_rise)
{
    /* PS1 XZ in meters: (x, -z). */
    const float ax = x0, az = -z0;
    const float rx = x1 - x0, rz = -(z1 - z0);
    float best = 1.0f;

    if (!world) {
        return best;
    }
    for (int ci = 0; ci < world->count; ci++) {
        const ShCollChunk *chunk = &world->chunks[ci];
        for (unsigned i = 0; i < chunk->sub_n; i++) {
            const uint8_t *sub = chunk->subcells + (size_t)i * 10u;
            if (!subcell_enabled(world, sub)) { continue; }
            if (sub[8] != 0xFFu && sub[9] != 0xFFu) {
                continue;
            }
            const unsigned v0i = sub[7];
            const unsigned v1i = sub[6];
            if (v0i >= chunk->split_n || v1i >= chunk->split_n) {
                continue;
            }
            const uint8_t *va = chunk->split_vertices + (size_t)v0i * 6u;
            const uint8_t *vb = chunk->split_vertices + (size_t)v1i * 6u;
            const float top0 = -(float)rd_s16(va + 2) / 256.0f;
            const float top1 = -(float)rd_s16(vb + 2) / 256.0f;
            const float top = top0 > top1 ? top0 : top1;
            /* Curbs and low ledges do not block the view; floors below us neither. */
            if (top < feet_y + min_rise || top < feet_y - WALL_BELOW_SLACK) {
                continue;
            }

            const float px = (float)(chunk->pos_x_q8 + rd_s16(va + 0)) / 256.0f;
            const float pz = (float)(chunk->pos_z_q8 + rd_s16(va + 4)) / 256.0f;
            const float sx = (float)(rd_s16(vb + 0) - rd_s16(va + 0)) / 256.0f;
            const float sz = (float)(rd_s16(vb + 4) - rd_s16(va + 4)) / 256.0f;

            const float denom = rx * sz - rz * sx;
            if (fabsf(denom) < 1e-6f) {
                continue;
            }
            const float qx = px - ax, qz = pz - az;
            const float t = (qx * sz - qz * sx) / denom;
            const float u = (qx * rz - qz * rx) / denom;
            if (t >= 0.0f && t < best && u >= 0.0f && u <= 1.0f) {
                best = t;
            }
        }
    }
    return best;
}

bool sh_coll_nudge(const ShCollWorld *world, float *x, float *z, float feet_y, float radius)
{
    if (!world || !x || !z) {
        return false;
    }
    return resolve_solids(world, x, z, feet_y, radius);
}

bool sh_coll_move(const ShCollWorld *world, float *x, float *y, float *z, float dx, float dz, float radius,
                  float eye_height)
{
    ShCollHit start;
    if (!sh_coll_ground_near(world, *x, *z, *y - eye_height, &start)) {
        *x += dx;
        *z += dz;
        return false;
    }

    const float old_ground = start.height;
    const float feet = old_ground;
    const float move_len = sqrtf(dx * dx + dz * dz);
    float nx = *x + dx;
    float nz = *z + dz;

    resolve_solids(world, &nx, &nz, feet, radius);

    ShCollHit hit;
    if (ground_ok(world, nx, nz, old_ground, move_len, &hit)) {
        /*
         * Only snap to the next tread when ground_near stuck to the lower one
         * while a single riser sits just above (stair cell with both surfaces).
         */
        float tread_y;
        if (next_tread(world, nx, nz, old_ground, SH_COLL_STEP_UP, &tread_y) &&
            tread_y > hit.height + 0.03f && tread_y - old_ground <= SH_COLL_STEP_UP) {
            hit.height = tread_y;
        }
        resolve_solids(world, &nx, &nz, hit.height, radius);
        if (ground_ok(world, nx, nz, old_ground, move_len, &hit)) {
            if (next_tread(world, nx, nz, old_ground, SH_COLL_STEP_UP, &tread_y) &&
                tread_y > hit.height + 0.03f && tread_y - old_ground <= SH_COLL_STEP_UP) {
                hit.height = tread_y;
            }
            *x = nx;
            *z = nz;
            *y = hit.height + eye_height;
            return true;
        }
    }

    /*
     * Short stair probe only when almost blocked — never vault onto floors/ledges
     * further ahead (that caused church "flight" and sewer wall hops).
     */
    const float moved = sqrtf((nx - *x) * (nx - *x) + (nz - *z) * (nz - *z));
    if (move_len > 1e-4f && moved < move_len * 0.25f) {
        const float inv = 1.0f / move_len;
        const float dir_x = dx * inv;
        const float dir_z = dz * inv;
        const float px = *x + dir_x * 0.40f;
        const float pz = *z + dir_z * 0.40f;
        float tread_y;
        if (next_tread(world, px, pz, old_ground, SH_COLL_STEP_UP, &tread_y)) {
            float sx = px, sz = pz;
            resolve_solids(world, &sx, &sz, tread_y, radius);
            if (ground_ok(world, sx, sz, tread_y, 0.40f, &hit)) {
                *x = sx;
                *z = sz;
                *y = tread_y + eye_height;
                return true;
            }
        }
    }

    /* Slide along axes if the full move failed. */
    nx = *x + dx;
    nz = *z;
    resolve_solids(world, &nx, &nz, feet, radius);
    if (ground_ok(world, nx, nz, old_ground, fabsf(dx), &hit)) {
        float tread_y;
        if (next_tread(world, nx, nz, old_ground, SH_COLL_STEP_UP, &tread_y) &&
            tread_y > hit.height + 0.03f) {
            hit.height = tread_y;
        }
        *x = nx;
        *y = hit.height + eye_height;
        return true;
    }

    nx = *x;
    nz = *z + dz;
    resolve_solids(world, &nx, &nz, feet, radius);
    if (ground_ok(world, nx, nz, old_ground, fabsf(dz), &hit)) {
        float tread_y;
        if (next_tread(world, nx, nz, old_ground, SH_COLL_STEP_UP, &tread_y) &&
            tread_y > hit.height + 0.03f) {
            hit.height = tread_y;
        }
        *z = nz;
        *y = hit.height + eye_height;
        return true;
    }

    *y = old_ground + eye_height;
    return true;
}
