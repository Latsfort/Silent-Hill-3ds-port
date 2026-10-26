#include "sh3ds/path.h"
#include "sh3ds/fixed.h"

#include <stdlib.h>

static int16_t path_angle(int32_t value)
{
    return (int16_t)(value & 4095);
}

/* player_control.c Math_ShortestAngleGet, including the half-turn tie. */
static void path_shortest(int16_t from, int16_t to, int16_t *delta)
{
    if (to > from) {
        *delta = to - from < 2048 ? to - from : to - 4096 - from;
    } else {
        *delta = from - to < 2048 ? to - from : to - (from - 4096);
    }
}

/* fixed_point.h Q12_2D_DISTANCE_SQR uses Q8 intermediates. */
static uint32_t path_distance_squared(ShPathPoint from, ShPathPoint to)
{
    const int32_t dx = (to.vx - from.vx) >> 4;
    const int32_t dz = (to.vz - from.vz) >> 4;
    return (uint32_t)(dx * dx + dz * dz);
}

#define PATH_DISTANCE_SQR(a, b) path_distance_squared((a), (b))
#define PATH_TO_Q8(value) ((value) >> 4)
#define PATH_TIMESTEP(dt, value) (((value) * (dt)) / SH_Q12(1.0f / 30.0f))

/* player.h Player_ExtraStateSet. */
void sh_path_state_set(ShPathPlayer *player, int state)
{
    player->state = state;
    player->model.state_step = player->model.control_state = 0;
    player->extra_model.state_step = player->extra_model.control_state = 0;
    player->upper_body_state = player->lower_body_state = 0;
}

/* model.h Model_AnimStatusSet. */
static void path_model_status(ShPathModel *model, int animation, bool active)
{
    if (!model->state_step) {
        model->animation_status = animation * 2 + active;
        model->state_step++;
    }
}

static void path_animation_reset(ShPathPlayer *player)
{
    if (player->animation_reset) {
        sh_path_state_set(player, 52);
        player->animation_reset = false;
    }
}

#include "player_path_steps.h"
#include "path_locomotion.h"
