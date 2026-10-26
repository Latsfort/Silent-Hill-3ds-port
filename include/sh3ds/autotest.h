#ifndef SH3DS_AUTOTEST_H
#define SH3DS_AUTOTEST_H

#include "sh3ds/input.h"

#include <stdbool.h>

struct ShGame;

/*
 * Debug-only scripted input. Active only if sdmc:/sh3ds_autotest.txt exists.
 * Script lines: "<frames> <keys|-> [stick_x stick_y]", keys joined by '+'
 * (A B X Y L R START SELECT UP DOWN LEFT RIGHT), "log <period>", "quit".
 * State is written to sdmc:/sh3ds_autotest.log.
 */
bool sh_autotest_init(void);
bool sh_autotest_active(void);
void sh_autotest_input(ShInputState *io);
void sh_autotest_log(struct ShGame *game);
bool sh_autotest_done(void);
void sh_autotest_shutdown(void);

#endif
