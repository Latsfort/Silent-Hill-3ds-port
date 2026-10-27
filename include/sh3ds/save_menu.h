#pragma once

#include <stddef.h>

#include "sh3ds/choice.h"
#include "sh3ds/input.h"
#include "sh3ds/savefile.h"
#include "sh3ds/save_menu_limits.h"

typedef enum ShSaveMenuStep {
    SH_SAVE_MENU_INTRO,
    SH_SAVE_MENU_LIST,
    SH_SAVE_MENU_OVERWRITE,
    SH_SAVE_MENU_STATUS
} ShSaveMenuStep;

typedef enum ShSaveMenuAction {
    SH_SAVE_MENU_WAIT,
    SH_SAVE_MENU_CLOSED,
    SH_SAVE_MENU_LOADED
} ShSaveMenuAction;

typedef struct ShSaveMenuEntry {
    ShSavefileResult result;
    uint8_t location;
} ShSaveMenuEntry;

typedef struct ShSaveMenu {
    ShSaveMenuStep step;
    bool writing;
    bool loaded;
    unsigned card, entry;
    ShSavefileResult result;
    ShMapChoice choice;
    ShSaveMenuEntry entries[SH_SAVE_CARDS][SH_SAVE_ENTRIES];
    ShAutosave snapshot;
    const char *root;
} ShSaveMenu;

void sh_save_menu_begin(ShSaveMenu *menu, bool writing, const ShAutosave *snapshot, const char *root);
ShSaveMenuAction sh_save_menu_update(ShSaveMenu *menu, const ShInputState *input, float dt);
void sh_save_menu_format(const ShSaveMenu *menu, char *title, size_t title_size, char *body, size_t body_size);
const char *sh_save_title_label(unsigned index);
