#include "sh3ds/path.h"
#include "sh3ds/fixed.h"
#include <assert.h>
#include <stdio.h>

static void run(int refresh)
{
    ShPathPlayer player = {0};
    player.model.keyframe = player.extra_model.keyframe = 503;
    player.model.time = player.extra_model.time = SH_Q12(503);
    assert(!sh_path_alley_animation_start(&player, 0));
    assert(sh_path_alley_animation_start(&player, 84));
    assert(player.model.animation_status == 106);
    bool first = false, middle = false;
    int frames = 0;
    const int32_t delta = SH_Q12(1.0f / refresh);
    while (!sh_path_school_animation_done(&player) && frames < refresh * 3) {
        ShAnimPoseTiming pose;
        assert(sh_path_animation_timing(&player, delta, 0, 0, &pose));
        if (player.model.animation_status == 107) {
            assert(player.model.keyframe >= 973 && player.model.keyframe <= 993);
            first |= player.model.keyframe == 973;
            middle |= player.model.keyframe >= 983;
        }
        frames++;
    }
    assert(first && middle && player.model.keyframe == 993);
    assert(frames > refresh && frames < refresh * 2);
    for (int i = 0; i < refresh; i++) {
        ShAnimPoseTiming pose;
        assert(sh_path_animation_timing(&player, delta, 0, 0, &pose));
        assert(player.model.keyframe == 993 && sh_path_school_animation_done(&player));
    }
}

int main(void)
{
    run(30);
    run(60);
    puts("PASS: original torch blend, 973..993 playback, match threshold and completion at 30/60 Hz");
    return 0;
}
