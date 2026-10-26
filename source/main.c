#include "sh3ds/autotest.h"
#include "sh3ds/assets.h"
#include "sh3ds/config.h"
#include "sh3ds/game.h"
#include "sh3ds/gfx.h"
#include "sh3ds/input.h"
#include "sh3ds/memory.h"
#include "sh3ds/sound.h"

#include <3ds.h>

static ShGame s_game;

int main(int argc, char *argv[])
{
    sh_assets_init(argc, argv);

    if (!sh_memory_init()) {
        romfsExit();
        return 1;
    }
    if (!sh_gfx_init()) {
        sh_memory_shutdown();
        romfsExit();
        return 1;
    }
    if (!sh_input_init()) {
        sh_gfx_shutdown();
        sh_memory_shutdown();
        romfsExit();
        return 1;
    }
    sh_sound_init(); /* optional: the driver still runs muted if the DSP is unavailable */

    if (!sh_game_init(&s_game)) {
        sh_sound_shutdown();
        sh_input_shutdown();
        sh_gfx_shutdown();
        sh_memory_shutdown();
        romfsExit();
        return 1;
    }

    sh_autotest_init();

    uint64_t last_ms = osGetTime();
    while (aptMainLoop()) {
        ShInputState input;
        sh_input_poll(&input);
        sh_autotest_input(&input);

        sh_gfx_begin_frame();
        sh_game_update(&s_game, &input);
        sh_autotest_log(&s_game);
        const uint64_t now = osGetTime();
        sh_sound_update((float)(now - last_ms) / 1000.0f);
        last_ms = now;
        sh_game_render(&s_game);
        sh_gfx_end_frame();

        if (s_game.wants_exit || sh_autotest_done()) {
            break;
        }
    }

    sh_autotest_shutdown();
    sh_game_shutdown(&s_game);
    sh_sound_shutdown();
    sh_input_shutdown();
    sh_gfx_shutdown();
    sh_memory_shutdown();
    romfsExit();
    return 0;
}
