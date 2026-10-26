#include "sh3ds/smoke.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef int32_t s32;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t q19_12;
#define ARRAY_SIZE(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define Q12_SHIFT 12
#define Q12(a) SH_Q12(a)
#define Q12_ANGLE(a) SH_Q12((a) / 360.0f)
#define Q12_MULT(a, b) (((a) * (b)) >> 12)
#define Q12_MULT_PRECISE(a, b) sh_q12_mul((a), (b))
#define Q12_DIV(a, b) (((a) * 4096) / (b))
#define TO_FIXED(a, shift) ((int32_t)((a) * (1 << (shift))))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP_LOW(a, b) MAX(a, b)
#define SQUARE(a) ((a) * (a))
#define Rng_GenerateUInt(low, high) ((int32_t)((uint32_t)ref_rand16() % (((high) - (low)) + 1)) + (low))

static ShSmokeConfig ref_config;
static ShSmokeParticle ref_particles[SH_SMOKE_CAPACITY];
static uint32_t ref_seed, ref_flags;
static int32_t ref_delta;
static unsigned zero_divisors;

static int32_t ref_remainder(int32_t value, int32_t divisor)
{
    if (!divisor) { zero_divisors++; return value; }
    return value % divisor;
}

#include "../source/game/school_smoke_data.h"
#include "school_smoke_reference.h"

int main(void)
{
    unsigned comparisons = 0;
    for (unsigned seed = 0; seed < 32; seed++) {
        ShSmoke smoke = {0};
        smoke.particles[2] = (ShSmokeParticle){.kind = 7, .age = 123};
        memcpy(ref_particles, smoke.particles, sizeof(ref_particles));
        ref_config = s_school_acid_smoke;
        ref_seed = seed;
        ref_flags = 0;
        sh_rng_set_seed(seed);
        sh_smoke_begin_acid(&smoke);
        ref_begin();
        assert(ref_flags == 2);
        assert(memcmp(smoke.particles, ref_particles, sizeof(ref_particles)) == 0);
        for (int frame = 0; frame < 600; frame++) {
            ref_delta = SH_Q12(1.0f / 30.0f);
            ref_global_tick();
            for (int i = 0; i < SH_SMOKE_CAPACITY; i++) {
                ref_particles[i].visible = false;
                if (ref_particles[i].kind == 13) { ref_delay(i); }
                else if (ref_particles[i].kind == 14) { ref_particles[i].visible = ref_update(i); }
            }
            sh_smoke_step(&smoke, ref_delta);
            assert(ref_seed == sh_rng_get_seed());
            assert(memcmp(&ref_config, &smoke.config, sizeof(ref_config)) == 0);
            assert(memcmp(ref_particles, smoke.particles, sizeof(ref_particles)) == 0);
            comparisons += SH_SMOKE_CAPACITY;
        }
        sh_smoke_stop(&smoke);
        assert(!smoke.active && smoke.particles[2].kind == 7 && smoke.particles[2].age == 123);
        for (int i = 0; i < SH_SMOKE_CAPACITY; i++) {
            assert(smoke.particles[i].kind != 13 && smoke.particles[i].kind != 14);
        }
    }
    assert(zero_divisors > 0);
    ShSmoke thirty = {0}, sixty = {0};
    sh_rng_set_seed(0);
    sh_smoke_begin_acid(&thirty);
    for (int i = 0; i < 180; i++) { sh_smoke_update(&thirty, 1.0f / 30.0f); }
    const uint32_t seed = sh_rng_get_seed();
    sh_rng_set_seed(0);
    sh_smoke_begin_acid(&sixty);
    for (int i = 0; i < 360; i++) { sh_smoke_update(&sixty, 1.0f / 60.0f); }
    assert(seed == sh_rng_get_seed());
    assert(memcmp(&thirty.config, &sixty.config, sizeof(thirty.config)) == 0);
    assert(memcmp(thirty.particles, sixty.particles, sizeof(thirty.particles)) == 0);
    printf("PASS: %u particle states match retail C, including %u MIPS zero-divisor cases; identical 30/60 fps simulation\n", comparisons, zero_divisors);
    return 0;
}
