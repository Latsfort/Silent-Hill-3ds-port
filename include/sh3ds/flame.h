#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sh3ds/flame_data.h"

typedef struct ShFlamePoint { int16_t vx, vy, vz; } ShFlamePoint;
typedef struct ShFlameScreen { int16_t vx, vy; } ShFlameScreen;
typedef struct ShFlameQuad {
    int16_t x0, y0, x1, y1, x2, y2, x3, y3;
    uint8_t u0, v0, u1, v1, u2, v2, u3, v3, shade;
    uint16_t tpage, clut;
    int32_t ordering, depth;
} ShFlameQuad;
typedef struct ShFlame {
    uint32_t counter;
    uint32_t draw_counter;
    int16_t random[8];
    ShFlameQuad sprite;
    bool visible;
    bool started;
    double phase;
} ShFlame;

/* Return SZ3 / 4 like RotTransPers; `hand` selects the right-hand/view matrix. */
typedef int32_t (*ShFlameProject)(void *user, bool hand, const ShFlamePoint *point, ShFlameScreen *screen);
bool sh_flame_frame(ShFlame *flame, bool tick, ShFlameProject project, void *user);
