/* Host-side check of source/platform/tim.c: converts a TIM to a PPM image. */
#include "sh3ds/tim.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: tim2ppm in.TIM out.ppm\n");
        return 1;
    }

    FILE *in = fopen(argv[1], "rb");
    if (!in) {
        return 1;
    }
    fseek(in, 0, SEEK_END);
    const long size = ftell(in);
    fseek(in, 0, SEEK_SET);
    unsigned char *data = malloc((size_t)size);
    fread(data, 1, (size_t)size, in);
    fclose(in);

    ShTim tim;
    if (!sh_tim_parse(data, (size_t)size, &tim)) {
        fprintf(stderr, "parse failed\n");
        return 1;
    }

    FILE *out = fopen(argv[2], "wb");
    fprintf(out, "P6\n%d %d\n255\n", tim.width, tim.height);
    for (int y = 0; y < tim.height; y++) {
        for (int x = 0; x < tim.width; x++) {
            const uint32_t c = sh_tim_pixel_rgba(&tim, x, y);
            fputc((c >> 24) & 0xFF, out);
            fputc((c >> 16) & 0xFF, out);
            fputc((c >> 8) & 0xFF, out);
        }
    }
    fclose(out);
    printf("%s: %dx%d %dbpp\n", argv[1], tim.width, tim.height, tim.bpp);
    return 0;
}
