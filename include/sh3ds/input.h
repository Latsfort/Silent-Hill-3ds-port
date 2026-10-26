#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShInputState {
    uint32_t held;
    uint32_t down;
    uint32_t up;
    int16_t circle_x;
    int16_t circle_y;
} ShInputState;

bool sh_input_init(void);
void sh_input_shutdown(void);
void sh_input_poll(ShInputState *out);

bool sh_input_down(const ShInputState *s, uint32_t key);
bool sh_input_held(const ShInputState *s, uint32_t key);

#ifdef __cplusplus
}
#endif
