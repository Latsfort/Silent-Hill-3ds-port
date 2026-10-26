#include "sh3ds/alley.h"

#include <assert.h>
#include <stdio.h>

bool sh_input_down(const ShInputState *input, uint32_t key)
{
    return input && (input->down & key) != 0;
}

int main(void)
{
    ShAlley alley = {0};
    for (int stage = SH_ALLEY_OPENING; stage <= SH_ALLEY_TO_CAFE; stage++) {
        alley.stage = (ShAlleyStage)stage;
        alley.skip_collision = true;
        alley.controls_locked = false;
        assert(!sh_alley_skip_collision(&alley));
    }
    /* IntoTheAlley's scripted gate crossing must remain available. */
    alley.stage = SH_ALLEY_INTO;
    alley.controls_locked = true;
    assert(sh_alley_skip_collision(&alley));
    alley.skip_collision = false;
    assert(!sh_alley_skip_collision(&alley));
    assert(!sh_alley_skip_collision(NULL));
    ShSavegame save = {0};
    ShInputState input = {0};
    float x = 0.0f, y = 0.0f, z = 0.0f, yaw = 0.0f;
    alley.stage = SH_ALLEY_FIGHT;
    alley.controls_locked = false;
    alley.skip_collision = true;
    alley.grey_count = 1;
    alley.grey[0].alive = true;
    alley.grey[0].pos[0] = 10.0f;
    assert(!sh_alley_update(&alley, &save, &x, &y, &z, &yaw, &input, 1.0f / 30.0f));
    assert(alley.stage == SH_ALLEY_FIGHT && !sh_alley_skip_collision(&alley));
    assert(!alley.skip_collision);
    puts("PASS: world collision required in every free-control alley state; scripted gate crossing preserved");
    return 0;
}
