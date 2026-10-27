#include "sh3ds/save_menu.h"
#include "sh3ds/controls.h"
#include "sh3ds/snd.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "save_menu_data.h"

static bool menu_path(const ShSaveMenu *menu, unsigned card, unsigned entry, char *path, size_t size)
{
    const int length = snprintf(path, size, "%s/sh3ds-%u-%03u.sav", menu->root, card, entry);
    return length >= 0 && (size_t)length < size;
}

static void menu_refresh(ShSaveMenu *menu)
{
    for (unsigned card = 0; card < SH_SAVE_CARDS; card++) {
        for (unsigned entry = 0; entry < SH_SAVE_ENTRIES; entry++) {
            char path[512];
            ShAutosave saved;
            ShSaveMenuEntry *row = &menu->entries[card][entry];
            row->result = menu_path(menu, card, entry, path, sizeof(path)) ?
                sh_savefile_read(path, &saved) : SH_SAVEFILE_IO_ERROR;
            row->location = row->result == SH_SAVEFILE_OK ? saved.save.location_id : 0;
        }
    }
}

static void menu_transfer(ShSaveMenu *menu)
{
    char path[512];
    if (!menu_path(menu, menu->card, menu->entry, path, sizeof(path))) {
        menu->result = SH_SAVEFILE_IO_ERROR;
    } else if (menu->writing) {
        menu->result = sh_savefile_write(path, &menu->snapshot);
    } else {
        menu->result = sh_savefile_read(path, &menu->snapshot);
    }
    menu->loaded = !menu->writing && menu->result == SH_SAVEFILE_OK;
    menu->step = SH_SAVE_MENU_STATUS;
    sh_snd_play(menu->result == SH_SAVEFILE_OK ? SH_SFX_MENU_CONFIRM : SH_SFX_MENU_ERROR, 0, 0);
    if (menu->writing && menu->result == SH_SAVEFILE_OK) { menu_refresh(menu); }
}

void sh_save_menu_begin(ShSaveMenu *menu, bool writing, const ShAutosave *snapshot, const char *root)
{
    memset(menu, 0, sizeof(*menu));
    menu->writing = writing;
    menu->root = root;
    if (snapshot) { menu->snapshot = *snapshot; }
    menu->step = writing && !sh_save_event_get(&menu->snapshot.save, 186) ? SH_SAVE_MENU_INTRO : SH_SAVE_MENU_LIST;
    menu_refresh(menu);
}

ShSaveMenuAction sh_save_menu_update(ShSaveMenu *menu, const ShInputState *input, float dt)
{
    const bool enter = sh_input_down(input, SH_BTN_ACTION);
    const bool cancel = sh_input_down(input, SH_BTN_CANCEL);
    if (menu->step == SH_SAVE_MENU_INTRO) {
        if (cancel) { return SH_SAVE_MENU_CLOSED; }
        if (enter) {
            sh_save_event_set(&menu->snapshot.save, 186);
            menu->step = SH_SAVE_MENU_LIST;
        }
        return SH_SAVE_MENU_WAIT;
    }
    if (menu->step == SH_SAVE_MENU_OVERWRITE) {
        const ShMapChoiceFrame frame = sh_map_choice_step(&menu->choice, SH_Q12(dt),
            sh_input_down(input, KEY_DUP | KEY_DLEFT), sh_input_down(input, KEY_DDOWN | KEY_DRIGHT), enter, cancel);
        if (frame.sound) { sh_snd_play(frame.sound, 0, 0); }
        if (frame.selection == 0) { menu_transfer(menu); }
        else if (frame.selection >= 1) { menu->step = SH_SAVE_MENU_LIST; }
        return SH_SAVE_MENU_WAIT;
    }
    if (menu->step == SH_SAVE_MENU_STATUS) {
        if (enter || cancel) {
            if (menu->loaded) { return SH_SAVE_MENU_LOADED; }
            menu->step = SH_SAVE_MENU_LIST;
        }
        return SH_SAVE_MENU_WAIT;
    }
    if (cancel) { return SH_SAVE_MENU_CLOSED; }
    if (sh_input_down(input, KEY_DUP)) {
        menu->entry = (menu->entry + SH_SAVE_ENTRIES - 1) % SH_SAVE_ENTRIES;
    } else if (sh_input_down(input, KEY_DDOWN)) {
        menu->entry = (menu->entry + 1) % SH_SAVE_ENTRIES;
    } else if (sh_input_down(input, KEY_DLEFT | KEY_L)) {
        menu->card = (menu->card + SH_SAVE_CARDS - 1) % SH_SAVE_CARDS;
    } else if (sh_input_down(input, KEY_DRIGHT | KEY_R)) {
        menu->card = (menu->card + 1) % SH_SAVE_CARDS;
    } else if (enter) {
        if (menu->writing && menu->entries[menu->card][menu->entry].result == SH_SAVEFILE_OK) {
            sh_map_choice_begin(&menu->choice, 2);
            menu->step = SH_SAVE_MENU_OVERWRITE;
        } else { menu_transfer(menu); }
    } else { return SH_SAVE_MENU_WAIT; }
    sh_snd_play(SH_SFX_MENU_MOVE, 0, 0);
    return SH_SAVE_MENU_WAIT;
}

const char *sh_save_title_label(unsigned index)
{
    return index < sizeof(s_title_labels) / sizeof(s_title_labels[0]) ? s_title_labels[index] : "";
}

void sh_save_menu_format(const ShSaveMenu *menu, char *title, size_t title_size, char *body, size_t body_size)
{
    if (!title_size || !body_size) { return; }
    snprintf(title, title_size, "%s", menu->writing ? s_save_file : s_title_labels[0]);
    if (menu->step == SH_SAVE_MENU_INTRO) {
        snprintf(body, body_size, "%s", s_save_intro);
        return;
    }
    if (menu->step == SH_SAVE_MENU_OVERWRITE) {
        snprintf(body, body_size, "%s\n\n%c %s\n%c %s", s_save_overwrite[0],
            menu->choice.selected == 0 ? '>' : ' ', s_save_yes,
            menu->choice.selected == 1 ? '>' : ' ', s_save_no);
        return;
    }
    if (menu->step == SH_SAVE_MENU_STATUS) {
        const unsigned message = menu->result == SH_SAVEFILE_OK ? (menu->writing ? 5 : 10) :
            menu->result == SH_SAVEFILE_MISSING ? 7 : menu->result == SH_SAVEFILE_INVALID ? 8 :
            menu->writing ? 6 : 9;
        snprintf(body, body_size, "%s", s_save_dialogs[message]);
        return;
    }
    size_t used = (size_t)snprintf(body, body_size, "SLOT%u\n\n", menu->card + 1);
    const unsigned first = menu->entry > 3 ? menu->entry - 3 : 0;
    for (unsigned i = first; i < SH_SAVE_ENTRIES && i < first + 7 && used < body_size; i++) {
        const ShSaveMenuEntry *row = &menu->entries[menu->card][i];
        const char *location = row->result == SH_SAVEFILE_OK ?
            (row->location < sizeof(s_save_locations) / sizeof(s_save_locations[0]) ? s_save_locations[row->location] : "") :
            row->result == SH_SAVEFILE_MISSING ? "" : s_save_dialogs[8];
        const int length = snprintf(body + used, body_size - used, "%c %s %u  %s\n",
            i == menu->entry ? '>' : ' ', s_save_file, i + 1, location);
        if (length < 0 || (size_t)length >= body_size - used) { break; }
        used += (size_t)length;
    }
}
