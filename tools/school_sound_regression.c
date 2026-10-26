#include "sh3ds/sound.h"
#include "sd_port.h"
#include "libsd/libsd.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    sd_port_set_roots("romfs/game/snd", "romfs/game/xa");
    assert(sh_sound_init());
    sh_sd_call(17);
    sh_sd_call(172); /* background_sound_init.c g_AmbientVabTaskLoad[5]. */
    for (int frame = 0; frame < 300; frame++) {
        sh_sound_update(1.0f / 30.0f);
    }
    assert(sh_sound_streaming() == 0);
    assert(sd_file_get(12)); /* g_AudioData[12] MEP100.VAB. */
    const uint16_t sounds[] = {1416, 1419, 1420, 1422, 1430};
    for (unsigned i = 0; i < sizeof(sounds) / sizeof(sounds[0]); i++) {
        assert(sh_sfx_play(sounds[i], 0, 0) < SD_VOICE_COUNT);
        int16_t pcm[4096];
        unsigned nonzero = 0;
        for (int block = 0; block < 10; block++) {
            sh_sound_render(pcm, 2048);
            for (unsigned sample = 0; sample < sizeof(pcm) / sizeof(pcm[0]); sample++) {
                nonzero += pcm[sample] != 0;
            }
        }
        assert(nonzero > 0);
        sh_sfx_stop(sounds[i]);
        printf("Sfx %u: PCM generated\n", sounds[i]);
    }
    sh_sound_shutdown();
    puts("PASS: retail school ambient VAB loads and all medallion/boiler effects produce audio");
    return 0;
}
