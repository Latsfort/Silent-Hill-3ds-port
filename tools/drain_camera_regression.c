#include "sh3ds/drain.h"
#include "sh3ds/fixed.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../source/game/drain_camera_data.h"

static ShPathPoint interpolate(ShPathPoint a, ShPathPoint b, int elapsed, int duration)
{
    ShPathPoint result;
    result.vx = a.vx + (int32_t)(((int64_t)b.vx - a.vx) * elapsed / duration);
    result.vy = a.vy + (int32_t)(((int64_t)b.vy - a.vy) * elapsed / duration);
    result.vz = a.vz + (int32_t)(((int64_t)b.vz - a.vz) * elapsed / duration);
    return result;
}

int main(void)
{
    unsigned samples = 0;
    for (int ball = 0; ball <= 1; ball++) {
        const int last = ball ? 5 : 3;
        for (int ticks = 0; ticks < s_drain_camera_times[last]; ticks++) {
            ShPathPoint camera, look;
            assert(sh_drain_camera_sample(ticks, ball, &camera, &look));
            int segment = 1;
            while (ticks >= s_drain_camera_times[segment]) { segment++; }
            const int index = segment / 2 + (ball ? 2 : 0);
            ShPathPoint expected_camera = s_drain_camera_positions[index];
            ShPathPoint expected_look = s_drain_camera_looks[index];
            if (!(segment & 1)) {
                const int elapsed = ticks - s_drain_camera_times[segment - 1];
                const int duration = s_drain_camera_times[segment] - s_drain_camera_times[segment - 1];
                expected_camera = interpolate(s_drain_camera_positions[index - 1], expected_camera, elapsed, duration);
                expected_look = interpolate(s_drain_camera_looks[index - 1], expected_look, elapsed, duration);
            }
            assert(memcmp(&camera, &expected_camera, sizeof(camera)) == 0);
            assert(memcmp(&look, &expected_look, sizeof(look)) == 0);
            ShDrainLight light;
            sh_drain_camera_light(camera, look, &light);
            assert(light.position.vx == camera.vx && light.position.vz == camera.vz);
            assert(light.position.vy == (camera.vy < SH_Q12(-1.5f) ? SH_Q12(-1.5f) : camera.vy));
            const int32_t x = (look.vx - camera.vx) >> 6, z = (look.vz - camera.vz) >> 6;
            const int32_t horizontal = sh_fixed_sqrt0(x * x + z * z) * 64;
            assert(light.rotation.vx == -sh_fixed_atan2(look.vy - camera.vy, horizontal));
            assert(light.rotation.vy == sh_fixed_atan2(look.vx - camera.vx, look.vz - camera.vz));
            assert(!light.rotation.vz);
            int32_t direction[3];
            sh_drain_light_direction(&light, direction);
            double norm = 0, target_norm = 0, dot = 0;
            const int32_t target[3] = {look.vx - camera.vx, look.vy - camera.vy, look.vz - camera.vz};
            for (int axis = 0; axis < 3; axis++) {
                norm += (double)direction[axis] * direction[axis];
                target_norm += (double)target[axis] * target[axis];
                dot += (double)direction[axis] * target[axis];
            }
            assert(fabs(sqrt(norm) / 4096 - 1) < 0.002);
            assert(dot / sqrt(norm * target_norm) > 0.9999);
            samples++;
        }
        ShPathPoint camera = {1, 2, 3}, look = {4, 5, 6};
        assert(!sh_drain_camera_sample(s_drain_camera_times[last], ball, &camera, &look));
        assert(camera.vx == 1 && camera.vy == 2 && camera.vz == 3 && look.vz == 6);
    }
    printf("PASS: %u Q12 camera samples, both valve branches, exact segment endpoints and completion\n", samples);
    return 0;
}
