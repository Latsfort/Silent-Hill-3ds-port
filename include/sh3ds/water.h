#pragma once
#include <stdbool.h>
#include "sh3ds/water_types.h"

typedef struct ShWaterPool {
    ShWaterParticle *particles;
    int count, next;
    uint32_t flags;
} ShWaterPool;

typedef struct ShWaterVisibility {
    uint8_t visible[SH_ROOF_WATER_POOL_COUNT];
} ShWaterVisibility;

bool sh_water_particles_step(ShWaterPool *pool, const ShWaterConfig *config,
    const ShWaterSprayConfig *spray, int32_t dt, int32_t gravity, ShWaterVisibility *visibility);

typedef struct ShWaterSprayQuad {
    int16_t x[4], y[4];
    uint8_t u[4], v[4], shade;
    uint16_t clut, page;
} ShWaterSprayQuad;

typedef struct ShWaterRibbon {
    int16_t position[4][3]; /* Original camera-relative PS1 Q8 vectors, before rotation. */
    int32_t shade;
    uint8_t v_start, v_length, variant;
} ShWaterRibbon;

void sh_water_ribbon(const ShWaterParticle *particle, const ShWaterConfig *config,
    const int32_t camera[3], int yaw, ShWaterRibbon *out);

void sh_water_configure_roof(ShWaterConfig *config, bool ball);
void sh_water_particle_spawn(ShWaterParticle *particle, const ShWaterConfig *config, int mode, int stream);
bool sh_water_particle_step(ShWaterParticle *particle, const ShWaterConfig *config, int32_t dt, int32_t gravity);
void sh_water_pool_begin(ShWaterPool *pool, ShWaterConfig *config);
void sh_water_pool_stop(ShWaterPool *pool);
void sh_water_spray_configure_roof(ShWaterSprayConfig *config, bool ball);
void sh_water_spray_spawn(ShWaterParticle *particle, const ShWaterSprayConfig *config, int stream, int restart);
void sh_water_spray_begin(ShWaterPool *pool, const ShWaterSprayConfig *config, int stream);
bool sh_water_spray_step(ShWaterParticle *particle, const ShWaterSprayConfig *config, int32_t dt);
/* func_800CE164: projected depth must be positive. */
void sh_water_spray_quad(const ShWaterParticle *particle, const ShWaterSprayConfig *config,
                          int screen_x, int screen_y, int depth, int projection,
                          int pitch, int illumination, ShWaterSprayQuad *quad);
