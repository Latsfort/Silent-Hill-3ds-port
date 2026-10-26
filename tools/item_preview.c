/*
 * Host preview of the pickup close-up: bakes ITEM/UNQxx.TMD with sh_item_model_bake
 * and rasterizes it like the 3DS (48 deg FOV, 400x240, 2x modulate) into a PNG grid.
 *   gcc -O1 -std=c11 -Iinclude tools/item_preview.c source/game/item_model.c source/platform/tim.c -lm
 */
#include "sh3ds/item_model.h"
#include "sh3ds/tim.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 400
#define H 240
#define COLS 4
#define ROWS 3

static uint8_t s_img[H * ROWS][W * COLS][3];

static uint8_t *load(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(*size);
    fread(b, 1, *size, f);
    fclose(f);
    return b;
}

static void draw(int ox, int oy, const ShMapVertex *v, size_t n, const ShTim *tim, int clut)
{
    const float t = tanf(24.0f * 3.14159265f / 180.0f);
    const float aspect = (float)W / (float)H;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            s_img[oy + y][ox + x][0] = 40;
            s_img[oy + y][ox + x][1] = 44;
            s_img[oy + y][ox + x][2] = 48;
        }
    for (size_t i = 0; i + 2 < n; i += 3) {
        float sx[3], sy[3], iz[3];
        for (int k = 0; k < 3; k++) {
            const float z = -v[i + k].pos[2];
            iz[k] = 1.0f / z;
            sx[k] = (v[i + k].pos[0] * iz[k] / (t * aspect) * 0.5f + 0.5f) * W;
            sy[k] = (0.5f - v[i + k].pos[1] * iz[k] / t * 0.5f) * H;
        }
        int x0 = (int)floorf(fminf(sx[0], fminf(sx[1], sx[2]))), x1 = (int)ceilf(fmaxf(sx[0], fmaxf(sx[1], sx[2])));
        int y0 = (int)floorf(fminf(sy[0], fminf(sy[1], sy[2]))), y1 = (int)ceilf(fmaxf(sy[0], fmaxf(sy[1], sy[2])));
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > W - 1) x1 = W - 1;
        if (y1 > H - 1) y1 = H - 1;
        const float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
        if (fabsf(area) < 1e-6f) continue;
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                const float px = x + 0.5f, py = y + 0.5f;
                float w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
                float w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
                float w2 = 1.0f - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                const float a0 = w0 * iz[0], a1 = w1 * iz[1], a2 = w2 * iz[2], s = a0 + a1 + a2;
                const float u = (a0 * v[i].uv[0] + a1 * v[i + 1].uv[0] + a2 * v[i + 2].uv[0]) / s;
                const float vv = (a0 * v[i].uv[1] + a1 * v[i + 1].uv[1] + a2 * v[i + 2].uv[1]) / s;
                const float c = (a0 * v[i].color[0] + a1 * v[i + 1].color[0] + a2 * v[i + 2].color[0]) / s;
                int tu = (int)(u * SH_ITEM_TEX_SIZE), tv = (int)((1.0f - vv) * SH_ITEM_TEX_SIZE);
                if (tu < 0) tu = 0;
                if (tv < 0) tv = 0;
                if (tu > 255) tu = 255;
                if (tv > 255) tv = 255;
                const uint32_t rgba = sh_tim_pixel_rgba_clut(tim, tu, tv, clut);
                if ((rgba & 0xFF) < 0x80) continue;
                const float m = c * 2.0f / 255.0f;
                for (int ch = 0; ch < 3; ch++) {
                    float o = (float)((rgba >> (24 - ch * 8)) & 0xFF) * m;
                    s_img[oy + y][ox + x][ch] = (uint8_t)(o > 255.0f ? 255.0f : o);
                }
            }
    }
}

static uint32_t crc_table[256];
static uint32_t crc(const uint8_t *b, size_t n, uint32_t c)
{
    c = ~c;
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ b[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t h[8];
    be32(h, len);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (len) fwrite(data, 1, len, f);
    uint32_t c = crc((const uint8_t *)type, 4, 0);
    c = crc(data, len, c);
    be32(h, c);
    fwrite(h, 1, 4, f);
}

static void write_png(const char *path, int w, int h, const uint8_t *rgb)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[i] = c;
    }
    const size_t row = (size_t)w * 3 + 1, raw_len = row * h;
    uint8_t *raw = malloc(raw_len);
    for (int y = 0; y < h; y++) {
        raw[y * row] = 0;
        memcpy(raw + y * row + 1, rgb + (size_t)y * w * 3, (size_t)w * 3);
    }
    const size_t blocks = (raw_len + 65534) / 65535;
    uint8_t *z = malloc(raw_len + blocks * 5 + 6);
    size_t zn = 0;
    z[zn++] = 0x78;
    z[zn++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t off = 0; off < raw_len; off += 65535) {
        const size_t len = raw_len - off < 65535 ? raw_len - off : 65535;
        z[zn++] = off + len >= raw_len;
        z[zn++] = len & 0xFF;
        z[zn++] = len >> 8;
        z[zn++] = ~len & 0xFF;
        z[zn++] = (~len >> 8) & 0xFF;
        memcpy(z + zn, raw + off, len);
        zn += len;
    }
    for (size_t i = 0; i < raw_len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    be32(z + zn, (b << 16) | a);
    zn += 4;
    FILE *f = fopen(path, "wb");
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    be32(ihdr, w);
    be32(ihdr + 4, h);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)zn);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
}

int main(void)
{
    static const struct { const char *path; float rx, rz; } items[4] = {
        {"dump/assets/ITEM/UNQ80.TMD", 45, 0},
        {"dump/assets/ITEM/UNQE0.TMD", 45, 0},
        {"dump/assets/ITEM/UNQE1.TMD", 225, 180},
        {"dump/assets/ITEM/UNQ20.TMD", 45, 0},
    };
    size_t tim_size = 0;
    uint8_t *tim_file = load("dump/assets/ITEM/TIM00.TIM", &tim_size);
    ShTim tim;
    if (!tim_file || !sh_tim_parse(tim_file, tim_size, &tim)) {
        fprintf(stderr, "TIM00 missing\n");
        return 1;
    }
    static ShMapVertex verts[SH_ITEM_MAX_VERTS];
    const float spins[ROWS] = {0.0f, 1.2f, 2.6f};
    const float d2r = 3.14159265f / 180.0f;
    for (int i = 0; i < COLS; i++) {
        size_t size = 0;
        uint8_t *tmd = load(items[i].path, &size);
        ShItemModel m;
        if (!tmd || !sh_item_model_init(&m, tmd, size, tim_file, tim_size)) {
            fprintf(stderr, "bad %s\n", items[i].path);
            continue;
        }
        const int clut = sh_tim_clut_offset(&tim, m.clut_delta);
        for (int r = 0; r < ROWS; r++) {
            size_t n = 0;
            sh_item_model_bake(&m, verts, SH_ITEM_MAX_VERTS, &n, items[i].rx * d2r, spins[r], items[i].rz * d2r, 1.0f,
                               48.0f);
            printf("%s spin %.1f: %zu verts clut_delta=%u\n", items[i].path, spins[r], n, m.clut_delta);
            draw(i * W, r * H, verts, n, &tim, clut);
        }
    }
    write_png("build/item_preview.png", W * COLS, H * ROWS, &s_img[0][0][0]);
    return 0;
}
