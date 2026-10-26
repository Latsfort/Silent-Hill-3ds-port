#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* View over a PS1 TIM file held in memory; pixels are decoded on demand. */
typedef struct ShTim {
    int bpp; /* 4, 8 or 16 */
    int width;
    int height;
    const uint8_t *clut; /* palette rows, little-endian 16-bit colors */
    int clut_colors;     /* entries per row */
    int clut_rows;
    const uint8_t *pixels;
    int stride; /* bytes per row */
} ShTim;

bool sh_tim_parse(const uint8_t *data, size_t size, ShTim *out);

/* Returns 0xRRGGBBAA, fully transparent for PS1 color 0x0000. */
uint32_t sh_tim_pixel_rgba(const ShTim *tim, int x, int y);

/* Same as sh_tim_pixel_rgba but looks colors up `clut_offset` entries into the CLUT block. */
uint32_t sh_tim_pixel_rgba_clut(const ShTim *tim, int x, int y, int clut_offset);

/* Color of palette entry `index` at `clut_offset`, same conventions as sh_tim_pixel_rgba_clut. */
uint32_t sh_tim_palette_rgba(const ShTim *tim, int clut_offset, unsigned index);

/* Converts a PS1 CLUT id delta (bits 0-5: x / 16, bits 6+: row) into a CLUT entry offset. */
int sh_tim_clut_offset(const ShTim *tim, unsigned clut_delta);

#ifdef __cplusplus
}
#endif
