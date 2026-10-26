#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct ShPiano {
    int32_t x, y, timer;
    int step, counter;
    float motion_fraction[2];
} ShPiano;

typedef struct ShPianoResult {
    bool draw_cursor, finished, solved;
    int16_t cursor_x, cursor_y;
    uint16_t sound;
} ShPianoResult;

void sh_piano_init(ShPiano *piano);
int sh_piano_axis(int circle, bool negative, bool positive);
int32_t sh_piano_gravity(float dt);
ShPianoResult sh_piano_update(ShPiano *piano, int stick_x, int stick_y,
                             bool enter, bool cancel, float dt);
