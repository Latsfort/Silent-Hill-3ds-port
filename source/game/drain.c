#include "sh3ds/drain.h"
#include "sh3ds/fixed.h"

#include "drain_camera_data.h"
#include "drain_camera_steps.h"

/* Math_Vector2MagCalcSafeQ6. */
static int32_t drain_magnitude_q6(int32_t x, int32_t z)
{
    x >>= 6;
    z >>= 6;
    return (int32_t)sh_fixed_sqrt0((uint32_t)(x * x + z * z)) * 64;
}

static int32_t drain_max(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

#include "drain_light_steps.h"

/* world_effects.c func_8003F4DC, LensFlareType_Custom. */
void sh_drain_light_direction(const ShDrainLight *light, int32_t direction[3])
{
    direction[1] = -sh_fixed_sin(light->rotation.vx);
    const int32_t horizontal = sh_fixed_cos(light->rotation.vx);
    direction[2] = sh_q12_mul(horizontal, sh_fixed_cos(light->rotation.vy));
    direction[0] = sh_q12_mul(horizontal, sh_fixed_sin(light->rotation.vy));
}

static int valve_angle_signed(int value)
{
    value &= 4095;
    return value > 2048 ? value - 4096 : value;
}

static int32_t valve_q12_div(int32_t numerator, int32_t denominator)
{
    return (int32_t)((int64_t)numerator * 4096 / denominator);
}

static int32_t drain_magnitude3_q6(int32_t x, int32_t y, int32_t z)
{
    x >>= 6; y >>= 6; z >>= 6;
    return (int32_t)sh_fixed_sqrt0((uint32_t)(x * x + y * y + z * z)) * 64;
}

#include "drain_valve_steps.h"

/* Compose the rendered Harry yaw/translation with the model-space hand hierarchy. */
void sh_drain_valve_world_bone(const ShCharaBone *model_bone, const int32_t position[3],
    int yaw, ShCharaBone *world_bone)
{
    const int32_t sine = sh_fixed_sin(yaw), cosine = sh_fixed_cos(yaw);
    ShCharaBone result;
    for (int column = 0; column < 3; column++) {
        result.rotation[0][column] = (int32_t)(((int64_t)cosine * model_bone->rotation[0][column] +
            (int64_t)sine * model_bone->rotation[2][column]) >> 12);
        result.rotation[1][column] = model_bone->rotation[1][column];
        result.rotation[2][column] = (int32_t)((-(int64_t)sine * model_bone->rotation[0][column] +
            (int64_t)cosine * model_bone->rotation[2][column]) >> 12);
    }
    result.translation[0] = (position[0] >> 4) + (int32_t)(((int64_t)cosine * model_bone->translation[0] +
        (int64_t)sine * model_bone->translation[2]) >> 12);
    result.translation[1] = (position[1] >> 4) + model_bone->translation[1];
    result.translation[2] = (position[2] >> 4) + (int32_t)((-(int64_t)sine * model_bone->translation[0] +
        (int64_t)cosine * model_bone->translation[2]) >> 12);
    *world_bone = result;
}

/* DrainageValveInteract rotates g_WorldObject5 around its PS1 X axis. */
void sh_drain_valve_transform_vertices(int16_t rotation, const float pivot[3],
    const ShMapVertex *source, ShMapVertex *destination, size_t count)
{
    /* WorldObjects_Add stores X as Q10, then Draw restores Q12. */
    const int angle = (rotation >> 2) * 4;
    const int32_t sine = sh_fixed_sin(angle), cosine = sh_fixed_cos(angle);
    int32_t origin[3];
    for (int axis = 0; axis < 3; axis++) { origin[axis] = SH_Q12(pivot[axis]) >> 4; }
    for (size_t vertex = 0; vertex < count; vertex++) {
        int32_t local[3];
        for (int axis = 0; axis < 3; axis++) {
            local[axis] = (int32_t)(source[vertex].pos[axis] * (axis ? -256.0f : 256.0f)) - (int32_t)(pivot[axis] * 256.0f);
        }
        const int32_t y = (int32_t)(((int64_t)cosine * local[1] - (int64_t)sine * local[2]) >> 12);
        const int32_t z = (int32_t)(((int64_t)sine * local[1] + (int64_t)cosine * local[2]) >> 12);
        destination[vertex] = source[vertex];
        destination[vertex].pos[0] = (origin[0] + local[0]) / 256.0f;
        destination[vertex].pos[1] = -(origin[1] + y) / 256.0f;
        destination[vertex].pos[2] = -(origin[2] + z) / 256.0f;
    }
}
