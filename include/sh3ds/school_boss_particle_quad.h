#pragma once
#include "sh3ds/school_boss_particle_slot.h"
#include <stdbool.h>

typedef struct {
    uint32_t uv0, uv1, color, xy[4];
    uint16_t uv2, uv3;
    int16_t scale;
    unsigned depth_bucket;
    bool rise;
} ShBossParticleQuad;

void sh_school_boss_particle_style(const ShSchoolBossParticleSlot *slot, ShBossParticleQuad *quad);
bool sh_school_boss_particle_quad(const ShSchoolBossParticleSlot *slot, ShBossParticleQuad *quad,
    int16_t screen_x, int16_t screen_y, int32_t depth, int32_t projection_scale, uint8_t base_size);
