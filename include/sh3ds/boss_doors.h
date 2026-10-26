#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct ShBossDoors {
    int16_t progress[2], speed;
    int16_t rotation[2];
    bool reverse;
} ShBossDoors;

typedef struct ShBossDoorFrame {
    bool start_sound, stop_sound, finished;
    int volume;
} ShBossDoorFrame;

void sh_boss_doors_step(ShBossDoors *doors, int32_t delta, bool first_valve, ShBossDoorFrame *frame);
