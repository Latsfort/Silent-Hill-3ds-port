#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sh3ds/fixed.h"

typedef struct ShEnvPreset {
    uint8_t flags[4];
    int16_t spot_light;
    int16_t world_light;
    int16_t world_tint[3];
    uint8_t fog_mode;
    int32_t fog_distance;
    uint8_t fog_color[4];
    uint8_t overlap;
    uint8_t point_tint[4];
    uint8_t subtract_tint[4];
    uint8_t snow_color[4];
    uint8_t rain_color[4];
} ShEnvPreset;

typedef struct ShEnvState {
    ShEnvPreset preset;
    int16_t flare;
    int16_t brightness;
    int32_t fog_distance;
} ShEnvState;

typedef struct ShEnvironment {
    ShEnvState channel[2];
    ShEnvState from[2];
    ShEnvState to[2];
    ShEnvState current;
    int32_t flashlight;
    bool active;
    bool transitioning;
} ShEnvironment;

typedef struct ShEnvFog {
    uint8_t ramp[128];
    int depth_shift;
} ShEnvFog;

typedef struct ShEnvLight {
    int32_t position[3]; /* PS1 world, Q12. */
    int32_t plane[3][2][3]; /* Axis, positive/negative, origin/value/slope. */
} ShEnvLight;

const ShEnvPreset *sh_env_preset(unsigned index);
void sh_env_mix(ShEnvState *out, const ShEnvState *from, const ShEnvState *to, sh_q19_12 weight);
bool sh_env_set(ShEnvironment *env, unsigned off, unsigned on);
bool sh_env_transition_begin(ShEnvironment *env, unsigned off, unsigned on);
void sh_env_transition_update(ShEnvironment *env, int32_t value, int32_t minimum, int32_t maximum);
void sh_env_update(ShEnvironment *env, bool flashlight_on, float dt);
bool sh_env_fog_build(ShEnvFog *fog, sh_q19_12 near_distance);
sh_q19_12 sh_env_fog_alpha(const ShEnvFog *fog, sh_q19_12 distance);
int sh_env_flashlight_bone(void);
void sh_env_flashlight_local(int32_t position[3], int32_t direction[3]);
void sh_env_light_build(ShEnvLight *light, int32_t intensity, const int32_t position[3], const int32_t direction[3]);
uint8_t sh_env_light_intensity(const ShEnvState *env, const ShEnvLight *light, const int32_t position[3]);
