#include "sh3ds/fixed.h"

#include "fixed_trig_data.h"
#include "fixed_gte_data.h"

/* libkmath.s Math_Sin. */
sh_q19_12 sh_fixed_sin(sh_q19_12 angle)
{
    return s_fixed_sine[(uint32_t)angle & 0xfffu];
}

/* libkmath.s Math_Cos; the extracted table repeats its first 1024 values. */
sh_q19_12 sh_fixed_cos(sh_q19_12 angle)
{
    return s_fixed_sine[((uint32_t)angle + 1024u) & 0xfffu];
}

/* libgte/ratan.o: overflow avoidance uses a shifted denominator for large inputs. */
static uint32_t atan_ratio(uint32_t numerator, uint32_t denominator)
{
    return numerator & 0x7fe00000u ? numerator / (denominator >> 10) : (numerator << 10) / denominator;
}

sh_q19_12 sh_fixed_atan2(int32_t y, int32_t x)
{
    const uint32_t ay = y < 0 ? 0u - (uint32_t)y : (uint32_t)y;
    const uint32_t ax = x < 0 ? 0u - (uint32_t)x : (uint32_t)x;
    if (!ay && !ax) {
        return 0;
    }
    int32_t angle = ay < ax ? s_fixed_atan[atan_ratio(ay, ax)] :
                               1024 - s_fixed_atan[atan_ratio(ax, ay)];
    if (x < 0) {
        angle = 2048 - angle;
    }
    return y < 0 ? -angle : angle;
}

/* libgte/msc09.o SquareRoot0: GTE LZCS/LZCR, even exponent, SQRT lookup. */
uint32_t sh_fixed_sqrt0(uint32_t value)
{
    if (!value) {
        return 0;
    }
    const unsigned leading = (unsigned)__builtin_clz(value) & ~1u;
    const unsigned exponent = (31u - leading) >> 1;
    const uint32_t normalized = leading >= 24 ? value << (leading - 24) : value >> (24 - leading);
    return ((uint32_t)s_fixed_sqrt[normalized - 64] << exponent) >> 12;
}

/* BODYPROG SquareRoot12 80097528; even LZCR, exponent(19-LZCR)/2. */
uint32_t sh_fixed_sqrt12(uint32_t value)
{
    if (!value) { return 0; }
    unsigned leading = (unsigned)__builtin_clz(value) & 30u;
    uint32_t normalized = leading >= 24 ? value << (leading - 24) : value >> (24 - leading);
    uint32_t root = (uint16_t)s_fixed_sqrt[normalized - 64];
    int exponent = (19 - (int)leading) >> 1;
    return exponent >= 0 ? root << exponent : root >> -exponent;
}
