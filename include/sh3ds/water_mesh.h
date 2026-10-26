#pragma once
#include "sh3ds/gfx.h"
#include "sh3ds/water.h"
#include "sh3ds/water_surface.h"

/* Vertex RGB retains the PS1 byte shade; GPU modulation must use a 2x scale. */
size_t sh_water_surface_build_mesh(const ShWaterSurfaceQuad *quads, size_t count,
    const ShCamera *camera, ShMapVertex *out, size_t capacity);

size_t sh_water_spray_build_mesh(const ShWaterPool *pool, const ShWaterSprayConfig *config,
    size_t config_count, const ShCamera *camera, const ShEnvState *environment,
    const ShEnvLight *light, ShMapVertex *out, size_t capacity);
size_t sh_water_ribbon_build_mesh(const ShWaterPool *pool, const ShWaterConfig *config,
    const ShCamera *camera, const ShEnvState *environment, const ShEnvLight *light,
    ShMapVertex *out, size_t capacity);
size_t sh_water_spray_build_mesh_visible(const ShWaterPool *pool, const ShWaterSprayConfig *config,
    size_t config_count, const ShCamera *camera, const ShEnvState *environment,
    const ShEnvLight *light, ShMapVertex *out, size_t capacity, const ShWaterVisibility *visibility);
size_t sh_water_ribbon_build_mesh_visible(const ShWaterPool *pool, const ShWaterConfig *config,
    const ShCamera *camera, const ShEnvState *environment, const ShEnvLight *light,
    ShMapVertex *out, size_t capacity, const ShWaterVisibility *visibility);
