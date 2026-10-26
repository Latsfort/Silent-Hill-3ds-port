#include "sh3ds/sound.h"
#include "sd_port.h"
#include "libsd/libsd.h"

#include <stdio.h>

int main(void)
{
    sd_port_set_roots("romfs/game/snd", "romfs/game/xa");
    if (!sh_sound_init()) {
        return 1;
    }
    /* crash_dump_00000007: opening sound reads an unopened VAB slot 2. */
    if (!vab_h[2].vh_addr_4 || vab_h[2].vab_id != 2) {
        puts("FAIL: opening VAB is unavailable immediately after sound initialization");
        sh_sound_shutdown();
        return 1;
    }
    if (sh_sfx_play(1361, 0, 144) >= SD_VOICE_COUNT) {
        puts("FAIL: opening ambience could not start without waiting at the title");
        sh_sound_shutdown();
        return 1;
    }
    if (SdVoKeyOn(3 << 8, 60 << 8, 64, 64) != -1 ||
        SdUtKeyOnV(0, 3, 0, 0, 60, 0, 64, 64) != -1) {
        puts("FAIL: unopened sound banks must reject playback");
        sh_sound_shutdown();
        return 1;
    }
    sh_sound_shutdown();
    sd_port_set_roots("build/missing-sound-assets", "build/missing-xa-assets");
    sh_sound_init();
    if (SdVoKeyOn(2 << 8, 60 << 8, 64, 64) != -1) {
        puts("FAIL: missing sound files must not leave a playable stale bank");
        sh_sound_shutdown();
        return 1;
    }
    sh_sound_shutdown();
    puts("PASS: immediate opening ambience, unopened banks and missing assets");
    return 0;
}
