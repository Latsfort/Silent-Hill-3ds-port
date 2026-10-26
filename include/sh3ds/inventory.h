#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sh3ds/input.h"
#include "sh3ds/savegame.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Status / inventory screen — flow from decomp SysState_StatusMenu /
 * GameState_InventoryScreen (item_screens_2.c). 2D list + commands first;
 * 3D item carousel deferred.
 */

typedef enum ShInvFocus {
    SH_INV_FOCUS_ITEMS = 0,
    SH_INV_FOCUS_EQUIPPED,
    SH_INV_FOCUS_EXIT,
    SH_INV_FOCUS_MAP,
    SH_INV_FOCUS_SETTINGS,
    SH_INV_FOCUS_CMD
} ShInvFocus;

typedef struct ShInventory {
    bool open;
    ShInvFocus focus;
    int cursor;     /* packed slot index among non-empty items */
    int cmd_cursor; /* command submenu */
    int cmd_count;
    uint8_t cmd_ids[4]; /* e_InvCmdId subset */
    const char *status; /* one-line result ("Equipped.", "On.", …) */
    float status_time;
    bool request_map; /* open paper map after close */
    uint8_t available_use_items[5]; /* Event_Update item triggers at menu entry. */
    uint8_t request_use_item;
} ShInventory;

void sh_inv_init(ShInventory *inv);
void sh_inv_open(ShInventory *inv, const ShSavegame *save);
void sh_inv_close(ShInventory *inv);

/* true while inventory consumes input (gameplay paused). */
bool sh_inv_update(ShInventory *inv, ShSavegame *save, const ShInputState *input, float dt);

/* HUD lines for gfx (top = status portrait strip, bottom = list). */
void sh_inv_format_top(const ShInventory *inv, const ShSavegame *save, char *out, size_t out_sz);
void sh_inv_format_bottom(const ShInventory *inv, const ShSavegame *save, char *out, size_t out_sz);
void sh_inv_format_hint(const ShInventory *inv, char *out, size_t out_sz);

const char *sh_inv_item_name(uint8_t id);

#ifdef __cplusplus
}
#endif
