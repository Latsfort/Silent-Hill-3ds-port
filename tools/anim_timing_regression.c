#include "sh3ds/anim_timing.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const ShAnimClipTiming loop = {10, 12, -1};
    const ShAnimClipTiming once = {10, 12, 99};
    for (int offset = -32768; offset <= 32768; offset++) {
        ShAnimTiming model = {.time = 10 * 4096};
        ShAnimPoseTiming pose;
        sh_anim_loop_timing(&model, &loop, offset, &pose);
        const int wrapped = (offset % (3 * 4096) + 3 * 4096) % (3 * 4096);
        assert(model.time == 10 * 4096 + wrapped);
        assert(model.keyframe == 10 + wrapped / 4096);
        assert(pose.first == model.keyframe && pose.second == (pose.first == 12 ? 10 : pose.first + 1));
        assert(pose.alpha == wrapped % 4096 && model.alpha == 0);
        model = (ShAnimTiming){.status = 8, .keyframe = 11, .time = 11 * 4096};
        sh_anim_once_timing(&model, &once, offset, &pose);
        const int time = 11 * 4096 + offset;
        const int clamped = time < 10 * 4096 ? 10 * 4096 : time > 12 * 4096 ? 12 * 4096 : time;
        assert(model.time == clamped && model.keyframe == clamped / 4096);
        assert(model.status == (offset && (time <= 10 * 4096 || time >= 12 * 4096) ? 99 : 8));
        assert(pose.first == model.keyframe && pose.second == pose.first + 1 && pose.alpha == (clamped & 4095));
    }
    const ShAnimClipTiming blend = {-1, 10, 5};
    for (int step = 1; step <= 4096; step++) {
        ShAnimTiming model = {.status = 4, .keyframe = 7};
        ShAnimPoseTiming pose;
        for (int tick = 0; tick < 4096 / step + 1 && model.status == 4; tick++) {
            const int previous = model.alpha;
            sh_anim_blend_timing(&model, &blend, step, &pose);
            const int alpha = previous + step;
            assert(model.time == (alpha >= 2048 ? 10 : 7) * 4096);
            assert(pose.second == 10);
            if (alpha >= 4096) {
                assert(model.keyframe == 10 && model.status == 5 && model.alpha == 0);
                assert(pose.first == 10 && pose.alpha == 0);
            } else {
                assert(model.keyframe == 7 && pose.first == 7 && pose.alpha == alpha);
            }
        }
        assert(model.status == 5);
        sh_anim_loop_timing(&model, &loop, 3 * 4096 - 1, &pose);
        assert(pose.first == 12 && pose.second == 10 && pose.alpha == 4095);
    }
    puts("PASS: 65537 signed loop steps and 4096 blend timesteps, status links and seam interpolation");
    return 0;
}
