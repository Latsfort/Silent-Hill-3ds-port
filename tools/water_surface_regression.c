#include "sh3ds/water_surface.h"
#include "sh3ds/water_mesh.h"
#include "sh3ds/water_render_data.h"
#include "sh3ds/fixed.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned checks = 0;
    ShEnvState environment = {0};
    environment.preset.flags[1] = 255;
    environment.preset.world_light = 128 * 32;
    for (int ball = 0; ball <= 1; ball++) {
        for (int dt = 68; dt <= 272; dt *= 2) {
            ShWaterConfig config = {0};
            sh_water_configure_roof(&config, ball);
            for (int tick = 0; tick < 600; tick++) {
                const ShWaterConfig before = config;
                ShWaterSurfaceQuad output[64];
                memset(output, 0x5A, sizeof(output));
                const size_t count = sh_water_surface(&config, tick * 13, &environment, NULL, output, 63, dt);
                assert(count <= SH_WATER_SURFACE_MAX_QUADS);
                const ShCamera camera = {{0}, 0, 0, 53.13f};
                ShMapVertex mesh[64 * 6];
                memset(mesh, 0x5A, sizeof(mesh));
                assert(sh_water_surface_build_mesh(output, count, &camera, mesh, count * 6 + 1) == count * 6);
                const unsigned char *mesh_guard = (const unsigned char *)&mesh[count * 6];
                for (size_t byte = 0; byte < sizeof(mesh[0]); byte++) { assert(mesh_guard[byte] == 0x5A); }
                assert(sh_water_surface_build_mesh(output, count, &camera, mesh, 5) == 0);
                for (size_t quad = 0; quad < count; quad++) {
                    for (int vertex = 0; vertex < 6; vertex++) {
                        const int corner = vertex < 3 ? vertex : vertex == 3 ? 2 : vertex == 4 ? 1 : 3;
                        const ShMapVertex *v = &mesh[quad * 6 + vertex];
                        for (int axis = 0; axis < 3; axis++) {
                            assert(v->pos[axis] == (axis ? -1.0f : 1.0f) * (int16_t)(output[quad].position[corner][axis] >> 4) / 256.0f);
                            assert(v->color[axis] == output[quad].shade[corner]);
                        }
                        assert(v->color[3] == 255);
                        assert(v->uv[0] == (output[quad].u[corner] - SH_WATER_SURFACE_TEXTURE_U) / (float)SH_WATER_SURFACE_TEXTURE_W);
                        assert(v->uv[1] == 1.0f - (output[quad].v[corner] - SH_WATER_SURFACE_TEXTURE_V) / (float)SH_WATER_SURFACE_TEXTURE_H);
                    }
                }
                size_t index = 0;
                for (int stream = 0; stream < config.field_78; stream++) {
                    const int length = config.field_28[stream], spacing = config.field_44[stream];
                    const int phase = before.field_54[stream];
                    const int segments = (length - phase + spacing - 1) / spacing + 1;
                    int progress = 0;
                    for (int segment = 0; segment < segments; segment++) {
                        assert(index < count);
                        const ShWaterSurfaceQuad *quad = &output[index++];
                        assert(quad->clut == 78 && quad->page == 45);
                        assert(quad->u[0] == 64 && quad->u[1] == 95 && quad->u[2] == 64 && quad->u[3] == 95);
                        const int next = segment == 0 ? phase : progress + spacing < length ? progress + spacing : length;
                        for (int corner = 0; corner < 4; corner++) {
                            int at = corner < 2 ? progress : next;
                            int height;
                            if (config.field_30[stream]) { height = config.field_10[stream] + at; }
                            else { height = config.field_10[stream] + (config.field_10[stream + 1] - config.field_10[stream]) * at / length; }
                            assert(quad->position[corner][1] == height);
                            assert(quad->shade[corner] <= 128);
                        }
                        assert(quad->v[0] == quad->v[1] && quad->v[2] == quad->v[3]);
                        if (segment == 0) { assert(quad->v[0] == 127 - phase * 64 / spacing && quad->v[2] == 127); }
                        else { assert(quad->v[0] == 64); }
                        progress = next;
                        checks++;
                    }
                    assert(progress == length);
                    const unsigned advance = ((uint64_t)dt * config.field_4C[stream]) >> 12;
                    assert(config.field_54[stream] == (uint16_t)(phase + advance) % spacing);
                }
                assert(index == count);
                const unsigned char *guard = (const unsigned char *)&output[63];
                for (size_t byte = 0; byte < sizeof(output[63]); byte++) { assert(guard[byte] == 0x5A); }
                ShWaterConfig truncated = before;
                ShWaterSurfaceQuad one;
                assert(sh_water_surface(&truncated, tick * 13, &environment, NULL, &one, 1, dt) == 1);
                assert(!memcmp(&one, &output[0], sizeof(one)));
                assert(!memcmp(truncated.field_54, config.field_54, sizeof(config.field_54)));
                truncated = before;
                assert(sh_water_surface(&truncated, tick * 13, &environment, NULL, NULL, 0, dt) == 0);
                assert(!memcmp(truncated.field_54, config.field_54, sizeof(config.field_54)));
            }
        }
    }
    printf("PASS: %u continuous surface bands, stream heights, clipped UVs, scrolling and output guards\n", checks);
    return 0;
}
