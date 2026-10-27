#include "sh3ds/alley_flicker.h"
#include "sh3ds/rng.h"

#define FLICKER_MULT(a, b, shift) (((a) * (b)) >> (shift))
#define FLICKER_MAX(a, b) ((a) > (b) ? (a) : (b))

/* main/rng.h Rng_GenerateUInt and fixed_point.h Q12_FRACT. */
static int32_t flicker_random(uint32_t low, uint32_t high)
{
    return (int32_t)((uint32_t)sh_rng_rand16() % (high - low + 1)) + (int32_t)low;
}
static int32_t flicker_fraction(int32_t value) { return value & 4095; }

#include "alley_flicker_data.h"

void sh_alley_flicker_init(ShAlleyFlicker *state) { *state = s_flicker_initial; }

void sh_alley_flicker_update(ShAlleyFlicker *state, ShEnvironment *environment, int32_t delta)
{
    flicker_step(state, environment, delta);
    sh_env_transition_update(environment, state->value, 0, SH_Q12(100.0f));
}
