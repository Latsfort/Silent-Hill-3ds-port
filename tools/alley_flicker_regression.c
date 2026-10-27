#include "sh3ds/alley_flicker.h"
#include "sh3ds/flame_data.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void run(int32_t delta)
{
    ShAlleyFlicker state;
    ShEnvironment environment = {0};
    sh_alley_flicker_init(&state);
    assert(state.value == -1);
    assert(sh_env_set(&environment, 10, 10));
    sh_rng_set_seed(1);
    sh_alley_flicker_update(&state, &environment, 0);
    assert(state.value == SH_Q12(4.0f) && sh_rng_get_seed() == 1);
    assert(environment.transitioning);
    int32_t minimum = INT_MAX, maximum = INT_MIN;
    for (int frame = 0; frame < 5000; frame++) {
        sh_alley_flicker_update(&state, &environment, delta);
        assert(state.value >= 0 && state.value < SH_Q12(12.0f));
        assert(state.phase.field_0 >= 0 && state.phase.field_0 < SH_Q12(2.0f));
        assert(state.phase.field_2 >= 0 && state.phase.field_2 < SH_Q12(2.0f));
        assert(state.rate.field_0 >= 32 && state.rate.field_0 <= 95);
        assert(state.rate.field_1 >= 32 && state.rate.field_1 <= 63);
        assert(state.rate.field_2 >= 32 && state.rate.field_2 <= 95);
        assert(environment.current.preset.flags[0] & SH_FLAME_ENV_FLAG);
        assert(environment.current.preset.spot_light >= 0 &&
            environment.current.preset.spot_light <= sh_env_preset(10)->spot_light);
        minimum = state.value < minimum ? state.value : minimum;
        maximum = state.value > maximum ? state.value : maximum;
    }
    assert(maximum > minimum);
}

int main(void)
{
    run(136);
    run(68);
    ShAlleyFlicker state;
    ShEnvironment environment = {0};
    sh_alley_flicker_init(&state);
    assert(sh_env_set(&environment, 10, 10));
    state.phase.field_0 = state.phase.field_2 = SH_Q12(1.0f);
    sh_rng_set_seed(123);
    for (int i = 0; i < 3; i++) { sh_rng_rand16(); }
    const uint32_t expected = sh_rng_get_seed();
    sh_rng_set_seed(123);
    sh_alley_flicker_update(&state, &environment, 0);
    assert(sh_rng_get_seed() == expected);
    assert(state.phase.field_0 == 0 && state.phase.field_2 == 0);
    puts("PASS: retail flicker initialization, original three RNG requests, light transition and signed16 phases at 30/60 Hz");
    return 0;
}
