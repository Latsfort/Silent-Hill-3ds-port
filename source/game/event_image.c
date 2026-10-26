#include "sh3ds/event_image.h"

/* Screen_BackgroundImgDraw: neutral RGB is 128, brightness is Q8. */
bool sh_event_image_shade(ShTim *out, uint8_t *pixels, size_t capacity, const ShTim *source, unsigned shade)
{
    if (!out || !pixels || !source || source->width <= 0 || source->height <= 0 ||
        (size_t)source->width * (size_t)source->height > capacity / 2) {
        return false;
    }
    for (int y = 0; y < source->height; y++) {
        for (int x = 0; x < source->width; x++) {
            const uint32_t rgba = sh_tim_pixel_rgba(source, x, y);
            unsigned color = 0x8000u;
            for (int c = 0; c < 3; c++) {
                unsigned value = (((rgba >> (27 - c * 8)) & 31u) * shade) >> 7;
                if (value > 31) {
                    value = 31;
                }
                color |= value << (c * 5);
            }
            const size_t offset = ((size_t)y * source->width + x) * 2;
            pixels[offset] = (uint8_t)color;
            pixels[offset + 1] = (uint8_t)(color >> 8);
        }
    }
    *out = (ShTim){.bpp = 16, .width = source->width, .height = source->height,
                   .pixels = pixels, .stride = source->width * 2};
    return true;
}

/* Screen_BackgroundImgTransition: opaque source, subtract shaded source, add shaded destination. */
bool sh_event_image_transition(ShTim *out, uint8_t *pixels, size_t capacity,
                               const ShTim *from, const ShTim *to, float alpha)
{
    if (!out || !pixels || !from || !to || from->width != to->width || from->height != to->height ||
        from->width <= 0 || from->height <= 0 ||
        (size_t)from->width * (size_t)from->height > capacity / 2) {
        return false;
    }
    int shade = (int)(alpha * 128.0f);
    if (shade < 0) {
        shade = 0;
    } else if (shade > 128) {
        shade = 128;
    }
    for (int y = 0; y < from->height; y++) {
        for (int x = 0; x < from->width; x++) {
            const uint32_t a = sh_tim_pixel_rgba(from, x, y);
            const uint32_t b = sh_tim_pixel_rgba(to, x, y);
            unsigned color = 0x8000u;
            for (int c = 0; c < 3; c++) {
                const int shift = 24 - c * 8;
                const int av = (a >> (shift + 3)) & 31;
                const int bv = (b >> (shift + 3)) & 31;
                const int value = av - ((av * shade) >> 7) + ((bv * shade) >> 7);
                color |= (unsigned)value << (c * 5);
            }
            const size_t offset = ((size_t)y * from->width + x) * 2;
            pixels[offset] = (uint8_t)color;
            pixels[offset + 1] = (uint8_t)(color >> 8);
        }
    }
    *out = (ShTim){.bpp = 16, .width = from->width, .height = from->height,
                   .pixels = pixels, .stride = from->width * 2};
    return true;
}
