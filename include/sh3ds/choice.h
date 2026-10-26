#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct ShMapChoice {
    uint8_t selected, count;
    int16_t flash;
} ShMapChoice;

typedef struct ShMapChoiceFrame {
    int selection;
    unsigned moves;
    uint16_t sound;
} ShMapChoiceFrame;

void sh_map_choice_begin(ShMapChoice *choice, unsigned count);
/* Input processing once the retail message rollout has reached its selection. */
ShMapChoiceFrame sh_map_choice_step(ShMapChoice *choice, int32_t delta,
    bool up, bool down, bool enter, bool cancel);
unsigned sh_map_choice_color(const ShMapChoice *choice, unsigned row);
void sh_map_choice_row(const ShMapChoice *choice, unsigned row, int *x, int *y, uint8_t rgb[3]);
