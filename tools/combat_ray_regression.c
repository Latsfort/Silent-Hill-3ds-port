#include "sh3ds/combat_ray.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ShRayTrace cast(ShRayCharacter *characters, unsigned count,
                       ShPathPoint from, ShPathPoint delta, bool cylinder,
                       int16_t wall_distance)
{
    ShRayState state = {0};
    ShRayTrace trace = {0};
    assert(sh_combat_ray_setup(&state, cylinder, 0, &from, &delta, 0, 0, NULL, 0, 37));
    assert(state.field_4 == 37);
    if (wall_distance > 0) {
        state.hitDistance = wall_distance;
        state.target.vz = wall_distance;
    }
    for (unsigned i = 0; i < count; ++i) {
        sh_combat_ray_character(&state, &characters[i]);
    }
    sh_combat_ray_finish(&trace, &state, &from, &delta);
    return trace;
}

int main(void)
{
    ShRayCharacter shapes[2] = {0};
    shapes[0].position.vz = 5 * 4096;
    shapes[0].collision.cylinder.radius = 2 * 4096;
    shapes[0].collision.cylinder.field_2 = 4096;
    shapes[0].collision.box.top = -2 * 4096;
    shapes[0].collision.box.bottom = shapes[0].collision.box.height = 0;
    ShPathPoint from = {0, -4096, 0}, delta = {0, 0, 10 * 4096};
    ShRayTrace trace = cast(shapes, 1, from, delta, false, 0);
    assert(trace.hasHit && trace.character == shapes);
    assert(trace.hitDistance >= 4 * 4096 - 32 && trace.hitDistance <= 4 * 4096);
    assert(trace.target.vz == trace.hitDistance && trace.target.vy == -4096);
    trace = cast(shapes, 1, from, delta, true, 0);
    assert(trace.character == shapes && trace.hitDistance >= 3 * 4096 - 32 && trace.hitDistance <= 3 * 4096);

    from.vx = 4096;
    trace = cast(shapes, 1, from, delta, false, 0);
    assert(!trace.hasHit && !trace.character); /* Strict retail tangent test. */
    assert(trace.target.vx == from.vx && trace.target.vz == delta.vz && trace.groundHeight == 7680);
    trace = cast(shapes, 1, from, delta, true, 0);
    assert(trace.hasHit);
    from.vx = 0; from.vy = -3 * 4096;
    trace = cast(shapes, 1, from, delta, true, 0);
    assert(!trace.hasHit);
    from.vy = -4096;

    shapes[1] = shapes[0];
    shapes[1].position.vz = 3 * 4096;
    trace = cast(shapes, 2, from, delta, false, 0);
    assert(trace.character == &shapes[1]);
    trace = cast(shapes, 2, from, delta, false, 256);
    assert(trace.hasHit && !trace.character && trace.hitDistance == 4096);
    ShRayCharacter tmp = shapes[0]; shapes[0] = shapes[1]; shapes[1] = tmp;
    trace = cast(shapes, 2, from, delta, false, 0);
    assert(trace.character == shapes);

    /* All directions retain the same center hit and reject an elevated ray. */
    unsigned cases = 0;
    for (int axis = 0; axis < 2; ++axis) {
        for (int sign = -1; sign <= 1; sign += 2) {
            for (int distance = 3; distance <= 30; ++distance) {
                ShRayCharacter shape = shapes[0];
                shape.position.vx = axis ? sign * distance * 4096 : 0;
                shape.position.vz = axis ? 0 : sign * distance * 4096;
                delta.vx = axis ? sign * (distance + 3) * 4096 : 0;
                delta.vz = axis ? 0 : sign * (distance + 3) * 4096;
                trace = cast(&shape, 1, from, delta, false, 0);
                assert(trace.hasHit && trace.character == &shape);
                int expected = (distance - 1) * 4096;
                /* Retail SQRT lookup and Q8/Q12 division quantize long rays. */
                int tolerance = expected / 128 + 32;
                assert(trace.hitDistance >= expected - tolerance && trace.hitDistance <= expected + tolerance);
                ++cases;
            }
        }
    }
    ShRayState state = {0};
    delta = (ShPathPoint){0, 4096, 0};
    assert(!sh_combat_ray_setup(&state, false, 0, &from, &delta, 0, 0, NULL, 0, 0));
    printf("PASS: %u axial hits, weakpoint/LOS volumes, tangent/height rejection, nearest hit and wall precedence\n", cases);
    return 0;
}
