#pragma once
#include "sh3ds/environment.h"

typedef struct ShAlleyFlicker {
    int32_t value;
    struct { int16_t field_0, field_2; } phase;
    struct { uint8_t field_0, field_1, field_2; } rate;
} ShAlleyFlicker;

void sh_alley_flicker_init(ShAlleyFlicker *state);
void sh_alley_flicker_update(ShAlleyFlicker *state, ShEnvironment *environment, int32_t delta);
