#include "sh3ds/water.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check(int count, unsigned mask, int cursor, bool ball)
{
    ShWaterParticle actual[SH_ROOF_WATER_POOL_COUNT], expected[SH_ROOF_WATER_POOL_COUNT];
    memset(actual, 0xA5, sizeof(actual));
    for (int i = 0; i < count; i++) {
        actual[i].field_A = i < 8 && (mask & (1u << i)) ? (i % 3 ? 14 : i % 2 ? 27 : 28) : 0;
    }
    memcpy(expected, actual, sizeof(actual));
    ShWaterConfig config = {0};
    sh_water_configure_roof(&config, ball);
    for (int i = 0; i < 4; i++) { config.field_54[i] = 0x1234; }
    ShWaterPool pool = {actual, count, cursor, 0xA5};
    const uint32_t seed = 0x12345678;
    sh_rng_set_seed(seed);
    int expected_cursor = cursor;
    for (int particle = 0; particle < config.field_74; particle++) {
        int slot = -1;
        for (int offset = 0; offset < count; offset++) {
            const int index = (expected_cursor + offset) % count;
            if (!expected[index].field_A) { slot = index; break; }
        }
        if (slot < 0) { break; }
        expected[slot].field_A = 27;
        sh_water_particle_spawn(&expected[slot], &config, 2, 0);
        expected_cursor = slot + 1;
    }
    const uint32_t expected_seed = sh_rng_get_seed();
    sh_rng_set_seed(seed);
    sh_water_pool_begin(&pool, &config);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    assert(pool.next == expected_cursor && pool.flags == (0xA5 | (1u << 3)));
    assert(sh_rng_get_seed() == expected_seed);
    for (int i = 0; i < 4; i++) { assert(config.field_54[i] == (i < config.field_78 ? 0 : 0x1234)); }
    for (int i = 0; i < count; i++) {
        if (expected[i].field_A == 27 || expected[i].field_A == 28) { expected[i].field_A = 0; }
    }
    sh_water_pool_stop(&pool);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    assert(pool.next == expected_cursor && pool.flags == (0xA5 & ~(1u << 3)));
    assert(sh_rng_get_seed() == expected_seed);
}

int main(void)
{
    unsigned checks = 0;
    for (int ball = 0; ball <= 1; ball++) {
        for (unsigned mask = 0; mask < 256; mask++) {
            for (int cursor = 0; cursor <= 8; cursor++) { check(8, mask, cursor, ball); checks++; }
        }
        check(SH_ROOF_WATER_POOL_COUNT, 0, 0, ball);
        checks++;
    }
    printf("PASS: %u shared-pool configurations, circular allocation, full pools, RNG and selective cleanup\n", checks);
    return 0;
}
