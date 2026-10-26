#include "sh3ds/choice.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    ShMapChoice choice;
    sh_map_choice_begin(&choice, 3);
    assert(choice.selected == 0 && choice.flash == 0);
    assert(sh_map_choice_color(&choice, 0) == 4 && sh_map_choice_color(&choice, 1) == 7);
    int x, y;
    uint8_t rgb[3];
    sh_map_choice_row(&choice, 0, &x, &y, rgb);
    assert(x == 32 && y == 96 && rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 0);
    sh_map_choice_row(&choice, 2, &x, &y, rgb);
    assert(x == 32 && y == 128 && rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255);
    ShMapChoiceFrame frame = sh_map_choice_step(&choice, 1024, false, false, false, false);
    assert(frame.selection == -1 && !frame.moves && !frame.sound && sh_map_choice_color(&choice, 0) == 7);
    sh_map_choice_step(&choice, 1024, false, false, false, false);
    assert(choice.flash == 0 && sh_map_choice_color(&choice, 0) == 4);
    frame = sh_map_choice_step(&choice, 136, true, false, false, false);
    assert(choice.selected == 0 && !frame.moves);
    frame = sh_map_choice_step(&choice, 136, false, true, false, false);
    assert(choice.selected == 1 && frame.moves == 1 && frame.sound && !choice.flash);
    frame = sh_map_choice_step(&choice, 136, true, true, false, false);
    assert(choice.selected == 1 && frame.moves == 2 && !choice.flash);
    const ShMapChoiceFrame selected = sh_map_choice_step(&choice, 136, false, false, true, false);
    assert(selected.selection == 1 && selected.sound && !selected.moves);
    frame = sh_map_choice_step(&choice, 136, true, true, true, true);
    assert(frame.selection == 2 && choice.selected == 2 && !frame.moves && frame.sound != selected.sound);
    frame = sh_map_choice_step(&choice, 136, false, true, false, false);
    assert(choice.selected == 2 && !frame.moves);
    sh_map_choice_begin(&choice, 2);
    assert(choice.selected == 0);
    frame = sh_map_choice_step(&choice, 136, false, false, false, true);
    assert(choice.selected == 1 && frame.selection == 1);
    frame = sh_map_choice_step(&choice, 136, false, false, true, false);
    assert(frame.selection == 1 && frame.sound);
    puts("PASS: bounded two/three-choice navigation, input priority, repeated movement sounds, cancel and original flash colors");
    return 0;
}
