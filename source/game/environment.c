#include "sh3ds/environment.h"

#include <stddef.h>

#include "environment_data.h"

static int32_t clamp_q12(int32_t value)
{
    return value < 0 ? 0 : (value > SH_Q12_ONE ? SH_Q12_ONE : value);
}

/* Math_WeightedAverageGet: each fixed-point multiply is truncated separately. */
static int32_t average(int32_t from, int32_t to, int32_t weight)
{
    return sh_q12_mul(from, SH_Q12_ONE - weight) + sh_q12_mul(to, weight);
}

/* LoadAverageCol, as used by func_8003FCB0 / func_8003FD38 / func_8003FE04. */
static void color_average(uint8_t out[4], const uint8_t from[4], const uint8_t to[4], int32_t weight)
{
    for (int channel = 0; channel < 3; channel++) {
        out[channel] = (uint8_t)(((int32_t)from[channel] * (SH_Q12_ONE - weight) +
                                  (int32_t)to[channel] * weight) >> SH_Q12_SHIFT);
    }
}

const ShEnvPreset *sh_env_preset(unsigned index)
{
    return index < sizeof(s_env_presets) / sizeof(s_env_presets[0]) ? &s_env_presets[index] : NULL;
}

/* Gfx_FogParametersSet. */
static void state_init(ShEnvState *state, const ShEnvPreset *preset)
{
    state->preset = *preset;
    state->brightness = preset->flags[0] & SH_ENV_ENABLEBRIGHTNESS ? SH_Q12_ONE : 0;
    state->flare = preset->flags[0] & SH_ENV_ENABLELENSFLARE ? SH_Q12_ONE : 0;
    state->fog_distance = preset->fog_mode == 2 ? 0 : preset->fog_distance;
}

/* func_8003FD38. */
static void fog_average(ShEnvState *out, const ShEnvState *from, const ShEnvState *to,
                        int32_t weight0, int32_t weight1, int32_t alpha)
{
    out->brightness = from->brightness != to->brightness ? average(from->brightness, to->brightness, weight0)
                                                        : to->brightness;
    out->fog_distance = average(from->fog_distance, to->fog_distance, weight0);
    out->preset.fog_distance = average(from->preset.fog_distance, to->preset.fog_distance, weight1);
    out->preset.world_light = average(from->preset.world_light, to->preset.world_light, weight0);
    color_average(out->preset.fog_color, from->preset.fog_color, to->preset.fog_color, alpha);
}

/* func_8003F838, world_effects.c. */
void sh_env_mix(ShEnvState *out, const ShEnvState *from, const ShEnvState *to, sh_q19_12 weight)
{
    weight = clamp_q12(weight);
    int32_t weight0 = clamp_q12(weight * 2);
    const int32_t weight1 = clamp_q12((weight - SH_Q12(0.5f)) * 2);
    ShEnvState result = *to;
    ShEnvPreset *preset = &result.preset;
    const ShEnvPreset *a = &from->preset;
    const ShEnvPreset *b = &to->preset;

    preset->flags[0] = weight < SH_Q12(0.5f) ? a->flags[0] : b->flags[0];
    color_average(preset->snow_color, a->snow_color, b->snow_color, weight);
    color_average(preset->rain_color, a->rain_color, b->rain_color, weight);
    result.flare = from->flare == 0 ? average(0, to->flare, weight1) : average(from->flare, to->flare, weight0);

    if (a->flags[0] & SH_ENV_DARKENVIRONMENT) {
        preset->flags[1] = average(a->flags[1], b->flags[1], b->flags[0] & SH_ENV_DARKENVIRONMENT ? weight : weight1);
    } else {
        preset->flags[1] = average(a->flags[1], b->flags[1], b->flags[0] & SH_ENV_DARKENVIRONMENT ? weight0 : weight);
    }

    if (a->fog_mode == 0) {
        preset->fog_mode = b->fog_mode;
        if (b->fog_mode != 0) {
            fog_average(&result, from, to, weight, weight0, weight1);
        } else {
            fog_average(&result, from, to, weight, weight, weight);
        }
    } else if (b->fog_mode == 0) {
        preset->fog_mode = weight1 >= SH_Q12_ONE ? b->fog_mode : a->fog_mode;
        fog_average(&result, from, to, weight, weight1, weight0);
    } else {
        preset->fog_mode = b->fog_mode;
        fog_average(&result, from, to, weight, weight, weight);
    }

    for (int channel = 0; channel < 3; channel++) {
        preset->world_tint[channel] = average(a->world_tint[channel], b->world_tint[channel], weight);
    }

    if (a->flags[2] == 1 && b->flags[2] == 2) {
        if (weight < SH_Q12(5.0f / 6.0f)) {
            const int32_t weight2 = clamp_q12(sh_q12_mul(weight, SH_Q12(1.2f)));
            preset->flags[2] = a->flags[2];
            preset->spot_light = average(a->spot_light, 0, weight2);
        } else {
            const int32_t weight2 = clamp_q12((weight - SH_Q12(5.0f / 6.0f)) * 6);
            preset->flags[2] = b->flags[2];
            preset->spot_light = average(0, b->spot_light, weight2);
        }
    } else if (a->flags[2] == 2 && b->flags[2] == 1) {
        if (weight < SH_Q12(1.0f / 6.0f)) {
            const int32_t weight2 = clamp_q12(weight * 6);
            preset->flags[2] = a->flags[2];
            preset->spot_light = average(a->spot_light, 0, weight2);
        } else {
            const int32_t weight2 = clamp_q12(sh_q12_mul(weight - SH_Q12(1.0f / 6.0f), SH_Q12(1.2f)));
            preset->flags[2] = b->flags[2];
            preset->spot_light = average(0, b->spot_light, weight2);
        }
    } else {
        preset->flags[2] = a->flags[2] != 0 && b->flags[2] == 0 && weight < SH_Q12_ONE ? a->flags[2] : b->flags[2];
        preset->spot_light = average(a->spot_light, b->spot_light, weight);
    }

    const int32_t tint_weight = !a->overlap && b->overlap ? weight1 : weight;
    color_average(preset->point_tint, a->point_tint, b->point_tint, tint_weight);
    color_average(preset->subtract_tint, a->subtract_tint, b->subtract_tint, tint_weight);
    preset->overlap = preset->point_tint[0] || preset->point_tint[1] || preset->point_tint[2] ||
                      preset->subtract_tint[0] || preset->subtract_tint[1] || preset->subtract_tint[2];
    *out = result;
}

/* Gfx_MapEnvStepUpdate / Gfx_MapEnvSet. */
bool sh_env_set(ShEnvironment *env, unsigned off, unsigned on)
{
    const ShEnvPreset *a = sh_env_preset(off);
    const ShEnvPreset *b = sh_env_preset(on);
    if (!env || !a || !b) {
        return false;
    }
    state_init(&env->channel[0], a);
    state_init(&env->channel[1], b);
    env->active = true;
    env->transitioning = false;
    sh_env_mix(&env->current, &env->channel[0], &env->channel[1], env->flashlight);
    return true;
}

/* Gfx_MapEnvStepUpdate: retain the current two channels as the transition origins. */
bool sh_env_transition_begin(ShEnvironment *env, unsigned off, unsigned on)
{
    const ShEnvPreset *a = sh_env_preset(off);
    const ShEnvPreset *b = sh_env_preset(on);
    if (!env || !env->active || !a || !b) {
        return false;
    }
    env->from[0] = env->channel[0];
    env->from[1] = env->channel[1];
    state_init(&env->to[0], a);
    state_init(&env->to[1], b);
    env->transitioning = true;
    return true;
}

/* Gfx_EffectsUpdate / func_8003F6F0, PrimitiveType_S32 transition. */
void sh_env_transition_update(ShEnvironment *env, int32_t value, int32_t minimum, int32_t maximum)
{
    if (!env || !env->transitioning) {
        return;
    }
    int32_t weight = SH_Q12_ONE;
    if (minimum != maximum) {
        const int32_t lower = minimum < maximum ? minimum : maximum;
        const int32_t upper = minimum < maximum ? maximum : minimum;
        value = value < lower ? lower : (value > upper ? upper : value);
        const int32_t range = (int32_t)((uint32_t)maximum - (uint32_t)minimum);
        int bits = 0;
        for (uint32_t remaining = (uint32_t)range; remaining; remaining >>= 1) {
            bits++;
        }
        int shift = bits + SH_Q12_SHIFT >= 31 ? bits - 19 : 0;
        shift = shift < 0 ? 0 : (shift > SH_Q12_SHIFT ? SH_Q12_SHIFT : shift);
        const int32_t numerator = (int32_t)(((uint32_t)value - (uint32_t)minimum) << (SH_Q12_SHIFT - shift));
        weight = numerator / (range >> shift);
    }
    weight = clamp_q12(weight);
    for (int channel = 0; channel < 2; channel++) {
        sh_env_mix(&env->channel[channel], &env->from[channel], &env->to[channel], weight);
    }
    sh_env_mix(&env->current, &env->channel[0], &env->channel[1], env->flashlight);
    if (weight == SH_Q12_ONE) {
        env->transitioning = false;
    }
}

/* Gfx_EffectsUpdate: flashlightIntensity +=/-= deltaTime * 4. */
void sh_env_update(ShEnvironment *env, bool flashlight_on, float dt)
{
    if (!env || !env->active) {
        return;
    }
    const int32_t change = SH_Q12(dt) * 4;
    env->flashlight = clamp_q12(env->flashlight + (flashlight_on ? change : -change));
    sh_env_mix(&env->current, &env->channel[0], &env->channel[1], env->flashlight);
}

/* WorldEnv_FogDistanceSet, bodyprog_80055028.c. */
bool sh_env_fog_build(ShEnvFog *fog, sh_q19_12 near_distance)
{
    const int32_t near = near_distance >> 4;
    if (!fog || near <= 0) {
        return false;
    }
    int shift = 0;
    for (uint32_t value = (uint32_t)near - 1; value; value >>= 1) {
        shift++;
    }
    if (shift < 7 || shift > 25) {
        return false;
    }
    fog->depth_shift = shift;
    const int32_t increment = (int32_t)((16u << (shift + 1)) / (uint32_t)near);
    int index = 0;
    for (int32_t value = 0; value < SH_Q12_ONE && index < 128; value += increment, index++) {
        const int curve_index = value >> 8;
        int denominator;
        int remainder;
        int from;
        int to;
        if (curve_index == 0) {
            denominator = 128;
            remainder = value & 127;
            from = value & 128 ? 0 : s_env_fog_curve[0];
            to = value & 128 ? s_env_fog_curve[0] : s_env_fog_curve[1];
        } else {
            denominator = 256;
            remainder = value & 255;
            from = s_env_fog_curve[curve_index];
            to = s_env_fog_curve[curve_index + 1];
        }
        const int color = (from * (denominator - remainder) + to * remainder) / denominator;
        fog->ramp[index] = (uint8_t)(color < 0 ? 0 : (color > 255 ? 255 : color));
    }
    for (; index < 128; index++) {
        fog->ramp[index] = 255;
    }
    return true;
}

/* func_800559A8. */
sh_q19_12 sh_env_fog_alpha(const ShEnvFog *fog, sh_q19_12 distance)
{
    if (!fog || fog->depth_shift < 7 || fog->depth_shift > 25 || distance < 0) {
        return 0;
    }
    const int span = (1 << fog->depth_shift) >> 7;
    const int32_t depth = distance >> 4;
    const int index = depth / span;
    if (index >= 128) {
        return SH_Q12_ONE;
    }
    const int a = fog->ramp[index] * 16;
    const int b = index == 127 ? SH_Q12_ONE : fog->ramp[index + 1] * 16;
    return a + ((b - a) * (depth % span)) / span;
}

int sh_env_flashlight_bone(void)
{
    return SH_ENV_FLASHLIGHT_BONE;
}

/* Game_FlashlightAttributesFix and func_8003F4DC, LensFlareType_Custom. */
void sh_env_flashlight_local(int32_t position[3], int32_t direction[3])
{
    for (int axis = 0; axis < 3; axis++) {
        position[axis] = s_env_flashlight_offset[axis];
    }
    direction[1] = -sh_fixed_sin(s_env_flashlight_angles[0]);
    const int32_t horizontal = sh_fixed_cos(s_env_flashlight_angles[0]);
    direction[2] = sh_q12_mul(horizontal, sh_fixed_cos(s_env_flashlight_angles[1]));
    direction[0] = sh_q12_mul(horizontal, sh_fixed_sin(s_env_flashlight_angles[1]));
}

/* func_80055648, bodyprog_80055028.c. */
void sh_env_light_build(ShEnvLight *light, int32_t intensity, const int32_t position[3], const int32_t direction[3])
{
    for (int axis = 0; axis < 3; axis++) {
        light->position[axis] = position[axis];
        const int32_t positive = ((direction[axis] + SH_Q12(1.3f)) * SH_Q12_ONE) / SH_Q12(2.3f);
        const int32_t negative = ((SH_Q12(1.3f) - direction[axis]) * SH_Q12_ONE) / SH_Q12(2.3f);
        const int32_t positive_scale = SH_Q12(1024.0f) / positive;
        const int32_t negative_scale = SH_Q12(1024.0f) / negative;
        light->plane[axis][0][1] = light->plane[axis][1][1] = sh_q12_mul(s_env_light_shape[1], intensity);
        light->plane[axis][0][0] = sh_q12_mul(s_env_light_shape[0], positive);
        light->plane[axis][1][0] = sh_q12_mul(s_env_light_shape[0], negative);
        light->plane[axis][0][2] = sh_q12_mul((s_env_light_shape[2] * positive_scale) >> 10, intensity);
        light->plane[axis][1][2] = sh_q12_mul((s_env_light_shape[2] * negative_scale) >> 10, intensity);
    }
}

/* func_80055D78, bodyprog_80055028.c. */
uint8_t sh_env_light_intensity(const ShEnvState *env, const ShEnvLight *light, const int32_t position[3])
{
    int32_t value = 0;
    if (env->preset.flags[2] != 0 && light) {
        value = 255;
        for (int axis = 0; axis < 3; axis++) {
            int32_t distance = (position[axis] >> 4) - (light->position[axis] >> 4);
            const int side = distance < 0;
            if (side) {
                distance = -distance;
            }
            const int32_t *plane = light->plane[axis][side];
            const int32_t candidate = plane[1] + sh_q12_mul(plane[2], distance - plane[0]);
            if (candidate < value) {
                value = candidate;
            }
        }
    }
    value += env->preset.world_light >> 5;
    return (uint8_t)(value < 0 ? 0 : (value > env->preset.flags[1] ? env->preset.flags[1] : value));
}
