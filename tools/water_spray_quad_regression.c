#include "sh3ds/water.h"
#include "sh3ds/fixed.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned checks = 0;
    for (int ball = 0; ball <= 1; ball++) {
        ShWaterSprayConfig config[3] = {{0}};
        sh_water_spray_configure_roof(config, ball);
        for (int stream = 0; stream < (ball ? 3 : 2); stream++) {
            for (int age = 0; age <= config[stream].field_10; age++) {
                for (int variant = 0; variant < 4; variant++) {
                    for (int gain = 96; gain <= 159; gain++) {
                        ShWaterParticle particle = {0};
                        particle.field_C.s_0.field_0 = age;
                        particle.field_C.s_1.field_2 = variant;
                        particle.field_C.s_1.field_3 = gain;
                        particle.field_10.s_2.field_0 = stream;
                        const int depth = (age % 16 + 1) * 128;
                        const int pitch = ((age + gain) % 2049) - 1024;
                        const int illumination = (age + gain) % 129;
                        ShWaterSprayQuad quad;
                        sh_water_spray_quad(&particle, config, -100, 17, depth, 240, pitch, illumination, &quad);
                        const int phase = age * 4096 / config[stream].field_10;
                        const int base = (int)(((int64_t)config[stream].field_E * gain * sh_fixed_sin(phase / 4)) >> 24);
                        const int width = base * 240 / depth;
                        const int absolute_pitch = pitch < 0 ? -pitch : pitch;
                        const int height = width * (2048 - absolute_pitch) >> 10;
                        assert(quad.x[0] == (int16_t)(-100 - width) && quad.x[1] == (int16_t)(-100 + width));
                        assert(quad.x[2] == quad.x[0] && quad.x[3] == quad.x[1]);
                        assert(quad.y[0] == (int16_t)(17 - height) && quad.y[1] == quad.y[0]);
                        assert(quad.y[2] == 17 && quad.y[3] == 17);
                        const int triangle = phase < 2048 ? phase / 16 : (4096 - phase + 15) / 16;
                        assert(quad.shade == triangle * illumination / 128);
                        for (int corner = 0; corner < 4; corner++) {
                            assert(quad.u[corner] == (corner % 2 ? 63 : 0));
                            assert(quad.v[corner] == variant * 32 + (corner >= 2 ? 31 : 0));
                        }
                        assert(quad.clut == 14 && quad.page == 45);
                        checks++;
                    }
                }
            }
        }
    }
    printf("PASS: %u original spray quads, projected size, pitch, fade, UVs and texture state\n", checks);
    return 0;
}
