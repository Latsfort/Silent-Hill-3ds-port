/* Host test: drives the sound driver with timed SD_Call commands and writes a 44.1 kHz WAV.
 * usage: snd_test <out.wav> <seconds> [time:cmd | time:x<preload> | time:p<sfx>,<bal>,<atten>]... */
#include "sh3ds/snd.h"
#include "sh3ds/sound.h"

#include "sd_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 44100
#define FPS 30

static void put32(FILE *f, uint32_t v)
{
    fputc(v & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
    fputc((v >> 16) & 0xFF, f);
    fputc((v >> 24) & 0xFF, f);
}

static void put16(FILE *f, uint16_t v)
{
    fputc(v & 0xFF, f);
    fputc(v >> 8, f);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: snd_test <out.wav> <seconds> [time:cmd]...\n");
        return 1;
    }
    sd_port_set_roots("dump/assets", "romfs/game/xa");
    if (!sh_sound_init()) {
        fprintf(stderr, "init failed\n");
        return 1;
    }

    const float secs = (float)atof(argv[2]);
    const int frames_total = (int)(secs * RATE);
    int16_t *pcm = (int16_t *)calloc((size_t)frames_total * 2, sizeof(int16_t));
    bool *done = (bool *)calloc((size_t)argc, sizeof(bool));

    int pos = 0;
    int frame = 0;
    int bgm_target = 0;
    unsigned bgm_flags = SH_BGM_LAYER1;
    while (pos < frames_total) {
        const float t = (float)frame / FPS;
        sh_bgm_frame_begin();
        if (bgm_target > 0 && sh_bgm_crossfade(bgm_target)) {
            printf("%6.2fs bgm %d loaded\n", t, bgm_target);
            bgm_target = 0;
        }
        sh_bgm_layers_update(bgm_flags, 240.0f, false, 1.0f / FPS);
        for (int i = 3; i < argc; i++) {
            if (done[i]) {
                continue;
            }
            const char *colon = strchr(argv[i], ':');
            if (!colon || (float)atof(argv[i]) > t) {
                continue;
            }
            done[i] = true;
            const char *c = colon + 1;
            if (*c == 'b') {
                bgm_target = atoi(c + 1);
            } else if (*c == 'l') {
                bgm_flags = (unsigned)strtoul(c + 1, NULL, 0);
            } else if (*c == 'x') {
                sh_xa_preload((uint16_t)atoi(c + 1));
            } else if (*c == 'p') {
                int id = 0, bal = 0, att = 0;
                sscanf(c + 1, "%d,%d,%d", &id, &bal, &att);
                sh_sfx_play((uint16_t)id, (int8_t)bal, (uint8_t)att);
            } else {
                sh_sd_call((uint32_t)atoi(c));
            }
            printf("%6.2fs cmd %s (streaming %d)\n", t, c, sh_sound_streaming());
        }
        sh_sound_update(1.0f / FPS);
        int n = RATE / FPS;
        if (pos + n > frames_total) {
            n = frames_total - pos;
        }
        sh_sound_render(pcm + pos * 2, n);
        pos += n;
        frame++;
    }

    int peak = 0;
    for (int i = 0; i < frames_total * 2; i++) {
        const int a = abs(pcm[i]);
        peak = a > peak ? a : peak;
    }
    printf("peak %d\n", peak);

    FILE *f = fopen(argv[1], "wb");
    fwrite("RIFF", 1, 4, f);
    put32(f, 36 + frames_total * 4);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put16(f, 1);
    put16(f, 2);
    put32(f, RATE);
    put32(f, RATE * 4);
    put16(f, 4);
    put16(f, 16);
    fwrite("data", 1, 4, f);
    put32(f, frames_total * 4);
    fwrite(pcm, 4, (size_t)frames_total, f);
    fclose(f);
    sh_sound_shutdown();
    return 0;
}
