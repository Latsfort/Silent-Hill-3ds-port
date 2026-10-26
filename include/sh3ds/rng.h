#pragma once
#include <stdint.h>

uint32_t sh_rng_rand32(void);
int32_t sh_rng_rand16(void);
uint32_t sh_rng_get_seed(void);
void sh_rng_set_seed(uint32_t seed);
