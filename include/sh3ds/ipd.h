#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool sh_ipd_relocate(uint8_t *data, size_t size, int chunk_x, int chunk_z);
