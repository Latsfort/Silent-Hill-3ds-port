#include "sh3ds/school_boss_particle_quad.h"
#include <assert.h>
#include <stdio.h>

static int x(uint32_t packed) { return (int16_t)packed; }
static int y(uint32_t packed) { return (int16_t)(packed >> 16); }

int main(void)
{
    ShSchoolBossParticleSlot slot = {0};
    slot.field_A = 8;
    ShBossParticleQuad quad;
    sh_school_boss_particle_style(&slot, &quad);
    assert(quad.rise && quad.color == 0x2ec4c4b4u);
    assert(quad.uv0 == 0x020e8000u && quad.uv1 == 0x006d803fu && quad.uv2 == 0xbf00 && quad.uv3 == 0xbf3f);
    assert(sh_school_boss_particle_quad(&slot, &quad, 10, -5, 1000, 320, 60));
    assert(quad.depth_bucket == 125 && x(quad.xy[0]) == 29 && y(quad.xy[0]) == 14);
    assert(x(quad.xy[1]) == -9 && y(quad.xy[1]) == 13); /* Source packed addition borrows across halves. */
    assert(x(quad.xy[2]) == 29 && y(quad.xy[2]) == -24);
    assert(!sh_school_boss_particle_quad(&slot, &quad, 201, 0, 1000, 320, 60));
    assert(!sh_school_boss_particle_quad(&slot, &quad, 0, -161, 1000, 320, 60));
    assert(!sh_school_boss_particle_quad(&slot, &quad, 0, 0, 16384, 320, 60));
    assert(!sh_school_boss_particle_quad(&slot, &quad, 0, 0, 0, 320, 60));
    slot.field_A = 9;
    sh_school_boss_particle_style(&slot, &quad);
    assert(!quad.rise && quad.color == 0x2e404040u && quad.scale == 36);
    assert(quad.uv0 == 0x024ec000u && quad.uv1 == 0x004dc03fu);
    assert(sh_school_boss_particle_quad(&slot, &quad, 0, 0, 320, 320, 60));
    assert(x(quad.xy[0]) == -36 && y(quad.xy[0]) == -36);
    slot.field_A = 11;
    sh_school_boss_particle_style(&slot, &quad);
    assert(quad.color == 0x2e80c0ffu && quad.scale == 2);
    assert(sh_school_boss_particle_quad(&slot, &quad, -10, -10, 16000, 320, 60));
    assert(x(quad.xy[0]) == -10 && y(quad.xy[0]) == -10 && x(quad.xy[3]) == -9 && y(quad.xy[3]) == -9);
    unsigned cases = 0;
    for (unsigned state = 8; state <= 11; ++state) {
        for (unsigned variant = 0; variant < 6; ++variant) {
            for (unsigned phase = 0; phase <= 4096; phase += 16) {
                slot.field_A = state; slot.field_B = variant; slot.field_C.s_2.field_0 = phase;
                sh_school_boss_particle_style(&slot, &quad);
                assert((quad.color >> 24) == 0x2e);
                assert(sh_school_boss_particle_quad(&slot, &quad, -200, 160, 512, 320, 60));
                assert(quad.depth_bucket == 64);
                cases++;
            }
        }
    }
    puts("PASS: source UV/CLUT/tpage/RGB packets, packed-corner carry, depth/screen clipping, pixel minimums and6168 style/geometry fixtures");
    assert(cases == 6168);
    return 0;
}
