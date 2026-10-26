#include "sh3ds/school_boss_particle_quad.h"
#include <stdlib.h>
#include <string.h>

#define BOSS_QUAD_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BOSS_QUAD_XY(x, y) (((uint32_t)(x) & 65535u) + ((uint32_t)(y) << 16))

static void boss_quad_color(ShBossParticleQuad *quad, int32_t r, int32_t g, int32_t b, int32_t code)
{
    quad->color = (uint32_t)r + ((uint32_t)g << 8) + ((uint32_t)b << 16) + ((uint32_t)code << 24);
}

#include "school_boss_particle_quad_steps.h"

void sh_school_boss_particle_style(const ShSchoolBossParticleSlot *slot, ShBossParticleQuad *quad)
{
    memset(quad, 0, sizeof(*quad));
    quad->rise = slot->field_A == 8 || slot->field_A == 10;
    if (quad->rise) { boss_quad_rise_style(slot, quad); }
    else { boss_quad_fall_style(slot, quad); }
}

bool sh_school_boss_particle_quad(const ShSchoolBossParticleSlot *slot, ShBossParticleQuad *quad,
    int16_t screen_x, int16_t screen_y, int32_t depth, int32_t projection_scale, uint8_t base_size)
{
    if (quad->rise) {
        return boss_quad_rise_geometry(slot, quad, screen_x, screen_y, depth, projection_scale, base_size);
    }
    return boss_quad_fall_geometry(slot, quad, screen_x, screen_y, depth, projection_scale, base_size);
}
