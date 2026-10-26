#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fixed-point helpers matching bodyprog conventions (Q*.12).
 * Pure maths — no GPU / PsyQ.
 */

#define SH_Q12_SHIFT 12
#define SH_Q12_ONE (1 << SH_Q12_SHIFT)

typedef int32_t sh_q19_12; /* signed Q19.12 used for positions / health */
typedef int16_t sh_q3_12;  /* angles, often packed */

#define SH_Q12(f) ((sh_q19_12)((float)(f) * (float)SH_Q12_ONE))
#define SH_Q12_TO_FLOAT(v) ((float)(v) / (float)SH_Q12_ONE)
#define SH_FP_FROM(v, shift) ((int32_t)(v) >> (shift))
#define SH_FP_TO(v, shift) ((int32_t)(v) << (shift))

sh_q19_12 sh_fixed_sin(sh_q19_12 angle);
sh_q19_12 sh_fixed_cos(sh_q19_12 angle);
/* PsyQ ratan2, with component magnitudes <= INT32_MAX. */
sh_q19_12 sh_fixed_atan2(int32_t y, int32_t x);
/* PsyQ SquareRoot0; nonnegative inputs <= INT32_MAX. */
uint32_t sh_fixed_sqrt0(uint32_t value);
uint32_t sh_fixed_sqrt12(uint32_t value);

static inline sh_q19_12 sh_q12_mul(sh_q19_12 a, sh_q19_12 b)
{
    return (sh_q19_12)(((int64_t)a * (int64_t)b) >> SH_Q12_SHIFT);
}

static inline sh_q19_12 sh_q12_clamp(sh_q19_12 v, sh_q19_12 lo, sh_q19_12 hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

#ifdef __cplusplus
}
#endif
