#include "sh3ds/water_mesh.h"
#include "sh3ds/water_texture.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned checks = 0;
    ShEnvState env = {0};
    env.preset.flags[1] = 255;
    env.preset.world_light = 128 * 32;
    for (int ball = 0; ball <= 1; ball++) {
        ShWaterConfig config = {0};
        sh_water_configure_roof(&config, ball);
        for (int stream = 0; stream < config.field_78; stream++) {
            for (int variant = 0; variant < 4; variant++) {
                ShWaterParticle particle = {0};
                sh_water_particle_spawn(&particle, &config, 2, stream);
                particle.field_A = 27;
                particle.field_C.s_1.field_0 = stream * 4 + variant;
                ShWaterPool pool = {&particle, 1, 0, 0};
                ShCamera camera = {{SH_Q12_TO_FLOAT(particle.field_0.vx_0),
                    -SH_Q12_TO_FLOAT(particle.vy_8), -SH_Q12_TO_FLOAT(particle.field_4.vz_4) + 4}, 0, 0, 53.13f};
                const int32_t camera_q8[3] = {SH_Q12(camera.pos[0]) >> 4, -SH_Q12(camera.pos[1]) >> 4, -SH_Q12(camera.pos[2]) >> 4};
                for (int progress = 0; progress < config.field_28[stream] + config.field_64[stream]; progress += 17) {
                    particle.field_10.s_3.field_0 = progress;
                    ShMapVertex vertices[7];
                    memset(vertices, 0x5A, sizeof(vertices));
                    assert(sh_water_ribbon_build_mesh(&pool, &config, &camera, &env, NULL, vertices, 5) == 0);
                    assert(sh_water_ribbon_build_mesh(&pool, &config, &camera, &env, NULL, vertices, 7) == 6);
                    ShWaterVisibility visibility = {{0}};
                    assert(sh_water_ribbon_build_mesh_visible(&pool, &config, &camera, &env, NULL, vertices, 7, &visibility) == 0);
                    const unsigned char *guard = (const unsigned char *)&vertices[6];
                    for (size_t byte = 0; byte < sizeof(vertices[6]); byte++) { assert(guard[byte] == 0x5A); }
                    ShWaterRibbon ribbon;
                    sh_water_ribbon(&particle, &config, camera_q8, 0, &ribbon);
                    const unsigned tile = sh_water_spray_shade_slot(ribbon.shade) * SH_WATER_RIBBON_TEXTURE_VARIANTS + variant;
                    for (int vertex = 0; vertex < 6; vertex++) {
                        const int corner = vertex < 3 ? vertex : vertex == 3 ? 2 : vertex == 4 ? 1 : 3;
                        for (int axis = 0; axis < 3; axis++) {
                            const float expected = (axis ? -1.0f : 1.0f) * (ribbon.position[corner][axis] + camera_q8[axis]) / 256.0f;
                            assert(vertices[vertex].pos[axis] == expected);
                        }
                        const unsigned u = tile % SH_WATER_RIBBON_ATLAS_COLUMNS * SH_WATER_RIBBON_TEXTURE_W + (corner & 1 ? SH_WATER_RIBBON_TEXTURE_W - 1 : 0);
                        const unsigned v = tile / SH_WATER_RIBBON_ATLAS_COLUMNS * SH_WATER_RIBBON_TEXTURE_H + ribbon.v_start + (corner & 2 ? ribbon.v_length : 0);
                        assert(fabsf(vertices[vertex].uv[0] * SH_WATER_RIBBON_ATLAS_WIDTH - u) < 0.001f);
                        assert(fabsf((1 - vertices[vertex].uv[1]) * SH_WATER_RIBBON_ATLAS_HEIGHT - v) < 0.001f);
                    }
                    checks++;
                }
                ShMapVertex vertices[6];
                camera.pos[2] -= 8;
                assert(sh_water_ribbon_build_mesh(&pool, &config, &camera, &env, NULL, vertices, 6) == 0);
                camera.pos[2] += 84;
                assert(sh_water_ribbon_build_mesh(&pool, &config, &camera, &env, NULL, vertices, 6) == 0);
            }
        }
    }
    printf("PASS: %u ribbon mesh positions/UV/guard cases; behind-camera and distance rejection\n", checks);
    return 0;
}
