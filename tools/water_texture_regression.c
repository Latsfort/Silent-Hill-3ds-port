#include "sh3ds/water_texture.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static size_t tiled(unsigned x, unsigned y)
{
    size_t index = ((y / 8) * (SH_WATER_SPRAY_ATLAS_WIDTH / 8) + x / 8) * 64;
    for (unsigned bit = 0; bit < 3; bit++) {
        index |= ((x >> bit) & 1) << (2 * bit);
        index |= ((y >> bit) & 1) << (2 * bit + 1);
    }
    return index;
}

int main(void)
{
    FILE *file = fopen("romfs/game/tim/WATER.TIM", "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    const size_t size = ftell(file);
    rewind(file);
    uint8_t *raw = malloc(size);
    assert(raw && fread(raw, 1, size, file) == size);
    fclose(file);
    ShTim tim;
    assert(sh_tim_parse(raw, size, &tim));
    const size_t pixels = SH_WATER_SPRAY_ATLAS_WIDTH * SH_WATER_SPRAY_ATLAS_HEIGHT;
    uint32_t *atlas = malloc(pixels * sizeof(*atlas));
    assert(atlas);
    atlas[0] = 0x12345678;
    assert(!sh_water_spray_decode_atlas(&tim, atlas, pixels - 1) && atlas[0] == 0x12345678);
    assert(sh_water_spray_decode_atlas(&tim, atlas, pixels));
    unsigned checks = 0;
    for (unsigned shade = 0; shade < 256; shade++) {
        for (unsigned variant = 0; variant < SH_WATER_SPRAY_TEXTURE_VARIANTS; variant++) {
            const unsigned tile = sh_water_spray_shade_slot(shade) * SH_WATER_SPRAY_TEXTURE_VARIANTS + variant;
            for (unsigned y = 0; y < SH_WATER_SPRAY_TEXTURE_H; y++) {
                for (unsigned x = 0; x < SH_WATER_SPRAY_TEXTURE_W; x++) {
                    const unsigned sx = SH_WATER_SPRAY_TEXTURE_U + x;
                    const unsigned sy = SH_WATER_SPRAY_TEXTURE_V + variant * SH_WATER_SPRAY_TEXTURE_STRIDE + y;
                    const unsigned entry = (tim.pixels[sy * tim.stride + sx / 2] >> ((sx % 2) * 4)) & 15;
                    const size_t offset = (SH_WATER_SPRAY_CLUT_ROW * tim.clut_colors + entry) * 2;
                    const unsigned color = tim.clut[offset] | (tim.clut[offset + 1] << 8);
                    uint32_t expected = color ? 255 : 0;
                    for (unsigned channel = 0; channel < 3; channel++) {
                        unsigned value = ((color >> (channel * 5)) & 31) * shade / 128;
                        if (value > 31) { value = 31; }
                        expected |= ((value << 3) | (value >> 2)) << (24 - channel * 8);
                    }
                    const unsigned ax = tile % SH_WATER_SPRAY_ATLAS_COLUMNS * SH_WATER_SPRAY_TEXTURE_W + x;
                    const unsigned ay = tile / SH_WATER_SPRAY_ATLAS_COLUMNS * SH_WATER_SPRAY_TEXTURE_H + y;
                    assert(atlas[tiled(ax, ay)] == expected);
                    checks++;
                }
            }
        }
    }
    for (unsigned y = 0; y < SH_WATER_SPRAY_ATLAS_HEIGHT; y++) {
        for (unsigned x = 0; x < SH_WATER_SPRAY_ATLAS_WIDTH; x++) {
            const unsigned tile = y / SH_WATER_SPRAY_TEXTURE_H * SH_WATER_SPRAY_ATLAS_COLUMNS + x / SH_WATER_SPRAY_TEXTURE_W;
            if (tile >= SH_WATER_SPRAY_PALETTE_SLOTS * SH_WATER_SPRAY_TEXTURE_VARIANTS) { assert(!atlas[tiled(x, y)]); }
        }
    }
    free(atlas);
    const size_t surface_pixels = SH_WATER_SURFACE_TEXTURE_W * SH_WATER_SURFACE_TEXTURE_H;
    uint32_t *surface = malloc((surface_pixels + 1) * sizeof(*surface));
    assert(surface);
    surface[0] = surface[surface_pixels] = 0x12345678;
    assert(!sh_water_surface_decode(&tim, surface, surface_pixels - 1) && surface[0] == 0x12345678);
    assert(sh_water_surface_decode(&tim, surface, surface_pixels) && surface[surface_pixels] == 0x12345678);
    unsigned zero_entries = 0;
    for (unsigned y = 0; y < SH_WATER_SURFACE_TEXTURE_H; y++) {
        for (unsigned x = 0; x < SH_WATER_SURFACE_TEXTURE_W; x++) {
            const unsigned sx = SH_WATER_SURFACE_TEXTURE_U + x, sy = SH_WATER_SURFACE_TEXTURE_V + y;
            const unsigned entry = (tim.pixels[sy * tim.stride + sx / 2] >> ((sx % 2) * 4)) & 15;
            const size_t offset = (SH_WATER_SURFACE_CLUT_ROW * tim.clut_colors + entry) * 2;
            const unsigned color = tim.clut[offset] | (tim.clut[offset + 1] << 8);
            uint32_t expected = color ? 255 : 0;
            for (unsigned channel = 0; channel < 3; channel++) {
                const unsigned component = (color >> (channel * 5)) & 31;
                expected |= ((component << 3) | (component >> 2)) << (24 - channel * 8);
            }
            size_t target = ((y / 8) * (SH_WATER_SURFACE_TEXTURE_W / 8) + x / 8) * 64;
            for (unsigned bit = 0; bit < 3; bit++) {
                target |= ((x >> bit) & 1) << (bit * 2);
                target |= ((y >> bit) & 1) << (bit * 2 + 1);
            }
            assert(surface[target] == expected);
            if (!entry) { assert(surface[target] & 255); zero_entries++; }
        }
    }
    assert(zero_entries);
    free(surface);
    free(raw);
    printf("PASS: %u exact WATER.TIM shade/variant texels, tiled layout, padding and capacity guard\n", checks);
    printf("PASS: %u surface texels with original second palette, including nontransparent index zero\n", (unsigned)surface_pixels);
    return 0;
}
