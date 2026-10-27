#include "sh3ds/ipd.h"

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++) {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

bool sh_ipd_relocate(uint8_t *data, size_t size, int chunk_x, int chunk_z)
{
    if (!data || size < 0x88 || data[0] != 20 ||
        chunk_x < -128 || chunk_x > 127 || chunk_z < -128 || chunk_z > 127) {
        return false;
    }

    /* func_80044044: chunk coordinates and collision origins move together. */
    const int dx = chunk_x - (int8_t)data[2];
    const int dz = chunk_z - (int8_t)data[3];
    write_u32(data + 0x54, read_u32(data + 0x54) + (uint32_t)(dx * (40 * 256)));
    write_u32(data + 0x58, read_u32(data + 0x58) + (uint32_t)(dz * (40 * 256)));
    data[2] = (uint8_t)chunk_x;
    data[3] = (uint8_t)chunk_z;
    return true;
}
