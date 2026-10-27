#include "sh3ds/save_menu.h"
#include "sh3ds/controls.h"
#include "sh3ds/snd.h"

#include <3ds.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

bool sh_input_down(const ShInputState *input, uint32_t key) { return (input->down & key) != 0; }
void sh_snd_play(uint16_t id, int balance, int attenuation) { (void)id; (void)balance; (void)attenuation; }

static ShSaveMenuAction press(ShSaveMenu *menu, uint32_t key)
{
    const ShInputState input = {.down = key};
    return sh_save_menu_update(menu, &input, 1.0f / 30.0f);
}

int main(void)
{
    const char *file = "build/sh3ds-0-000.sav";
    remove(file);
    remove("build/sh3ds-0-000.sav.bak");
    ShAutosave snapshot = {0}, readback;
    sh_save_init(&snapshot.save);
    snapshot.save.map_idx = 1;
    snapshot.save.location_id = 1;
    snapshot.valid = true;
    ShSaveMenu menu;
    sh_save_menu_begin(&menu, true, &snapshot, "build");
    assert(menu.step == SH_SAVE_MENU_INTRO);
    assert(press(&menu, 0) == SH_SAVE_MENU_WAIT && menu.step == SH_SAVE_MENU_INTRO);
    assert(press(&menu, KEY_B) == SH_SAVE_MENU_CLOSED);
    sh_save_menu_begin(&menu, true, &snapshot, "build");
    press(&menu, KEY_A);
    assert(menu.step == SH_SAVE_MENU_LIST && sh_save_event_get(&menu.snapshot.save, 186));
    press(&menu, KEY_A);
    assert(menu.step == SH_SAVE_MENU_STATUS && menu.result == SH_SAVEFILE_OK);
    assert(sh_savefile_read(file, &readback) == SH_SAVEFILE_OK);
    assert(readback.save.location_id == 1 && sh_save_event_get(&readback.save, 186));
    char title[48], body[768];
    sh_save_menu_format(&menu, title, sizeof(title), body, sizeof(body));
    assert(strcmp(body, "Finished saving.") == 0);
    press(&menu, KEY_A);
    menu.snapshot.save.health = SH_Q12(44.0f);
    press(&menu, KEY_A);
    assert(menu.step == SH_SAVE_MENU_OVERWRITE);
    press(&menu, KEY_B);
    assert(menu.step == SH_SAVE_MENU_LIST);
    assert(sh_savefile_read(file, &readback) == SH_SAVEFILE_OK && readback.save.health == SH_Q12(100.0f));
    press(&menu, KEY_A);
    press(&menu, KEY_A);
    assert(menu.step == SH_SAVE_MENU_STATUS && menu.result == SH_SAVEFILE_OK);
    assert(sh_savefile_read(file, &readback) == SH_SAVEFILE_OK && readback.save.health == SH_Q12(44.0f));
    assert(press(&menu, KEY_A) == SH_SAVE_MENU_WAIT);
    assert(press(&menu, KEY_B) == SH_SAVE_MENU_CLOSED);
    puts("PASS: save-point introduction, persistent save and overwrite confirmation/cancellation");

    sh_save_menu_begin(&menu, false, NULL, "build");
    assert(menu.step == SH_SAVE_MENU_LIST && menu.entries[0][0].result == SH_SAVEFILE_OK);
    press(&menu, KEY_A);
    assert(menu.step == SH_SAVE_MENU_STATUS && menu.loaded);
    sh_save_menu_format(&menu, title, sizeof(title), body, sizeof(body));
    assert(strcmp(body, "Finished loading.") == 0);
    assert(press(&menu, KEY_A) == SH_SAVE_MENU_LOADED && menu.snapshot.save.health == SH_Q12(44.0f));
    sh_save_menu_begin(&menu, false, NULL, "build");
    press(&menu, KEY_R);
    assert(menu.card == 1);
    press(&menu, KEY_L);
    assert(menu.card == 0);
    press(&menu, KEY_DUP);
    assert(menu.entry == SH_SAVE_ENTRIES - 1);
    press(&menu, KEY_DDOWN);
    assert(menu.entry == 0);
    press(&menu, KEY_DDOWN);
    press(&menu, KEY_A);
    assert(menu.step == SH_SAVE_MENU_STATUS && !menu.loaded && menu.result == SH_SAVEFILE_MISSING);
    assert(press(&menu, KEY_A) == SH_SAVE_MENU_WAIT);
    char tiny_title[1], tiny_body[1];
    sh_save_menu_format(&menu, tiny_title, sizeof(tiny_title), tiny_body, sizeof(tiny_body));
    assert(tiny_title[0] == 0 && tiny_body[0] == 0);
    assert(press(&menu, KEY_B) == SH_SAVE_MENU_CLOSED);
    remove(file);
    remove("build/sh3ds-0-000.sav.bak");
    puts("PASS: load acknowledgement, slot navigation, missing data and bounded formatting");
    return 0;
}
