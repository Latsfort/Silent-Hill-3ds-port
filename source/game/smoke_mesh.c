#include "sh3ds/smoke_mesh.h"
#include "sh3ds/fixed.h"
#include <math.h>
#include <string.h>

#include "smoke_palette_data.h"

static size_t tiled_index(unsigned x, unsigned y)
{
    const unsigned tile = (((y >> 3) * (SH_SMOKE_ATLAS_WIDTH >> 3) + (x >> 3)) << 6);
    const unsigned morton = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) |
                            ((x & 4) << 2) | ((y & 4) << 3);
    return tile + morton;
}

bool sh_smoke_decode_atlas(const ShTim *tim, uint32_t *out, size_t pixels)
{
    if (!tim || !out || pixels < SH_SMOKE_ATLAS_WIDTH * SH_SMOKE_ATLAS_HEIGHT ||
        tim->width < SH_SMOKE_TEXTURE_U + SH_SMOKE_TEXTURE_W ||
        tim->height < SH_SMOKE_TEXTURE_V + SH_SMOKE_TEXTURE_STRIDE * SH_SMOKE_TEXTURE_VARIANTS ||
        tim->clut_rows <= SH_SMOKE_CLUT_ROW) {
        return false;
    }
    memset(out, 0, SH_SMOKE_ATLAS_WIDTH * SH_SMOKE_ATLAS_HEIGHT * sizeof(*out));
    for (unsigned slot = 0; slot < sizeof(s_smoke_slot_shade); slot++) {
        const unsigned shade = s_smoke_slot_shade[slot];
        for (unsigned variant = 0; variant < SH_SMOKE_TEXTURE_VARIANTS; variant++) {
            const unsigned tile = slot * SH_SMOKE_TEXTURE_VARIANTS + variant;
            const unsigned tx = tile % SH_SMOKE_ATLAS_COLUMNS * SH_SMOKE_TEXTURE_W;
            const unsigned ty = tile / SH_SMOKE_ATLAS_COLUMNS * SH_SMOKE_TEXTURE_H;
            for (unsigned y = 0; y < SH_SMOKE_TEXTURE_H; y++) {
                for (unsigned x = 0; x < SH_SMOKE_TEXTURE_W; x++) {
                    const uint32_t source = sh_tim_pixel_rgba_clut(tim, SH_SMOKE_TEXTURE_U + x,
                        SH_SMOKE_TEXTURE_V + variant * SH_SMOKE_TEXTURE_STRIDE + y, tim->clut_colors * SH_SMOKE_CLUT_ROW);
                    uint32_t color = source & 255;
                    for (int channel = 0; channel < 3; channel++) {
                        const unsigned shift = 24 - channel * 8;
                        /* PS1 texture colour * vertex colour / 128, truncated to 5 bits. */
                        const unsigned value = ((source >> (shift + 3)) & 31) * shade / 128;
                        color |= ((value << 3) | (value >> 2)) << shift;
                    }
                    out[tiled_index(tx + x, ty + y)] = color;
                }
            }
        }
    }
    return true;
}

size_t sh_smoke_build_mesh(const ShSmoke *smoke, const ShCamera *camera, ShMapVertex *out, size_t capacity)
{
    if (!smoke || !smoke->active || !camera || !out) { return 0; }
    const float cy = cosf(camera->yaw), sy = sinf(camera->yaw);
    const float cp = cosf(camera->pitch), sp = sinf(camera->pitch);
    const float right[3] = {cy, 0, -sy};
    const float up[3] = {sy * sp, cp, cy * sp};
    const float forward[3] = {-sy * cp, sp, -cy * cp};
    const float fov = camera->fov_deg > 10.0f && camera->fov_deg < 140.0f ? camera->fov_deg : 48.0f;
    const float projection = 120.0f / tanf(fov * 0.008726646259971648f);
    size_t count = 0;
    for (int i = 0; i < SH_SMOKE_CAPACITY && count + 6 <= capacity; i++) {
        const ShSmokeParticle *p = &smoke->particles[i];
        if (p->kind != 14 || !p->visible || !p->shade) { continue; }
        const float center[3] = {SH_Q12_TO_FLOAT(smoke->config.origin_x + p->x), -SH_Q12_TO_FLOAT(p->y),
                                -SH_Q12_TO_FLOAT(smoke->config.origin_z + p->z)};
        float depth = 0, screen_x = 0, screen_y = 0;
        for (int axis = 0; axis < 3; axis++) {
            const float relative = center[axis] - camera->pos[axis];
            depth += relative * forward[axis];
            screen_x += relative * right[axis];
            screen_y += relative * up[axis];
        }
        const int depth_q8 = (int)(depth * 256.0f);
        if (depth_q8 <= 0 || (depth_q8 >> 3) >= SH_SMOKE_ORDERING_TABLE_SIZE ||
            fabsf(screen_x * projection / depth) > 200.0f || fabsf(screen_y * projection / depth) > 160.0f) {
            continue;
        }
        int half = (int)(smoke->config.half_size * projection / depth_q8);
        if (p->age < 512) { half = half * (p->age * 4 + 2048) >> 12; }
        const float pixel = depth / projection;
        const float left = half ? -half * pixel : 0.0f;
        const float top = half ? half * pixel : 0.0f;
        const float extent = half ? half * pixel : pixel;
        const unsigned shade = p->shade < sizeof(s_smoke_shade_slot) ? p->shade : sizeof(s_smoke_shade_slot) - 1;
        const unsigned tile = s_smoke_shade_slot[shade] * SH_SMOKE_TEXTURE_VARIANTS + p->variant;
        const unsigned tx = tile % SH_SMOKE_ATLAS_COLUMNS * SH_SMOKE_TEXTURE_W;
        const unsigned ty = tile / SH_SMOKE_ATLAS_COLUMNS * SH_SMOKE_TEXTURE_H;
        ShMapVertex corners[4];
        for (unsigned corner = 0; corner < 4; corner++) {
            const float x = corner & 1 ? extent : left;
            const float y = corner & 2 ? -extent : top;
            for (int axis = 0; axis < 3; axis++) {
                corners[corner].pos[axis] = center[axis] + right[axis] * x + up[axis] * y;
            }
            corners[corner].uv[0] = (float)(tx + (corner & 1 ? SH_SMOKE_TEXTURE_W : 0)) / SH_SMOKE_ATLAS_WIDTH;
            corners[corner].uv[1] = 1.0f - (float)(ty + (corner & 2 ? SH_SMOKE_TEXTURE_H : 0)) / SH_SMOKE_ATLAS_HEIGHT;
            memset(corners[corner].color, 255, sizeof(corners[corner].color));
        }
        out[count++] = corners[0]; out[count++] = corners[1]; out[count++] = corners[2];
        out[count++] = corners[2]; out[count++] = corners[1]; out[count++] = corners[3];
    }
    return count;
}
