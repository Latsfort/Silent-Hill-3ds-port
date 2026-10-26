#include "sh3ds/smoke_mesh.h"
#include "sh3ds/fixed.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static size_t index_at(unsigned x, unsigned y)
{
    size_t result = ((y / 8) * (SH_SMOKE_ATLAS_WIDTH / 8) + x / 8) * 64;
    for (unsigned bit = 0; bit < 3; bit++) {
        result |= ((x >> bit) & 1) << (bit * 2);
        result |= ((y >> bit) & 1) << (bit * 2 + 1);
    }
    return result;
}

int main(void)
{
    FILE *file = fopen("romfs/game/tim/BLD.TIM", "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    const size_t size = (size_t)ftell(file);
    uint8_t *bytes = malloc(size);
    assert(bytes);
    rewind(file);
    assert(fread(bytes, 1, size, file) == size);
    fclose(file);
    ShTim tim;
    assert(sh_tim_parse(bytes, size, &tim));
    const size_t pixels = SH_SMOKE_ATLAS_WIDTH * SH_SMOKE_ATLAS_HEIGHT;
    uint32_t *atlas = malloc(pixels * sizeof(*atlas));
    assert(atlas && sh_smoke_decode_atlas(&tim, atlas, pixels));
    assert(!sh_smoke_decode_atlas(&tim, atlas, pixels - 1));
    ShSmoke smoke = {0};
    smoke.active = true;
    smoke.config.half_size = 32;
    ShSmokeParticle *p = &smoke.particles[0];
    p->kind = 14;
    p->visible = true;
    p->age = 512;
    p->z = SH_Q12(2.0f);
    ShCamera camera = {.fov_deg = 90.0f};
    ShMapVertex mesh[6];
    unsigned checked = 0;
    for (unsigned shade = 1; shade <= 64; shade++) {
        p->shade = shade;
        for (unsigned variant = 0; variant < SH_SMOKE_TEXTURE_VARIANTS; variant++) {
            p->variant = variant;
            assert(sh_smoke_build_mesh(&smoke, &camera, mesh, 6) == 6);
            const unsigned tx = (unsigned)lroundf(mesh[0].uv[0] * SH_SMOKE_ATLAS_WIDTH);
            const unsigned ty = (unsigned)lroundf((1.0f - mesh[0].uv[1]) * SH_SMOKE_ATLAS_HEIGHT);
            for (unsigned y = 0; y < SH_SMOKE_TEXTURE_H; y++) {
                for (unsigned x = 0; x < SH_SMOKE_TEXTURE_W; x++) {
                    const unsigned u = SH_SMOKE_TEXTURE_U + x;
                    const unsigned v = SH_SMOKE_TEXTURE_V + SH_SMOKE_TEXTURE_STRIDE * variant + y;
                    const unsigned entry = tim.pixels[v * tim.stride + u / 2] >> ((u & 1) * 4) & 15;
                    const unsigned offset = (SH_SMOKE_CLUT_ROW * tim.clut_colors + entry) * 2;
                    const uint16_t source = tim.clut[offset] | ((unsigned)tim.clut[offset + 1] << 8);
                    uint32_t expected = source ? 255 : 0;
                    for (unsigned axis = 0; axis < 3; axis++) {
                        const unsigned modulated = ((((source >> (axis * 5)) & 31) << 3) * shade) >> 7;
                        const unsigned channel = modulated >> 3;
                        expected |= ((channel << 3) | (channel >> 2)) << (24 - axis * 8);
                    }
                    assert(atlas[index_at(tx + x, ty + y)] == expected);
                    checked++;
                }
            }
        }
    }
    p->variant = 0;
    p->shade = 64;
    assert(sh_smoke_build_mesh(&smoke, &camera, mesh, 5) == 0);
    assert(sh_smoke_build_mesh(&smoke, &camera, mesh, 6) == 6);
    const float full = mesh[1].pos[0] - mesh[0].pos[0];
    p->age = 0;
    assert(sh_smoke_build_mesh(&smoke, &camera, mesh, 6) == 6);
    const float initial = mesh[1].pos[0] - mesh[0].pos[0];
    assert(initial > 0 && initial < full);
    p->age = 512;
    p->z = SH_Q12(63.0f);
    assert(sh_smoke_build_mesh(&smoke, &camera, mesh, 6) == 6);
    assert(fabsf((mesh[1].pos[0] - mesh[0].pos[0]) * 120.0f / 63.0f - 1.0f) < 0.0001f);
    p->z = SH_Q12(64.0f);
    assert(sh_smoke_build_mesh(&smoke, &camera, mesh, 6) == 0);
    p->z = -SH_Q12(2.0f);
    assert(sh_smoke_build_mesh(&smoke, &camera, mesh, 6) == 0);
    p->z = SH_Q12(2.0f);
    p->x = SH_Q12(4.0f);
    assert(sh_smoke_build_mesh(&smoke, &camera, mesh, 6) == 0);
    p->x = 0;
    p->visible = false;
    assert(sh_smoke_build_mesh(&smoke, &camera, mesh, 6) == 0);
    p->visible = true;
    p->shade = 0;
    assert(sh_smoke_build_mesh(&smoke, &camera, mesh, 6) == 0);
    free(atlas);
    free(bytes);
    printf("PASS: %u original BLD texels match PS1 5-bit modulation; mesh growth, one-pixel fallback, depth/clipping and capacity checks\n", checked);
    return 0;
}
