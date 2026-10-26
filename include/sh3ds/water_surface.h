#pragma once
#include <stddef.h>
#include "sh3ds/water.h"
#include "sh3ds/environment.h"
#include "sh3ds/water_surface_data.h"

typedef struct ShWaterSurfaceQuad {
    int32_t position[4][3]; /* PS1 world Q12. */
    uint8_t shade[4], u[4], v[4];
    uint16_t clut, page;
} ShWaterSurfaceQuad;

size_t sh_water_surface(ShWaterConfig *config, int yaw, const ShEnvState *environment,
    const ShEnvLight *light, ShWaterSurfaceQuad *out, size_t capacity, int32_t dt);
