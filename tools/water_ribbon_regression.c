#include "sh3ds/water.h"
#include "sh3ds/fixed.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned checks = 0;
    const int32_t origin[3] = {0}, camera[3] = {-123, 45, 678};
    for (int ball = 0; ball <= 1; ball++) {
        ShWaterConfig config = {0};
        sh_water_configure_roof(&config, ball);
        for (int stream = 0; stream < config.field_78; stream++) {
            for (int variant = 0; variant < 4; variant++) {
                ShWaterParticle particle = {0};
                sh_water_particle_spawn(&particle, &config, 2, stream);
                particle.field_C.s_1.field_0 = stream * 4 + variant;
                const int limit = config.field_28[stream] + config.field_64[stream];
                for (int progress = 0; progress < limit; progress++) {
                    particle.field_10.s_3.field_0 = progress;
                    ShWaterParticle before = particle;
                    ShWaterRibbon base, shifted;
                    const int yaw = progress & 4095;
                    sh_water_ribbon(&particle, &config, origin, yaw, &base);
                    sh_water_ribbon(&particle, &config, camera, yaw, &shifted);
                    assert(!memcmp(&before, &particle, sizeof(particle)));
                    assert(base.variant == variant && shifted.variant == variant);
                    assert(base.shade >= 0 && base.shade <= 128 && shifted.shade == base.shade);
                    assert(base.v_start <= 63 && base.v_length <= 64);
                    assert(shifted.v_start == base.v_start && shifted.v_length == base.v_length);
                    for (int corner = 0; corner < 4; corner++) {
                        for (int axis = 0; axis < 3; axis++) {
                            assert(shifted.position[corner][axis] == (int16_t)(base.position[corner][axis] - camera[axis]));
                        }
                    }
                    int ratio;
                    if (progress < config.field_64[stream]) { ratio = progress * 4096 / config.field_64[stream]; }
                    else if (progress > config.field_28[stream]) { ratio = (limit - progress) * 4096 / config.field_64[stream]; }
                    else { ratio = 4096; }
                    assert(base.v_length == (ratio == 4096 ? 63 : ratio / 64));
                    if (config.field_30[stream]) {
                        assert(base.shade == ratio / 32);
                        const int lower = particle.vy_8 + (int)((int64_t)ratio * config.field_64[stream] / 4096);
                        assert(base.position[0][1] == particle.vy_8 >> 4);
                        assert(base.position[1][1] == base.position[0][1]);
                        assert(base.position[2][1] == lower >> 4 && base.position[3][1] == lower >> 4);
                        assert(base.position[0][0] == base.position[2][0] && base.position[0][2] == base.position[2][2]);
                        assert(base.position[1][0] == base.position[3][0] && base.position[1][2] == base.position[3][2]);
                    } else {
                        int upper = config.field_10[stream], lower = config.field_10[stream + 1];
                        const int difference = lower - upper;
                        if (progress >= config.field_64[stream]) {
                            upper += difference * (progress - config.field_64[stream]) / config.field_28[stream];
                        }
                        if (progress <= config.field_28[stream]) {
                            lower = config.field_10[stream] + difference * progress / config.field_28[stream];
                        }
                        assert(base.position[0][1] == upper >> 4 && base.position[1][1] == upper >> 4);
                        assert(base.position[2][1] == lower >> 4 && base.position[3][1] == lower >> 4);
                    }
                    checks++;
                }
            }
        }
    }
    printf("PASS: %u original roof ribbon progress/variant/camera/height/UV cases\n", checks);
    return 0;
}
