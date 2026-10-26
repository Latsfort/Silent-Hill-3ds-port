#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sh3ds/smoke_capacity.h"

typedef struct ShSmokeConfig {
    uint8_t shape, half_size, disable_spawn, reserved;
    int16_t count, ceiling, base, spread, speed, fade_rate, phase, depth_bias;
    int32_t origin_x, origin_z, emission_time;
} ShSmokeConfig;

typedef struct ShSmokeParticle {
    int32_t x, z;
    int16_t y;
    uint8_t kind, variant;
    int32_t age;
    uint8_t shade;
    bool visible;
} ShSmokeParticle;

typedef struct ShSmoke {
    ShSmokeConfig config;
    ShSmokeParticle particles[SH_SMOKE_CAPACITY];
    float accumulator;
    bool active;
} ShSmoke;

void sh_smoke_begin_acid(ShSmoke *smoke);
void sh_smoke_step(ShSmoke *smoke, int32_t delta_q12);
void sh_smoke_update(ShSmoke *smoke, float dt);
void sh_smoke_stop(ShSmoke *smoke);
