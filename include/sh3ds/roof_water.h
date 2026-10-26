#pragma once
#include "sh3ds/water_surface.h"

typedef struct ShRoofWater {
    ShWaterParticle particles[SH_ROOF_WATER_POOL_COUNT];
    ShWaterPool pool;
    ShWaterConfig config;
    ShWaterSprayConfig spray[3];
    ShWaterVisibility visibility;
    ShWaterSurfaceQuad surface[SH_WATER_SURFACE_MAX_QUADS];
    size_t surface_count;
    bool ball;
} ShRoofWater;

/* Owns a self-referencing pool pointer: initialize in place, do not copy after initialization. */

void sh_roof_water_init(ShRoofWater *water);
void sh_roof_water_prepare(ShRoofWater *water, bool ball);
void sh_roof_water_begin(ShRoofWater *water);
void sh_roof_water_stop(ShRoofWater *water);
bool sh_roof_water_update(ShRoofWater *water, int yaw, const ShEnvState *environment,
    const ShEnvLight *light, int32_t dt, int32_t gravity);
