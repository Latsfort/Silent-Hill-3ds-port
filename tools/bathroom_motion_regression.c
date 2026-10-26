#include "sh3ds/bathroom.h"
#include "sh3ds/fixed.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    ShBathroomMotion motion = {.elapsed = 4097};
    ShBathroomFrame frame;
    sh_bathroom_motion_step(&motion, 136, &frame);
    assert(frame.camera && !frame.finished && motion.tween[0] == 272 && motion.tween[1] == 136);
    assert(frame.position.vx == SH_Q12(21) + (-SH_Q12(1.5f) * 136) / SH_Q12(5));
    assert(frame.look.vx == SH_Q12(17) + (-SH_Q12(1.5f) * 272) / SH_Q12(5));
    assert(frame.look.vy == -SH_Q12(0.5f) + (-SH_Q12(1.5f) * 136) / SH_Q12(5));
    assert(frame.position.vz == SH_Q12(60) && frame.look.vz == SH_Q12(60));
    assert(frame.params.watch[0] == 3.0f && frame.params.watch[2] == SH_Q12_TO_FLOAT(SH_Q12(0.3f)));
    assert(!frame.params.camera[0] && !frame.params.watch[1]);
    const ShBathroomMotion before = motion;
    sh_bathroom_motion_step(&motion, 0, &frame);
    assert(!memcmp(&motion, &before, sizeof(motion)));
    const int deltas[] = {136, 68};
    for (unsigned i = 0; i < sizeof(deltas) / sizeof(deltas[0]); i++) {
        memset(&motion, 0, sizeof(motion));
        unsigned frames = 0;
        do {
            const int32_t elapsed = motion.elapsed;
            sh_bathroom_motion_step(&motion, deltas[i], &frame);
            assert(frame.camera == (elapsed > SH_Q12(1.0f)));
            assert(motion.rotation >= SH_Q12(-112.5f / 360.0f));
            assert(frame.finished == (motion.elapsed >= SH_Q12(8.5f)));
            assert(++frames < 1000);
        } while (!frame.finished);
        assert(motion.rotation == SH_Q12(-112.5f / 360.0f));
        assert(motion.tween[0] == SH_Q12(5.0f) && motion.tween[1] == SH_Q12(5.0f));
    }
    puts("PASS: bathroom camera tween call order, original parameters, pause, duration and door clamp at30/60Hz");
    return 0;
}
