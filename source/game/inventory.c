#include "sh3ds/inventory.h"
#include "sh3ds/cafe.h"
#include "sh3ds/controls.h"
#include "sh3ds/snd.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

/* Subset of e_InvCmdId */
enum {
    CMD_USE_HEALTH = 0,
    CMD_USE = 1,
    CMD_EQUIP = 2,
    CMD_UNEQUIP = 3,
    CMD_ONOFF = 6,
    CMD_LOOK = 8
};

typedef struct PackedSlot {
    int save_idx;
    uint8_t id;
    uint8_t count;
} PackedSlot;

static int pack_items(const ShSavegame *save, PackedSlot *out, int max_out)
{
    int n = 0;
    if (!save) {
        return 0;
    }
    for (int i = 0; i < SH_INV_SLOT_MAX && n < max_out; i++) {
        if (save->items[i].count > 0 && save->items[i].id != SH_ITEM_UNEQUIPPED) {
            out[n].save_idx = i;
            out[n].id = save->items[i].id;
            out[n].count = save->items[i].count;
            n++;
        }
    }
    return n;
}

const char *sh_inv_item_name(uint8_t id)
{
    /* INV_ITEM_NAMES[id - 32] for id >= 32 (decomp). */
    switch (id) {
    case SH_ITEM_HEALTH_DRINK:
        return "Health drink(s)";
    case SH_ITEM_FIRST_AID:
        return "First aid kit(s)";
    case SH_ITEM_AMPOULE:
        return "Ampoule(s)";
    case SH_ITEM_POCKET_KNIFE:
        return "Kitchen knife";
    case SH_ITEM_HANDGUN:
        return "Handgun";
    case SH_ITEM_HANDGUN_BULLETS:
        return "Handgun bullets";
    case SH_ITEM_FLASHLIGHT:
        return "Flashlight";
    case SH_ITEM_RADIO:
        return "Pocket radio";
#define SH_TOWN_KEY_NAMES
#include "town_keys.h"
#undef SH_TOWN_KEY_NAMES
#define SH_TOWN_STORY_NAMES
#include "town_story_data.h"
#undef SH_TOWN_STORY_NAMES
#define SH_SCHOOL_ITEM_NAMES
#include "school_data.h"
#undef SH_SCHOOL_ITEM_NAMES
#define SH_SCHOOL_UPPER_NAMES
#include "school_upper_data.h"
#undef SH_SCHOOL_UPPER_NAMES
#define SH_SCHOOL_ALT_NAMES
#include "school_alternative_data.h"
#undef SH_SCHOOL_ALT_NAMES
#define SH_SCHOOL_ROOF_PICKUP_NAMES
#include "school_roof_pickup_data.h"
#undef SH_SCHOOL_ROOF_PICKUP_NAMES
    default:
        return "???";
    }
}

static const char *cmd_name(uint8_t cmd, uint8_t item_id, const ShSavegame *save)
{
    switch (cmd) {
    case CMD_USE_HEALTH:
    case CMD_USE:
        return "Use";
    case CMD_EQUIP:
        return "Equip";
    case CMD_UNEQUIP:
        return "Unequip";
    case CMD_ONOFF:
        if (item_id == SH_ITEM_FLASHLIGHT) {
            return sh_save_flashlight_on(save) ? "Off" : "On";
        }
        if (item_id == SH_ITEM_RADIO) {
            return sh_save_radio_on(save) ? "Off" : "On";
        }
        return "On/Off";
    case CMD_LOOK:
        return "Look";
    default:
        return "???";
    }
}

static bool story_item_use(uint8_t id)
{
    switch (id) {
#define SH_SCHOOL_ITEM_USE
#include "school_data.h"
#undef SH_SCHOOL_ITEM_USE
#define SH_SCHOOL_UPPER_ITEM_USE
#include "school_upper_data.h"
#undef SH_SCHOOL_UPPER_ITEM_USE
    default:
        return false;
    }
}

static void build_commands(ShInventory *inv, const ShSavegame *save, uint8_t id)
{
    inv->cmd_count = 0;
    inv->cmd_cursor = 0;
    if (id == SH_ITEM_HEALTH_DRINK || id == SH_ITEM_FIRST_AID || id == SH_ITEM_AMPOULE) {
        inv->cmd_ids[inv->cmd_count++] = CMD_USE_HEALTH;
    } else if (id == SH_ITEM_POCKET_KNIFE || id == SH_ITEM_HANDGUN) {
        if (save->equipped_weapon == id) {
            inv->cmd_ids[inv->cmd_count++] = CMD_UNEQUIP;
        } else {
            inv->cmd_ids[inv->cmd_count++] = CMD_EQUIP;
        }
    } else if (id == SH_ITEM_FLASHLIGHT || id == SH_ITEM_RADIO) {
        inv->cmd_ids[inv->cmd_count++] = CMD_ONOFF;
    } else if (id == SH_ITEM_HANDGUN_BULLETS) {
        inv->cmd_ids[inv->cmd_count++] = CMD_LOOK;
    } else if (story_item_use(id)) {
        inv->cmd_ids[inv->cmd_count++] = CMD_USE;
    }
}

/* Item screen Sd_SfxPlay(id, balance, Q8(0.25)). */
static void menu_sfx(uint16_t sfx, int balance)
{
    sh_snd_play(sfx, balance, 64);
}

static void set_status(ShInventory *inv, const char *text)
{
    inv->status = text;
    inv->status_time = 1.6f;
}

void sh_inv_init(ShInventory *inv)
{
    if (!inv) {
        return;
    }
    memset(inv, 0, sizeof(*inv));
}

void sh_inv_open(ShInventory *inv, const ShSavegame *save)
{
    if (!inv) {
        return;
    }
    memset(inv, 0, sizeof(*inv));
    inv->open = true;
    inv->focus = SH_INV_FOCUS_ITEMS;
    inv->cursor = 0;
    (void)save;
}

void sh_inv_close(ShInventory *inv)
{
    if (!inv) {
        return;
    }
    inv->open = false;
    inv->focus = SH_INV_FOCUS_ITEMS;
    inv->cmd_count = 0;
}

static void run_command(ShInventory *inv, ShSavegame *save, uint8_t id, uint8_t cmd)
{
    switch (cmd) {
    case CMD_USE:
        /* Inventory_ItemUse: a matching active event turns the lamp on and returns to gameplay. */
        for (unsigned i = 0; i < sizeof(inv->available_use_items); i++) {
            if (inv->available_use_items[i] == id && sh_save_item_count(save, id) > 0) {
                sh_save_flashlight_set(save, true);
                inv->request_use_item = id;
                sh_inv_close(inv);
                return;
            }
        }
        /* item_screens_3.c STRS, Inventory_ItemUse failure. */
        set_status(inv, "Can't use it here.");
        break;
    case CMD_USE_HEALTH:
        if (sh_save_item_use_health(save, id)) {
            set_status(inv, "Used.");
            inv->focus = SH_INV_FOCUS_ITEMS;
        } else {
            set_status(inv, "Can't use.");
        }
        break;
    case CMD_EQUIP:
        save->equipped_weapon = id;
        set_status(inv, "Equipped.");
        inv->focus = SH_INV_FOCUS_ITEMS;
        break;
    case CMD_UNEQUIP:
        save->equipped_weapon = SH_ITEM_UNEQUIPPED;
        set_status(inv, "Unequipped.");
        inv->focus = SH_INV_FOCUS_ITEMS;
        break;
    case CMD_ONOFF:
        if (id == SH_ITEM_FLASHLIGHT) {
            const bool on = !sh_save_flashlight_on(save);
            sh_save_flashlight_set(save, on);
            set_status(inv, on ? "Flashlight: On" : "Flashlight: Off");
        } else if (id == SH_ITEM_RADIO) {
            const bool on = !sh_save_radio_on(save);
            sh_save_radio_set(save, on);
            set_status(inv, on ? "Radio: On" : "Radio: Off");
        }
        inv->focus = SH_INV_FOCUS_ITEMS;
        break;
    case CMD_LOOK:
        set_status(inv, "Handgun ammunition.");
        inv->focus = SH_INV_FOCUS_ITEMS;
        break;
    default:
        break;
    }
}

bool sh_inv_update(ShInventory *inv, ShSavegame *save, const ShInputState *input, float dt)
{
    if (!inv || !inv->open || !save || !input) {
        return false;
    }

    if (inv->status_time > 0.0f) {
        inv->status_time -= dt;
        if (inv->status_time <= 0.0f) {
            inv->status = NULL;
        }
    }

    PackedSlot packed[SH_INV_SLOT_MAX];
    const int n = pack_items(save, packed, SH_INV_SLOT_MAX);
    if (inv->cursor >= n) {
        inv->cursor = n > 0 ? n - 1 : 0;
    }

    /* SELECT closes inventory (original). */
    if (sh_input_down(input, SH_BTN_INVENTORY)) {
        sh_inv_close(inv);
        return true;
    }

    if (inv->focus == SH_INV_FOCUS_CMD) {
        if (sh_input_down(input, KEY_DUP) || sh_input_down(input, KEY_DLEFT)) {
            if (inv->cmd_count > 1) {
                inv->cmd_cursor = (inv->cmd_cursor + inv->cmd_count - 1) % inv->cmd_count;
                menu_sfx(SH_SFX_MENU_MOVE, 0);
            }
        } else if (sh_input_down(input, KEY_DDOWN) || sh_input_down(input, KEY_DRIGHT)) {
            if (inv->cmd_count > 1) {
                inv->cmd_cursor = (inv->cmd_cursor + 1) % inv->cmd_count;
                menu_sfx(SH_SFX_MENU_MOVE, 0);
            }
        } else if (sh_input_down(input, SH_BTN_ACTION)) {
            if (n > 0 && inv->cmd_count > 0) {
                menu_sfx(SH_SFX_MENU_CONFIRM, 64);
                run_command(inv, save, packed[inv->cursor].id, inv->cmd_ids[inv->cmd_cursor]);
            }
        } else if (sh_input_down(input, SH_BTN_CANCEL)) {
            menu_sfx(SH_SFX_MENU_CANCEL, 0);
            inv->focus = SH_INV_FOCUS_ITEMS;
            inv->cmd_count = 0;
        }
        return true;
    }

    if (inv->focus == SH_INV_FOCUS_ITEMS) {
        if (sh_input_down(input, KEY_DLEFT) && n > 0) {
            inv->cursor = (inv->cursor + n - 1) % n;
            menu_sfx(SH_SFX_MENU_MOVE, -64);
        } else if (sh_input_down(input, KEY_DRIGHT) && n > 0) {
            inv->cursor = (inv->cursor + 1) % n;
            menu_sfx(SH_SFX_MENU_MOVE, 64);
        } else if (sh_input_down(input, KEY_DUP)) {
            inv->focus = SH_INV_FOCUS_EQUIPPED;
            menu_sfx(SH_SFX_MENU_MOVE, 0);
        } else if (sh_input_down(input, KEY_DDOWN)) {
            inv->focus = SH_INV_FOCUS_EXIT;
            menu_sfx(SH_SFX_MENU_MOVE, 0);
        } else if (sh_input_down(input, SH_BTN_ACTION) && n > 0) {
            build_commands(inv, save, packed[inv->cursor].id);
            if (inv->cmd_count > 0) {
                inv->focus = SH_INV_FOCUS_CMD;
                menu_sfx(SH_SFX_MENU_CONFIRM, 0);
            } else {
                menu_sfx(SH_SFX_MENU_ERROR, 64);
            }
        } else if (sh_input_down(input, SH_BTN_CANCEL)) {
            inv->focus = SH_INV_FOCUS_EXIT;
            menu_sfx(SH_SFX_MENU_CANCEL, 0);
        }
        return true;
    }

    if (inv->focus == SH_INV_FOCUS_EQUIPPED) {
        if (sh_input_down(input, KEY_DDOWN) || sh_input_down(input, SH_BTN_CANCEL)) {
            inv->focus = SH_INV_FOCUS_ITEMS;
            menu_sfx(sh_input_down(input, SH_BTN_CANCEL) ? SH_SFX_MENU_CANCEL : SH_SFX_MENU_MOVE, 0);
        } else if (sh_input_down(input, SH_BTN_ACTION)) {
            menu_sfx(save->equipped_weapon != SH_ITEM_UNEQUIPPED ? SH_SFX_MENU_CONFIRM : SH_SFX_MENU_ERROR, 64);
            if (save->equipped_weapon != SH_ITEM_UNEQUIPPED) {
                /* Jump cursor to equipped item and open Unequip. */
                for (int i = 0; i < n; i++) {
                    if (packed[i].id == save->equipped_weapon) {
                        inv->cursor = i;
                        build_commands(inv, save, packed[i].id);
                        inv->focus = SH_INV_FOCUS_CMD;
                        break;
                    }
                }
            }
        }
        return true;
    }

    /* Ring: Exit / Map / Settings */
    if (inv->focus == SH_INV_FOCUS_EXIT || inv->focus == SH_INV_FOCUS_MAP ||
        inv->focus == SH_INV_FOCUS_SETTINGS) {
        if (sh_input_down(input, KEY_DUP)) {
            inv->focus = SH_INV_FOCUS_ITEMS;
            menu_sfx(SH_SFX_MENU_MOVE, 0);
        } else if (sh_input_down(input, KEY_DLEFT)) {
            menu_sfx(SH_SFX_MENU_MOVE, -64);
            if (inv->focus == SH_INV_FOCUS_EXIT) {
                inv->focus = SH_INV_FOCUS_SETTINGS;
            } else if (inv->focus == SH_INV_FOCUS_MAP) {
                inv->focus = SH_INV_FOCUS_EXIT;
            }
        } else if (sh_input_down(input, KEY_DRIGHT)) {
            menu_sfx(SH_SFX_MENU_MOVE, 64);
            if (inv->focus == SH_INV_FOCUS_EXIT) {
                inv->focus = SH_INV_FOCUS_MAP;
            } else if (inv->focus == SH_INV_FOCUS_SETTINGS) {
                inv->focus = SH_INV_FOCUS_EXIT;
            }
        } else if (sh_input_down(input, SH_BTN_ACTION) ||
                   (inv->focus == SH_INV_FOCUS_EXIT && sh_input_down(input, SH_BTN_CANCEL))) {
            if (inv->focus == SH_INV_FOCUS_EXIT) {
                menu_sfx(SH_SFX_MENU_CANCEL, 0);
                sh_inv_close(inv);
            } else if (inv->focus == SH_INV_FOCUS_MAP) {
                if (save->has_map || sh_save_event_get(save, SH_EV_MAP)) {
                    menu_sfx(SH_SFX_MENU_MAP, 0);
                    inv->request_map = true;
                    sh_inv_close(inv);
                } else {
                    menu_sfx(SH_SFX_MENU_ERROR, 64);
                    set_status(inv, "I don't have a map.");
                }
            } else {
                menu_sfx(SH_SFX_MENU_ERROR, 64);
                set_status(inv, "Options — coming soon.");
            }
        } else if (sh_input_down(input, SH_BTN_CANCEL)) {
            menu_sfx(SH_SFX_MENU_CANCEL, 0);
            sh_inv_close(inv);
        }
        return true;
    }

    return true;
}

static const char *health_word(float hp)
{
    if (hp > 75.0f) {
        return "Fine";
    }
    if (hp > 40.0f) {
        return "Caution";
    }
    if (hp > 15.0f) {
        return "Caution!";
    }
    return "Danger";
}

void sh_inv_format_top(const ShInventory *inv, const ShSavegame *save, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) {
        return;
    }
    if (!inv || !inv->open || !save) {
        out[0] = '\0';
        return;
    }
    const float hp = sh_save_health_f(save);
    const char *wep = save->equipped_weapon == SH_ITEM_UNEQUIPPED
                          ? "Unequipped"
                          : sh_inv_item_name(save->equipped_weapon);
    snprintf(out, out_sz,
             "STATUS\n"
             "Health  %s  (%.0f)\n"
             "Equipped  %s\n"
             "Flashlight %s    Radio %s",
             health_word(hp), hp, wep, sh_save_flashlight_on(save) ? "On" : "Off",
             sh_save_radio_on(save) ? "On" : "Off");
}

void sh_inv_format_bottom(const ShInventory *inv, const ShSavegame *save, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) {
        return;
    }
    out[0] = '\0';
    if (!inv || !inv->open || !save) {
        return;
    }

    PackedSlot packed[SH_INV_SLOT_MAX];
    const int n = pack_items(save, packed, SH_INV_SLOT_MAX);
    char line[96];
    size_t used = 0;

    if (inv->status && inv->status_time > 0.0f) {
        snprintf(line, sizeof(line), "%s\n\n", inv->status);
        strncat(out, line, out_sz - used - 1);
        used = strlen(out);
    }

    if (n == 0) {
        strncat(out, "(no items)\n", out_sz - used - 1);
    } else {
        /* Show a window of up to 6 items around cursor. */
        int start = inv->cursor - 2;
        if (start < 0) {
            start = 0;
        }
        if (start + 6 > n) {
            start = n > 6 ? n - 6 : 0;
        }
        for (int i = start; i < n && i < start + 6; i++) {
            const char mark = (inv->focus == SH_INV_FOCUS_ITEMS || inv->focus == SH_INV_FOCUS_CMD) &&
                                      i == inv->cursor
                                  ? '>'
                                  : ' ';
            const char eq = packed[i].id == save->equipped_weapon ? '*' : ' ';
            snprintf(line, sizeof(line), "%c%c %s x%d\n", mark, eq, sh_inv_item_name(packed[i].id),
                     packed[i].count);
            used = strlen(out);
            strncat(out, line, out_sz - used - 1);
        }
    }

    used = strlen(out);
    if (inv->focus == SH_INV_FOCUS_CMD && n > 0) {
        strncat(out, "\n", out_sz - used - 1);
        for (int c = 0; c < inv->cmd_count; c++) {
            snprintf(line, sizeof(line), "  %c %s\n", c == inv->cmd_cursor ? '>' : ' ',
                     cmd_name(inv->cmd_ids[c], packed[inv->cursor].id, save));
            used = strlen(out);
            strncat(out, line, out_sz - used - 1);
        }
    } else {
        const char *ex = inv->focus == SH_INV_FOCUS_EXIT       ? ">Exit"
                         : inv->focus == SH_INV_FOCUS_MAP      ? " Exit"
                                                               : " Exit";
        const char *mp = inv->focus == SH_INV_FOCUS_MAP        ? ">Map"
                         : inv->focus == SH_INV_FOCUS_EXIT || inv->focus == SH_INV_FOCUS_SETTINGS
                             ? " Map"
                             : " Map";
        const char *st = inv->focus == SH_INV_FOCUS_SETTINGS ? ">Option" : " Option";
        const char *eqf = inv->focus == SH_INV_FOCUS_EQUIPPED ? ">Equipped" : " Equipped";
        snprintf(line, sizeof(line), "\n%s\n%s   %s   %s", eqf, ex, mp, st);
        used = strlen(out);
        strncat(out, line, out_sz - used - 1);
    }
}

void sh_inv_format_hint(const ShInventory *inv, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) {
        return;
    }
    if (!inv || !inv->open) {
        out[0] = '\0';
        return;
    }
    if (inv->focus == SH_INV_FOCUS_CMD) {
        snprintf(out, out_sz, "A : OK    B : retour\nSELECT : fermer");
    } else {
        snprintf(out, out_sz, "D-pad : choisir    A : commande\nSELECT / Exit : fermer");
    }
}
