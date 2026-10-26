#include "sh3ds/input.h"

#include <3ds.h>

bool sh_input_init(void)
{
    return true;
}

void sh_input_shutdown(void)
{
}

void sh_input_poll(ShInputState *out)
{
    if (!out) {
        return;
    }

    hidScanInput();

    out->held = hidKeysHeld();
    out->down = hidKeysDown();
    out->up = hidKeysUp();

    circlePosition stick;
    hidCircleRead(&stick);
    out->circle_x = stick.dx;
    out->circle_y = stick.dy;
}

bool sh_input_down(const ShInputState *s, uint32_t key)
{
    return s && (s->down & key) != 0;
}

bool sh_input_held(const ShInputState *s, uint32_t key)
{
    return s && (s->held & key) != 0;
}
