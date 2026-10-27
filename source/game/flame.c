#include "sh3ds/flame.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"

/* Math_AngleNormalize / Q12_ANGLE_NORM_S. */
static int16_t flame_angle(int32_t angle) { return (int16_t)(angle & 4095); }

#include "flame_steps.h"

bool sh_flame_frame(ShFlame *flame, bool tick, ShFlameProject project, void *user)
{
    if (!flame || !project) { return false; }
    flame->draw_counter = flame->counter + (tick ? 8u : 0u);
    flame->visible = flame_frame(flame, tick, project, user, &flame->sprite);
    return flame->visible;
}
