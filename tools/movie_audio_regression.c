#include "sh3ds/sound.h"
#include "sd_port.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    sd_port_set_roots("romfs/game/snd", "romfs/game/xa");
    assert(sh_sound_init());
    assert(sh_sound_movie_begin("romfs/game/movie/016.xac"));
    assert(sd_port_xa_playing());
    int16_t samples[1024 * 2];
    unsigned nonzero = 0;
    for (unsigned block = 0; block < 90; block++) {
        sh_sound_render(samples, 1024);
        for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
            if (samples[i]) { nonzero++; }
        }
    }
    /* Independent decoding of M2_01190 confirms its original XA packets are silent. */
    assert(nonzero == 0 && !sd_port_xa_playing());
    sh_sound_movie_end();
    assert(!sh_sound_movie_begin("build/nonexistent-movie.xac"));
    assert(!sd_port_xa_playing());
    const char *fixture = "build/movie-audio-test.xac";
    FILE *file = fopen(fixture, "wb");
    const uint8_t header[8] = {'X', 'A', 'C', '1', 1, 0, 1, 0};
    uint8_t sector[2304] = {0};
    for (unsigned group = 0; group < 18; group++) {
        for (unsigned i = 16; i < 128; i++) { sector[group * 128 + i] = 0x77; }
    }
    assert(file && fwrite(header, 1, sizeof(header), file) == sizeof(header));
    assert(fwrite(sector, 1, sizeof(sector), file) == sizeof(sector) && fclose(file) == 0);
    assert(sh_sound_movie_begin(fixture));
    nonzero = 0;
    for (unsigned block = 0; block < 8; block++) {
        sh_sound_render(samples, 1024);
        for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
            if (samples[i]) { nonzero++; }
        }
    }
    assert(nonzero > 100 && !sd_port_xa_playing());
    sh_sound_movie_end();
    remove(fixture);
    sh_sound_shutdown();
    puts("PASS: original silent XA packets, non-silent decoder fixture, completion and clean stop");
    return 0;
}
