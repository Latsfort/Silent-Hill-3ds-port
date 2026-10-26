#include "sh3ds/water_mesh.h"
#include "sh3ds/water_texture.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* sharedFunc_800CCE60_1_s02: preserve Q8 vector quantization before native drawing. */
size_t sh_water_surface_build_mesh(const ShWaterSurfaceQuad *quads, size_t count,
    const ShCamera *camera, ShMapVertex *out, size_t capacity)
{
    if (!quads || !camera || !out) { return 0; }
    const int32_t camera_q8[3] = {SH_Q12(camera->pos[0]) >> 4,
        -SH_Q12(camera->pos[1]) >> 4, -SH_Q12(camera->pos[2]) >> 4};
    size_t emitted = 0;
    for (size_t i = 0; i < count && capacity - emitted >= 6; i++) {
        const ShWaterSurfaceQuad *quad = &quads[i];
        if (quad->clut != SH_WATER_SURFACE_CLUT || quad->page != SH_WATER_SURFACE_PAGE) { continue; }
        ShMapVertex corners[4];
        for (int corner = 0; corner < 4; corner++) {
            for (int axis = 0; axis < 3; axis++) {
                const int32_t camera_component = axis == 0 ? (int16_t)camera_q8[axis] : camera_q8[axis];
                const int16_t relative = (quad->position[corner][axis] >> 4) - camera_component;
                corners[corner].pos[axis] = (axis ? -1.0f : 1.0f) * (relative + camera_q8[axis]) / 256.0f;
                corners[corner].color[axis] = quad->shade[corner];
            }
            corners[corner].color[3] = 255;
            corners[corner].uv[0] = (float)(quad->u[corner] - SH_WATER_SURFACE_TEXTURE_U) / SH_WATER_SURFACE_TEXTURE_W;
            corners[corner].uv[1] = 1.0f - (float)(quad->v[corner] - SH_WATER_SURFACE_TEXTURE_V) / SH_WATER_SURFACE_TEXTURE_H;
        }
        out[emitted++] = corners[0]; out[emitted++] = corners[1]; out[emitted++] = corners[2];
        out[emitted++] = corners[2]; out[emitted++] = corners[1]; out[emitted++] = corners[3];
    }
    return emitted;
}

/* sharedFunc_800CBDA8_1_s02, using the port camera projection in place of GTE. */
size_t sh_water_ribbon_build_mesh_visible(const ShWaterPool *pool, const ShWaterConfig *config,
    const ShCamera *camera, const ShEnvState *environment, const ShEnvLight *light,
    ShMapVertex *out, size_t capacity, const ShWaterVisibility *visibility)
{
    if (!pool || !pool->particles || !config || !camera || !environment || !out ||
        config->field_78 > 3 || camera->fov_deg <= 0 || camera->fov_deg >= 180) { return 0; }
    const float cy = cosf(camera->yaw), sy = sinf(camera->yaw);
    const float cp = cosf(camera->pitch), sp = sinf(camera->pitch);
    const float right[3] = {cy, 0, -sy}, up[3] = {sy * sp, cp, cy * sp};
    const float forward[3] = {-sy * cp, sp, -cy * cp};
    const float projection = 120.0f / tanf(camera->fov_deg * 0.008726646259971648f);
    const int yaw = (int)(-camera->yaw * (4096.0f / 6.283185307179586f));
    const int32_t camera_q8[3] = {SH_Q12(camera->pos[0]) >> 4,
        -SH_Q12(camera->pos[1]) >> 4, -SH_Q12(camera->pos[2]) >> 4};
    size_t count = 0;
    for (int i = 0; i < pool->count && capacity - count >= 6; i++) {
        if (visibility && (i >= SH_ROOF_WATER_POOL_COUNT || !visibility->visible[i])) { continue; }
        const ShWaterParticle *particle = &pool->particles[i];
        const unsigned stream = particle->field_C.s_1.field_0 >> 2;
        if (particle->field_A != 27 || stream >= config->field_78 || config->field_64[stream] <= 0 ||
            config->field_28[stream] <= 0) { continue; }
        ShWaterRibbon ribbon;
        sh_water_ribbon(particle, config, camera_q8, yaw, &ribbon);
        ShMapVertex corners[4];
        int depth_sum = 0;
        float last_depth = 0, last_x = 0, last_y = 0;
        for (int corner = 0; corner < 4; corner++) {
            float depth = 0, x = 0, y = 0;
            for (int axis = 0; axis < 3; axis++) {
                const float sign = axis ? -1.0f : 1.0f;
                const float relative = sign * ribbon.position[corner][axis] / 256.0f;
                corners[corner].pos[axis] = sign * (ribbon.position[corner][axis] + camera_q8[axis]) / 256.0f;
                depth += relative * forward[axis]; x += relative * right[axis]; y -= relative * up[axis];
            }
            const int depth_q8 = (int)(depth * 256);
            depth_sum += depth_q8 < 0 ? 0 : depth_q8 > 65535 ? 65535 : depth_q8;
            last_depth = depth; last_x = x; last_y = y;
        }
        const int depth = depth_sum >> 2;
        if (depth <= 0 || (depth >> 3) >= SH_WATER_RIBBON_ORDERING_TABLE_SIZE || last_depth <= 0 ||
            abs((int)(last_x * projection / last_depth)) > SH_WATER_RIBBON_SCREEN_LIMIT_X ||
            abs((int)(last_y * projection / last_depth)) > SH_WATER_RIBBON_SCREEN_LIMIT_Y) { continue; }
        const int32_t position[3] = {particle->field_0.vx_0, particle->vy_8, particle->field_4.vz_4};
        const uint8_t shade = (ribbon.shade * sh_env_light_intensity(environment, light, position)) >> 7;
        const unsigned tile = sh_water_spray_shade_slot(shade) * SH_WATER_RIBBON_TEXTURE_VARIANTS + ribbon.variant;
        const unsigned tx = tile % SH_WATER_RIBBON_ATLAS_COLUMNS * SH_WATER_RIBBON_TEXTURE_W;
        const unsigned ty = tile / SH_WATER_RIBBON_ATLAS_COLUMNS * SH_WATER_RIBBON_TEXTURE_H;
        for (unsigned corner = 0; corner < 4; corner++) {
            corners[corner].uv[0] = (float)(tx + (corner & 1 ? SH_WATER_RIBBON_TEXTURE_W - 1 : 0)) / SH_WATER_RIBBON_ATLAS_WIDTH;
            corners[corner].uv[1] = 1.0f - (float)(ty + ribbon.v_start + (corner & 2 ? ribbon.v_length : 0)) / SH_WATER_RIBBON_ATLAS_HEIGHT;
            memset(corners[corner].color, 255, sizeof(corners[corner].color));
        }
        out[count++] = corners[0]; out[count++] = corners[1]; out[count++] = corners[2];
        out[count++] = corners[2]; out[count++] = corners[1]; out[count++] = corners[3];
    }
    return count;
}

/* func_800CE164, using the port camera projection in place of GTE RTPS. */
size_t sh_water_spray_build_mesh_visible(const ShWaterPool *pool, const ShWaterSprayConfig *config,
    size_t config_count, const ShCamera *camera, const ShEnvState *environment,
    const ShEnvLight *light, ShMapVertex *out, size_t capacity, const ShWaterVisibility *visibility)
{
    if (!pool || !pool->particles || !config || !camera || !environment || !out ||
        camera->fov_deg <= 0.0f || camera->fov_deg >= 180.0f) { return 0; }
    const float cy = cosf(camera->yaw), sy = sinf(camera->yaw);
    const float cp = cosf(camera->pitch), sp = sinf(camera->pitch);
    const float right[3] = {cy, 0, -sy};
    const float up[3] = {sy * sp, cp, cy * sp};
    const float forward[3] = {-sy * cp, sp, -cy * cp};
    const float projection = 120.0f / tanf(camera->fov_deg * 0.008726646259971648f);
    const int pitch = (int)(camera->pitch * (4096.0f / 6.283185307179586f));
    size_t count = 0;
    for (int i = 0; i < pool->count && capacity - count >= 6; i++) {
        if (visibility && (i >= SH_ROOF_WATER_POOL_COUNT || !visibility->visible[i])) { continue; }
        const ShWaterParticle *particle = &pool->particles[i];
        const unsigned stream = particle->field_10.s_2.field_0;
        if (particle->field_A != 28 || stream >= config_count || config[stream].field_10 <= 0) { continue; }
        const int32_t position[3] = {particle->field_0.vx_0, particle->vy_8, particle->field_4.vz_4};
        const float center[3] = {SH_Q12_TO_FLOAT(position[0]), -SH_Q12_TO_FLOAT(position[1]),
                                -SH_Q12_TO_FLOAT(position[2])};
        float depth = 0, x = 0, y = 0;
        for (int axis = 0; axis < 3; axis++) {
            const float relative = center[axis] - camera->pos[axis];
            depth += relative * forward[axis];
            x += relative * right[axis];
            y -= relative * up[axis];
        }
        /* The GPU clips the near plane; avoid the source division at nonpositive depth. */
        const int depth_q8 = (int)(depth * 256.0f);
        if (depth_q8 <= 0) { continue; }
        ShWaterSprayQuad quad;
        sh_water_spray_quad(particle, config, (int)(x * projection / depth),
            (int)(y * projection / depth), depth_q8, (int)projection, pitch,
            sh_env_light_intensity(environment, light, position), &quad);
        const unsigned variant = quad.v[0] / SH_WATER_SPRAY_TEXTURE_STRIDE;
        if (variant >= SH_WATER_SPRAY_TEXTURE_VARIANTS) { continue; }
        const unsigned tile = sh_water_spray_shade_slot(quad.shade) * SH_WATER_SPRAY_TEXTURE_VARIANTS + variant;
        const unsigned tx = tile % SH_WATER_SPRAY_ATLAS_COLUMNS * SH_WATER_SPRAY_TEXTURE_W;
        const unsigned ty = tile / SH_WATER_SPRAY_ATLAS_COLUMNS * SH_WATER_SPRAY_TEXTURE_H;
        ShMapVertex corners[4];
        for (unsigned corner = 0; corner < 4; corner++) {
            const float px = quad.x[corner] * depth / projection;
            const float py = -quad.y[corner] * depth / projection;
            for (int axis = 0; axis < 3; axis++) {
                corners[corner].pos[axis] = camera->pos[axis] + forward[axis] * depth + right[axis] * px + up[axis] * py;
            }
            corners[corner].uv[0] = (float)(tx + quad.u[corner]) / SH_WATER_SPRAY_ATLAS_WIDTH;
            corners[corner].uv[1] = 1.0f - (float)(ty + quad.v[corner] - quad.v[0]) / SH_WATER_SPRAY_ATLAS_HEIGHT;
            memset(corners[corner].color, 255, sizeof(corners[corner].color));
        }
        out[count++] = corners[0]; out[count++] = corners[1]; out[count++] = corners[2];
        out[count++] = corners[2]; out[count++] = corners[1]; out[count++] = corners[3];
    }
    return count;
}

size_t sh_water_ribbon_build_mesh(const ShWaterPool *pool, const ShWaterConfig *config,
    const ShCamera *camera, const ShEnvState *environment, const ShEnvLight *light,
    ShMapVertex *out, size_t capacity)
{
    return sh_water_ribbon_build_mesh_visible(pool, config, camera, environment, light, out, capacity, NULL);
}

size_t sh_water_spray_build_mesh(const ShWaterPool *pool, const ShWaterSprayConfig *config,
    size_t config_count, const ShCamera *camera, const ShEnvState *environment,
    const ShEnvLight *light, ShMapVertex *out, size_t capacity)
{
    return sh_water_spray_build_mesh_visible(pool, config, config_count, camera, environment, light, out, capacity, NULL);
}
