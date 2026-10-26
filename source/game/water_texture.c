#include "sh3ds/water_texture.h"
#include <string.h>

#include "water_palette_data.h"

static size_t tiled_index(unsigned x, unsigned y, unsigned width)
{
    const unsigned tile = (((y >> 3) * (width >> 3) + (x >> 3)) << 6);
    const unsigned morton = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) |
                            ((x & 4) << 2) | ((y & 4) << 3);
    return tile + morton;
}

unsigned sh_water_spray_shade_slot(uint8_t shade)
{
    return s_water_spray_shade_slot[shade];
}

static bool decode_atlas(const ShTim *tim, uint32_t *out, size_t pixels, bool ribbon)
{
    const unsigned width = ribbon ? SH_WATER_RIBBON_ATLAS_WIDTH : SH_WATER_SPRAY_ATLAS_WIDTH;
    const unsigned height = ribbon ? SH_WATER_RIBBON_ATLAS_HEIGHT : SH_WATER_SPRAY_ATLAS_HEIGHT;
    const unsigned tile_width = ribbon ? SH_WATER_RIBBON_TEXTURE_W : SH_WATER_SPRAY_TEXTURE_W;
    const unsigned tile_height = ribbon ? SH_WATER_RIBBON_TEXTURE_H : SH_WATER_SPRAY_TEXTURE_H;
    if (!tim || !out || !tim->pixels || !tim->clut || tim->bpp != 4 ||
        pixels < width * height ||
        tim->width < (ribbon ? SH_WATER_RIBBON_TEXTURE_U + SH_WATER_RIBBON_TEXTURE_U_STRIDE + SH_WATER_RIBBON_TEXTURE_W : SH_WATER_SPRAY_TEXTURE_U + SH_WATER_SPRAY_TEXTURE_W) ||
        tim->height < SH_WATER_SPRAY_TEXTURE_V + SH_WATER_SPRAY_TEXTURE_STRIDE * SH_WATER_SPRAY_TEXTURE_VARIANTS ||
        tim->clut_rows <= SH_WATER_SPRAY_CLUT_ROW || tim->clut_colors < 16) { return false; }
    memset(out, 0, width * height * sizeof(*out));
    for (unsigned slot = 0; slot < sizeof(s_water_spray_slot_shade); slot++) {
        const unsigned shade = s_water_spray_slot_shade[slot];
        for (unsigned variant = 0; variant < SH_WATER_SPRAY_TEXTURE_VARIANTS; variant++) {
            const unsigned tile = slot * SH_WATER_SPRAY_TEXTURE_VARIANTS + variant;
            const unsigned tx = tile % SH_WATER_SPRAY_ATLAS_COLUMNS * tile_width;
            const unsigned ty = tile / SH_WATER_SPRAY_ATLAS_COLUMNS * tile_height;
            const unsigned source_x = ribbon ? SH_WATER_RIBBON_TEXTURE_U + (variant >> 1) * SH_WATER_RIBBON_TEXTURE_U_STRIDE : SH_WATER_SPRAY_TEXTURE_U;
            const unsigned source_y = ribbon ? SH_WATER_RIBBON_TEXTURE_V + (variant & 1) * SH_WATER_RIBBON_TEXTURE_V_STRIDE : SH_WATER_SPRAY_TEXTURE_V + variant * SH_WATER_SPRAY_TEXTURE_STRIDE;
            for (unsigned y = 0; y < tile_height; y++) {
                for (unsigned x = 0; x < tile_width; x++) {
                    const uint32_t source = sh_tim_pixel_rgba_clut(tim, source_x + x, source_y + y,
                        tim->clut_colors * SH_WATER_SPRAY_CLUT_ROW);
                    uint32_t color = source & 255;
                    for (int channel = 0; channel < 3; channel++) {
                        const unsigned shift = 24 - channel * 8;
                        unsigned value = ((source >> (shift + 3)) & 31) * shade / 128;
                        if (value > 31) { value = 31; }
                        color |= ((value << 3) | (value >> 2)) << shift;
                    }
                    out[tiled_index(tx + x, ty + y, width)] = color;
                }
            }
        }
    }
    return true;
}

bool sh_water_spray_decode_atlas(const ShTim *tim, uint32_t *out, size_t pixels)
{
    return decode_atlas(tim, out, pixels, false);
}

bool sh_water_ribbon_decode_atlas(const ShTim *tim, uint32_t *out, size_t pixels)
{
    return decode_atlas(tim, out, pixels, true);
}

bool sh_water_surface_decode(const ShTim *tim, uint32_t *out, size_t pixels)
{
    if (!tim || !out || !tim->pixels || !tim->clut || tim->bpp != 4 ||
        pixels < SH_WATER_SURFACE_TEXTURE_W * SH_WATER_SURFACE_TEXTURE_H ||
        tim->width < SH_WATER_SURFACE_TEXTURE_U + SH_WATER_SURFACE_TEXTURE_W ||
        tim->height < SH_WATER_SURFACE_TEXTURE_V + SH_WATER_SURFACE_TEXTURE_H ||
        tim->clut_rows <= SH_WATER_SURFACE_CLUT_ROW || tim->clut_colors < 16) { return false; }
    for (unsigned y = 0; y < SH_WATER_SURFACE_TEXTURE_H; y++) {
        for (unsigned x = 0; x < SH_WATER_SURFACE_TEXTURE_W; x++) {
            out[tiled_index(x, y, SH_WATER_SURFACE_TEXTURE_W)] = sh_tim_pixel_rgba_clut(tim,
                SH_WATER_SURFACE_TEXTURE_U + x, SH_WATER_SURFACE_TEXTURE_V + y,
                tim->clut_colors * SH_WATER_SURFACE_CLUT_ROW);
        }
    }
    return true;
}
