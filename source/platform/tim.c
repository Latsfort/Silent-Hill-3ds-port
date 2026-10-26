#include "sh3ds/tim.h"

#define TIM_MAGIC 0x10u
#define TIM_FLAG_CLUT 0x8u

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t ps1_color_to_rgba(uint16_t c)
{
    if (c == 0) {
        return 0;
    }

    uint32_t r = c & 31;
    uint32_t g = (c >> 5) & 31;
    uint32_t b = (c >> 10) & 31;
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return (r << 24) | (g << 16) | (b << 8) | 0xFFu;
}

bool sh_tim_parse(const uint8_t *data, size_t size, ShTim *out)
{
    if (!data || !out || size < 8 + 12) {
        return false;
    }
    if (read_u32(data) != TIM_MAGIC) {
        return false;
    }

    const uint32_t flags = read_u32(data + 4);
    const uint32_t mode = flags & 3;
    size_t offset = 8;

    out->clut = NULL;
    out->clut_colors = 0;
    out->clut_rows = 0;

    if (flags & TIM_FLAG_CLUT) {
        if (offset + 12 > size) {
            return false;
        }
        const uint32_t block_len = read_u32(data + offset);
        const uint16_t clut_w = read_u16(data + offset + 8);
        const uint16_t clut_h = read_u16(data + offset + 10);
        if (block_len < 12 || offset + block_len > size ||
            (size_t)clut_w * clut_h * 2u > block_len - 12) {
            return false;
        }
        out->clut = data + offset + 12;
        out->clut_colors = clut_w;
        out->clut_rows = clut_h;
        offset += block_len;
    }

    if (offset + 12 > size) {
        return false;
    }

    const uint16_t w16 = read_u16(data + offset + 8);
    const uint16_t h = read_u16(data + offset + 10);
    const size_t pixel_bytes = (size_t)w16 * 2u * h;
    if (offset + 12 + pixel_bytes > size) {
        return false;
    }

    switch (mode) {
    case 0:
        out->bpp = 4;
        out->width = w16 * 4;
        break;
    case 1:
        out->bpp = 8;
        out->width = w16 * 2;
        break;
    case 2:
        out->bpp = 16;
        out->width = w16;
        break;
    default:
        return false; /* 24-bit TIMs are not used by Silent Hill */
    }

    out->height = h;
    out->pixels = data + offset + 12;
    out->stride = w16 * 2;
    return out->width > 0 && out->height > 0;
}

uint32_t sh_tim_pixel_rgba(const ShTim *tim, int x, int y)
{
    return sh_tim_pixel_rgba_clut(tim, x, y, 0);
}

int sh_tim_clut_offset(const ShTim *tim, unsigned clut_delta)
{
    const int offset = (int)(clut_delta >> 6) * tim->clut_colors + (int)(clut_delta & 63) * 16;
    return offset < tim->clut_colors * tim->clut_rows ? offset : 0;
}

uint32_t sh_tim_palette_rgba(const ShTim *tim, int clut_offset, unsigned index)
{
    const int total = tim->clut_colors * tim->clut_rows;
    if (!tim->clut || clut_offset < 0 || clut_offset + (int)index >= total) {
        const uint32_t v = (tim->bpp == 4) ? index * 17u : index & 0xFFu;
        return (v << 24) | (v << 16) | (v << 8) | 0xFFu;
    }
    return ps1_color_to_rgba(read_u16(tim->clut + (size_t)(clut_offset + index) * 2));
}

uint32_t sh_tim_pixel_rgba_clut(const ShTim *tim, int x, int y, int clut_offset)
{
    const uint8_t *row = tim->pixels + (size_t)y * tim->stride;
    unsigned index;

    switch (tim->bpp) {
    case 16:
        return ps1_color_to_rgba(read_u16(row + x * 2));
    case 8:
        index = row[x];
        break;
    case 4:
    default:
        index = (row[x >> 1] >> ((x & 1) * 4)) & 0xF;
        break;
    }
    return sh_tim_palette_rgba(tim, clut_offset, index);
}
