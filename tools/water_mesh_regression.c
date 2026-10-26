#include "sh3ds/water_mesh.h"
#include "sh3ds/water_texture.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    ShWaterSprayConfig config[3] = {{0}};
    sh_water_spray_configure_roof(config, true);
    ShWaterParticle particles[2];
    memset(particles, 0, sizeof(particles));
    ShWaterParticle *p = &particles[0];
    p->field_A = 28;
    p->field_0.vx_0 = 4096;
    p->field_4.vz_4 = 12288;
    p->vy_8 = -2048;
    p->field_C.s_1.field_3 = 127;
    ShWaterPool pool = {particles, 2, 0, 0};
    ShEnvState environment = {0};
    environment.preset.flags[1] = 255;
    ShMapVertex mesh[7];
    unsigned checks = 0;
    for (int heading = 0; heading < 16; heading++) {
        for (int tilt = -3; tilt <= 3; tilt++) {
            ShCamera camera = {{0}, heading * 0.3926990817f, tilt * 0.13f, 53.13f};
            const float sy = sinf(camera.yaw), cy = cosf(camera.yaw);
            const float sp = sinf(camera.pitch), cp = cosf(camera.pitch);
            const float right[3] = {cy, 0, -sy}, up[3] = {sy * sp, cp, cy * sp};
            const float forward[3] = {-sy * cp, sp, -cy * cp};
            const float center[3] = {1, 0.5f, -3};
            for (int axis = 0; axis < 3; axis++) { camera.pos[axis] = center[axis] - forward[axis] * 4; }
            for (int stream = 0; stream < 3; stream++) {
                p->field_10.s_2.field_0 = stream;
                p->field_C.s_0.field_0 = config[stream].field_10 / 2;
                for (int variant = 0; variant < 4; variant++) {
                    p->field_C.s_1.field_2 = variant;
                    for (int shade = 0; shade < 256; shade += 17) {
                        environment.preset.world_light = shade * 32;
                        memset(mesh, 0x5A, sizeof(mesh));
                        assert(sh_water_spray_build_mesh(&pool, config, 3, &camera, &environment, NULL, mesh, 5) == 0);
                        assert(sh_water_spray_build_mesh(&pool, config, 3, &camera, &environment, NULL, mesh, 7) == 6);
                        ShWaterVisibility visibility = {{0}};
                        assert(sh_water_spray_build_mesh_visible(&pool, config, 3, &camera, &environment, NULL, mesh, 7, &visibility) == 0);
                        const unsigned char *guard = (const unsigned char *)&mesh[6];
                        for (size_t byte = 0; byte < sizeof(mesh[6]); byte++) { assert(guard[byte] == 0x5A); }
                        const float projection = 120 / tanf(camera.fov_deg * 0.008726646259971648f);
                        float depth = 0, x = 0, y = 0;
                        for (int axis = 0; axis < 3; axis++) {
                            const float relative = center[axis] - camera.pos[axis];
                            depth += relative * forward[axis]; x += relative * right[axis]; y -= relative * up[axis];
                        }
                        ShWaterSprayQuad expected;
                        sh_water_spray_quad(p, config, (int)(x * projection / depth), (int)(y * projection / depth),
                            (int)(depth * 256), (int)projection, (int)(camera.pitch * (4096 / 6.283185307179586f)), shade, &expected);
                        for (int vertex = 0; vertex < 6; vertex++) {
                            const int corner = vertex < 3 ? vertex : (vertex == 3 ? 2 : vertex == 4 ? 1 : 3);
                            float vx = 0, vy = 0, vz = 0;
                            for (int axis = 0; axis < 3; axis++) {
                                float relative = mesh[vertex].pos[axis] - camera.pos[axis];
                                vx += relative * right[axis]; vy -= relative * up[axis]; vz += relative * forward[axis];
                            }
                            assert(fabsf(vx * projection / vz - expected.x[corner]) < 0.001f);
                            assert(fabsf(vy * projection / vz - expected.y[corner]) < 0.001f);
                            const unsigned tile = sh_water_spray_shade_slot(expected.shade) * 4 + variant;
                            assert(fabsf(mesh[vertex].uv[0] * 512 - (tile % 8 * 64 + expected.u[corner])) < 0.001f);
                            assert(fabsf((1 - mesh[vertex].uv[1]) * 512 - (tile / 8 * 32 + expected.v[corner] - variant * 32)) < 0.001f);
                        }
                        checks++;
                    }
                }
            }
            for (int axis = 0; axis < 3; axis++) { camera.pos[axis] = center[axis] + forward[axis] * 4; }
            assert(sh_water_spray_build_mesh(&pool, config, 3, &camera, &environment, NULL, mesh, 7) == 0);
        }
    }
    printf("Water spray mesh: %u projection/UV/lighting/capacity cases passed.\n", checks);
    return 0;
}
