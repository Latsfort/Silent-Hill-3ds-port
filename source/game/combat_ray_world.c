#include "sh3ds/combat_ray.h"
#include "sh3ds/coll.h"
#include "sh3ds/fixed.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

typedef struct { int16_t vx, vy, vz, pad; } RayWorldShort;
typedef struct { int32_t vx, vy, vz, pad; } RayWorldVector;
typedef struct { int16_t vx, vy; } RayWorldPair;
typedef struct { int16_t vx, vz; } RayWorldCell;
typedef struct {
    RayWorldPair field_8;
    int16_t field_C, field_E;
    int32_t field_10, field_14;
    int16_t field_18;
    int32_t field_1C;
    /* Retail traversal checks the limit20 after up to two appends. */
    RayWorldCell field_20[22];
} RayWorldWalk;
typedef struct {
    int16_t field_0_0, field_2_0;
    unsigned field_0_14, field_2_14;
    uint8_t splitVertexIdx0, splitVertexIdx1, surfaceIdx0, surfaceIdx1;
} RayWorldEdge;
typedef struct {
    unsigned groundType, field_0_8;
    RayWorldShort offset;
    int16_t radiusOffset;
} RayWorldProp;

#define WORLD_MULT(a, b) ((int32_t)((uint32_t)(a) * (uint32_t)(b)) >> 12)
#define WORLD_Q12(v) ((int32_t)(v) * 4096)
#define WORLD_FRACT(v) ((v) & 4095)
#define WORLD_FROM(v, shift) ((v) >> (shift))
#define WORLD_SQUARE(v) ((v) * (v))

static int16_t world_read16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static unsigned world_ground(const uint8_t *surfaces, unsigned index)
{
    return surfaces[index * 12 + 6] & 31u;
}

static RayWorldShort world_vertex(const uint8_t *vertices, unsigned index)
{
    const uint8_t *p = vertices + index * 6;
    return (RayWorldShort){world_read16(p), world_read16(p + 2), world_read16(p + 4), 0};
}

static RayWorldEdge world_decode_edge(const uint8_t *p)
{
    uint16_t a = (uint16_t)world_read16(p), b = (uint16_t)world_read16(p + 2);
    return (RayWorldEdge){(int16_t)(a << 2) >> 2, (int16_t)(b << 2) >> 2,
                         a >> 14, b >> 14, p[6], p[7], p[8], p[9]};
}

static RayWorldProp world_decode_prop(const uint8_t *p)
{
    return (RayWorldProp){p[0] & 31u, p[1] & 15u,
                         {world_read16(p + 2), world_read16(p + 4), world_read16(p + 6), 0},
                         world_read16(p + 8)};
}

static uint32_t world_packed(const ShRayState *state)
{
    return state->field_58 | ((uint32_t)(uint16_t)state->field_5A << 16);
}

/* gpu.h gte_ldR11R12/ldR13R21, rtv0, stMAC12: stores low signed16 MAC values. */
static RayWorldShort world_project(uint32_t packed, int16_t x, int16_t z)
{
    uint32_t negative = 0u - packed;
    return (RayWorldShort){
        (int16_t)(((int32_t)(int16_t)packed * x + (int32_t)(int16_t)(packed >> 16) * z) >> 12),
        (int16_t)(((int32_t)(int16_t)(negative >> 16) * x + (int32_t)(int16_t)packed * z) >> 12), 0, 0};
}

static int32_t world_nclip(RayWorldShort a, RayWorldShort b)
{
    return (int32_t)((uint32_t)((int32_t)a.vx * b.vy) - (uint32_t)((int32_t)a.vy * b.vx));
}

static int16_t world_ir(int32_t value)
{
    return value < SHRT_MIN ? SHRT_MIN : value > SHRT_MAX ? SHRT_MAX : (int16_t)value;
}

/* GPF with sf=12, lm=0 followed by gte_stsv reads saturated IR1..3. */
static RayWorldShort world_gpf(int32_t alpha, int16_t x, int16_t y, int16_t z)
{
    return (RayWorldShort){world_ir((int32_t)(((int64_t)alpha * x) >> 12)),
                          world_ir((int32_t)(((int64_t)alpha * y) >> 12)),
                          world_ir((int32_t)(((int64_t)alpha * z) >> 12)), 0};
}

#include "combat_ray_world_steps.h"

void sh_combat_ray_world(ShRayState *state, const ShCollWorld *world)
{
    for (int i = 0; i < world->count; ++i) {
        const ShCollChunk *chunk = &world->chunks[i];
        if (!chunk->surf_n && !chunk->sub_n && !chunk->p18_n) {
            continue;
        }
        state->field_6C.positionX = chunk->pos_x_q8;
        state->field_6C.positionZ = chunk->pos_z_q8;
        state->field_6C.groundHeight = state->from.vx - chunk->pos_x_q8;
        state->field_6C.topHeight = state->from.vz - chunk->pos_z_q8;
        RayWorldWalk walk = {
            {state->field_6C.groundHeight, state->field_6C.topHeight},
            (int16_t)(state->field_6C.groundHeight + state->offset.vx),
            (int16_t)(state->field_6C.topHeight + state->offset.vz),
            chunk->nx, chunk->nz, chunk->cell_q8, 0, {{0, 0}}};
        world_walk(&walk, (RayWorldPair){state->offset.vx, state->offset.vy},
                          (RayWorldPair){state->offset.vz, 0});
        bool checked[256] = {false};
        for (int cell = 0; cell < walk.field_1C; ++cell) {
            unsigned index = walk.field_20[cell].vz * chunk->nx + walk.field_20[cell].vx;
            world_range(state, chunk->ranges + index * 4, chunk, checked);
        }
    }
}

bool sh_combat_ray_query(ShRayTrace *trace, const ShCollWorld *world,
                         const ShPathPoint *from, const ShPathPoint *offset,
                         ShRayCharacter **characters, int count, bool cylinder,
                         int16_t collision_flags)
{
    ShRayState state = {0};
    if (!sh_combat_ray_setup(&state, cylinder, 0, from, offset, 0, 0, characters, count, collision_flags)) {
        memset(trace, 0, sizeof(*trace));
        return false;
    }
    sh_combat_ray_world(&state, world);
    for (int i = 0; i < count; ++i) {
        sh_combat_ray_character(&state, characters[i]);
    }
    sh_combat_ray_finish(trace, &state, from, offset);
    return trace->hasHit;
}
