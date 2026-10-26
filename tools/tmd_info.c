/* Dumps PS1 TMD object/primitive info (texture page + CLUT usage) for item models. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb");
        if (!f) continue;
        static uint8_t b[1 << 20];
        size_t n = fread(b, 1, sizeof(b), f);
        fclose(f);
        uint32_t nobj = rd32(b + 8);
        printf("%s size=%zu id=%x flags=%x nobj=%u\n", argv[a], n, rd32(b), rd32(b + 4), nobj);
        for (uint32_t o = 0; o < nobj; o++) {
            const uint8_t *ob = b + 12 + o * 28;
            uint32_t vt = rd32(ob), nv = rd32(ob + 4), nt = rd32(ob + 8), nn = rd32(ob + 12);
            uint32_t pt = rd32(ob + 16), np = rd32(ob + 20);
            int32_t sc = (int32_t)rd32(ob + 24);
            printf(" obj%u verts@%x n=%u norms@%x n=%u prims@%x n=%u scale=%d\n", o, vt, nv, nt, nn, pt, np, sc);
            int16_t mn[3] = {32767, 32767, 32767}, mx[3] = {-32768, -32768, -32768};
            for (uint32_t i = 0; i < nv; i++) {
                const uint8_t *v = b + 12 + vt + i * 8;
                for (int k = 0; k < 3; k++) {
                    int16_t c = (int16_t)rd16(v + k * 2);
                    if (c < mn[k]) mn[k] = c;
                    if (c > mx[k]) mx[k] = c;
                }
            }
            printf("  bbox (%d %d %d)..(%d %d %d)\n", mn[0], mn[1], mn[2], mx[0], mx[1], mx[2]);
            const uint8_t *p = b + 12 + pt;
            int lastmode = -1, lastcba = -1, lasttsb = -1;
            for (uint32_t i = 0; i < np; i++) {
                int olen = p[0], ilen = p[1], flag = p[2], mode = p[3];
                int cba = -1, tsb = -1;
                if (mode & 4) {
                    cba = rd16(p + 4 + 2);
                    tsb = rd16(p + 4 + 6);
                }
                if (mode != lastmode || cba != lastcba || tsb != lasttsb) {
                    printf("  prim%u olen=%d ilen=%d flag=%d mode=%02x", i, olen, ilen, flag, mode);
                    if (cba >= 0)
                        printf(" clut=(%d,%d) tpage=(%d,%d) tp=%d abr=%d", (cba & 63) * 16, cba >> 6, (tsb & 15) * 64,
                               ((tsb >> 4) & 1) * 256, (tsb >> 7) & 3, (tsb >> 5) & 3);
                    printf("\n");
                    lastmode = mode;
                    lastcba = cba;
                    lasttsb = tsb;
                }
                p += 4 + ilen * 4;
            }
        }
    }
    return 0;
}
