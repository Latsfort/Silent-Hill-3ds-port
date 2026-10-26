#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Walk collision from Silent Hill .IPD collision blocks (offset 0x54).
 *   - Ground / slopes from surface + subcell height samples
 *   - Building walls from one-sided subcell edges (surface index 0xFF)
 *   - Props / signs from ptr_18 cylinders
 *
 * Positions match the map renderer (meters):
 *   ps1 (x, y, z) Q8  ->  3DS (x, -y, -z) / 256
 */

#define SH_COLL_MAX_CHUNKS 192
#define SH_COLL_PLAYER_RADIUS 0.35f
/* One stair riser / curb only — larger values let Harry walk onto cafe tables. */
#define SH_COLL_STEP_UP 0.28f
#define SH_COLL_STEP_DOWN 2.50f
/* Continuous ramps (sewers): rise allowed per metre walked, on top of STEP_UP. */
#define SH_COLL_MAX_SLOPE 0.95f
/*
 * Low curb lips only. Cafe booth seats (~0.35–0.50 m) must stay solid — Harry
 * cannot walk through chairs. Values at/below this rise are ignored as walls.
 */
#define SH_COLL_FURNITURE_WALL_MAX 0.18f

typedef struct ShCollHit {
    bool valid;
    float height; /* meters, 3DS Y */
    int ground_type;
} ShCollHit;

typedef struct ShCollChunk {
    int32_t pos_x_q8;
    int32_t pos_z_q8;
    int16_t cell_q8;
    uint8_t nx;
    uint8_t nz;
    uint8_t surf_n;
    uint8_t sub_n;
    uint8_t split_n;
    uint8_t p18_n;
    uint16_t n28;
    uint16_t n2c;
    const uint8_t *surfaces;       /* surf_n * 12 */
    const uint8_t *subcells;       /* sub_n * 10 */
    const uint8_t *split_vertices; /* split_n * 6 (SVECTOR3 q8) */
    const uint8_t *ptr18;          /* p18_n * 10 */
    const uint8_t *ranges;         /* nx * nz * 4 */
    const uint8_t *ptr28;
    const uint8_t *ptr2c;
} ShCollChunk;

typedef struct ShCollWorld {
    ShCollChunk chunks[SH_COLL_MAX_CHUNKS];
    int count;
    bool trigger_filter;
    uint16_t trigger_flags;
} ShCollWorld;

extern float sh_coll_last_wall[5]; /* debug: last blocking edge x, z, top, side0, side1 */
void sh_coll_world_reset(ShCollWorld *world);
/* Negative disables filtering in areas whose retail world flags are not ported yet. */
void sh_coll_trigger_flags_set(ShCollWorld *world, int flags);
bool sh_coll_world_add(ShCollWorld *world, const uint8_t *ipd, size_t ipd_size);

bool sh_coll_ground(const ShCollWorld *world, float x, float z, ShCollHit *out);
bool sh_coll_ground_near(const ShCollWorld *world, float x, float z, float prefer_y, ShCollHit *out);

bool sh_coll_move(const ShCollWorld *world, float *x, float *y, float *z, float dx, float dz,
                  float radius, float eye_height);

/* Pushes (x,z) out of walls/props at feet height. Returns true if still overlapping. */
bool sh_coll_nudge(const ShCollWorld *world, float *x, float *z, float feet_y, float radius);

/*
 * Fraction in [0, 1] of the segment (x0,z0)->(x1,z1) before it crosses a wall
 * whose top is at least `min_rise` above `feet_y`; 1 when nothing is hit.
 */
float sh_coll_ray(const ShCollWorld *world, float x0, float z0, float x1, float z1, float feet_y, float min_rise);

#ifdef __cplusplus
}
#endif
