#include "sh3ds/environment.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int32_t q19_12;
typedef int32_t q23_8;
typedef uint16_t q3_12;
typedef struct { u8 r, g, b, cd; } CVECTOR;
typedef struct { s32 vx, vy, vz; } VECTOR3;
typedef struct { int16_t vx, vy, vz, pad; } SVECTOR;
typedef struct { VECTOR3 field_0[2][1]; } s_WorldEnvWork_84;
typedef struct {
    union { s32 field_0; u8 field_00[4]; } flags;
    int16_t spotLightIntensity, worldLightIntensity, worldTintR, worldTintG, worldTintB;
    u8 field_E;
    q19_12 fogDistance;
    CVECTOR fogColor;
    u8 enableTintLightOverlap;
    CVECTOR pointLightTint, worldTint, field_21, field_25;
} s_MapEffectsInfo;
typedef struct {
    s_MapEffectsInfo effectsInfo;
    int16_t flashlightLensFlareIntensity, brightnessIntensity;
    q19_12 fogDistance;
} s_StructUnk3;

static struct {
    struct { s32 nearDistance, farDistance, depthShift; } fog;
    u8 fogRamp[128];
    int field_0;
    int field_20;
    u8 field_3;
    struct { VECTOR3 position; s_WorldEnvWork_84 field_38[3]; } light;
} g_WorldEnvWork;

#define Q12(x) ((s32)((x) * 4096))
#define Q12_SHIFT 12
#define Q12_ANGLE(x) ((int16_t)Q12((x) / 360.0f))
#define Q12_MULT(a, b) (((a) * (b)) >> 12)
#define FP_MULTIPLY(a, b, shift) (((a) * (b)) >> (shift))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define Q12_TO_Q8(x) ((x) >> 4)
#define CLAMP(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))
#define limitRange(x, low, high) ((x) = CLAMP((x), (low), (high)))

/* Math_MulFixed's signed 64-bit equivalent is included in its retail C source. */
static s32 Math_MulFixed(s32 a, s32 b, s32 shift)
{
    return (s32)(((int64_t)a * b) >> shift);
}

static s32 Lzc(u32 value)
{
    s32 count = 32;
    while (value) {
        value >>= 1;
        count--;
    }
    return count;
}

static void LoadAverageCol(const u8 *a, const u8 *b, s32 from, s32 to, u8 *out)
{
    for (int channel = 0; channel < 3; channel++) {
        out[channel] = (u8)((a[channel] * from + b[channel] * to) >> 12);
    }
}

#include "../build/environment_retail_reference.h"

static void state_equal(const ShEnvState *actual, const s_StructUnk3 *expected)
{
    assert(sizeof(*actual) == sizeof(*expected));
    const u8 *a = (const u8 *)actual;
    const u8 *b = (const u8 *)expected;
    assert(memcmp(a, b, 15) == 0);
    assert(memcmp(a + 16, b + 16, 25) == 0);
    assert(memcmp(a + 44, b + 44, 8) == 0);
}

static void transition_ranges_test(void)
{
    for (int reverse = 0; reverse < 2; reverse++) {
        for (int value = -4096; value < 101 * 4096; value += 137) {
            ShEnvironment env = {0};
            assert(sh_env_set(&env, 1, 1));
            assert(sh_env_transition_begin(&env, 6, 6));
            const int minimum = reverse ? 100 * 4096 : 0;
            const int maximum = reverse ? 0 : 100 * 4096;
            const int weight = retail_func_8003F6F0(value, minimum, maximum);
            s_StructUnk3 from;
            s_StructUnk3 to;
            memcpy(&from, &env.from[0], sizeof from);
            memcpy(&to, &env.to[0], sizeof to);
            s_StructUnk3 expected = to;
            retail_func_8003F838(&expected, &from, &to, weight);
            sh_env_transition_update(&env, value, minimum, maximum);
            state_equal(&env.channel[0], &expected);
        }
    }
}

static void light_test(void)
{
    int32_t origin[3];
    int32_t local_direction[3];
    sh_env_flashlight_local(origin, local_direction);
    for (unsigned preset = 0; sh_env_preset(preset); preset++) {
        ShEnvironment env = {0};
        assert(sh_env_set(&env, preset, preset));
        g_WorldEnvWork.field_0 = env.current.preset.flags[2];
        g_WorldEnvWork.field_20 = env.current.preset.world_light;
        g_WorldEnvWork.field_3 = env.current.preset.flags[1];
        memcpy(&g_WorldEnvWork.light.position, origin, sizeof origin);
        for (int angle = 0; angle < 4096; angle += 127) {
            int32_t direction[3] = {sh_fixed_sin(angle), local_direction[1], sh_fixed_cos(angle)};
            SVECTOR retail_direction = {(int16_t)direction[0], (int16_t)direction[1], (int16_t)direction[2], 0};
            ShEnvLight light;
            sh_env_light_build(&light, env.current.preset.spot_light, origin, direction);
            retail_func_80055648(env.current.preset.spot_light, &retail_direction);
            assert(memcmp(light.plane, g_WorldEnvWork.light.field_38, sizeof light.plane) == 0);
            for (int sample = -20480; sample <= 20480; sample += 257) {
                int32_t point[3] = {sample, sample / 3, -sample / 2};
                assert(sh_env_light_intensity(&env.current, &light, point) ==
                       retail_func_80055D78(point[0], point[1], point[2]));
            }
        }
    }
}

int main(void)
{
    assert(sizeof(ShEnvPreset) == sizeof(s_MapEffectsInfo));
    unsigned comparisons = 0;
    for (unsigned from = 0; sh_env_preset(from); from++) {
        for (unsigned to = 0; sh_env_preset(to); to++) {
            ShEnvironment env = {0};
            assert(sh_env_set(&env, from, to));
            s_MapEffectsInfo a;
            s_MapEffectsInfo b;
            memcpy(&a, sh_env_preset(from), sizeof a);
            memcpy(&b, sh_env_preset(to), sizeof b);
            s_StructUnk3 a_state = {0};
            s_StructUnk3 b_state = {0};
            retail_Gfx_FogParametersSet(&a_state, &a);
            retail_Gfx_FogParametersSet(&b_state, &b);
            state_equal(&env.channel[0], &a_state);
            state_equal(&env.channel[1], &b_state);
            for (int weight = 0; weight <= 4096; weight += 32) {
                ShEnvState actual;
                s_StructUnk3 expected = b_state;
                sh_env_mix(&actual, &env.channel[0], &env.channel[1], weight);
                retail_func_8003F838(&expected, &a_state, &b_state, weight);
                state_equal(&actual, &expected);
                comparisons++;
            }
        }
        ShEnvFog fog;
        const int32_t distance = sh_env_preset(from)->fog_distance;
        assert(sh_env_fog_build(&fog, distance));
        retail_WorldEnv_FogDistanceSet(distance, distance + 4096);
        assert(fog.depth_shift == g_WorldEnvWork.fog.depthShift);
        assert(memcmp(fog.ramp, g_WorldEnvWork.fogRamp, sizeof fog.ramp) == 0);
        for (int depth = -4096; depth < 120 * 4096; depth += 137) {
            assert(sh_env_fog_alpha(&fog, depth) == retail_func_800559A8(depth));
        }
    }
    ShEnvironment transition = {0};
    assert(sh_env_set(&transition, 1, 1));
    assert(sh_env_transition_begin(&transition, 6, 6));
    sh_env_transition_update(&transition, SH_Q12(40.0f), 0, SH_Q12(100.0f));
    assert(transition.transitioning);
    sh_env_transition_update(&transition, SH_Q12(100.0f), 0, SH_Q12(100.0f));
    assert(!transition.transitioning);
    assert(transition.current.preset.world_light == sh_env_preset(6)->world_light);
    transition_ranges_test();
    light_test();
    printf("PASS: %u environment mixes and all preset fog ramps/depth samples match extracted retail functions\n", comparisons);
    return 0;
}
