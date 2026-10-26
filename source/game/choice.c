#include "sh3ds/choice.h"
#include "choice_data.h"

/* Gfx_MapMsg_Draw resets selectedEntryIdx to zero for a new message. */
void sh_map_choice_begin(ShMapChoice *choice, unsigned count)
{
    *choice = (ShMapChoice){.count = (uint8_t)count};
}

ShMapChoiceFrame sh_map_choice_step(ShMapChoice *choice, int32_t delta,
    bool up, bool down, bool enter, bool cancel)
{
    ShMapChoiceFrame frame = {.selection = -1};
    if (cancel || enter) {
        if (cancel) { choice->selected = choice->count - 1; }
        frame.selection = choice->selected;
        frame.sound = choice->selected == choice->count - 1 ? SH_CHOICE_CANCEL : SH_CHOICE_CONFIRM;
        return frame;
    }
    choice->flash += delta;
    if (choice->flash >= SH_CHOICE_FLASH_MAX) { choice->flash -= SH_CHOICE_FLASH_MAX; }
    if (up && choice->selected != 0) {
        choice->flash = 0;
        choice->selected--;
        frame.moves++;
    }
    if (down && choice->selected != choice->count - 1) {
        choice->flash = 0;
        choice->selected++;
        frame.moves++;
    }
    if (frame.moves) { frame.sound = SH_CHOICE_MOVE; }
    return frame;
}

unsigned sh_map_choice_color(const ShMapChoice *choice, unsigned row)
{
    return row == choice->selected ? ((choice->flash >> SH_CHOICE_FLASH_SHIFT) * SH_CHOICE_FLASH_SCALE) + SH_CHOICE_FLASH_BASE : SH_CHOICE_WHITE;
}

/* Gfx_MapMsg_SelectionUpdate layout; PS1 textured text modulates RGB by /128. */
void sh_map_choice_row(const ShMapChoice *choice, unsigned row, int *x, int *y, uint8_t rgb[3])
{
    *x = SH_CHOICE_X;
    *y = SH_CHOICE_Y + SH_CHOICE_LINE * row;
    const unsigned color = sh_map_choice_color(choice, row);
    for (int channel = 0; channel < 3; channel++) { rgb[channel] = s_choice_rgb[color][channel]; }
}
