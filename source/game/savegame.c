#include "sh3ds/savegame.h"

#include <string.h>

static ShAutosave s_autosave;

/* func_8003A16C / SysWork_SavegameUpdatePlayer: retain the in-memory continue snapshot. */
void sh_save_autosave_set(const ShSavegame *save, int32_t x, int32_t y, int32_t z, int16_t yaw)
{
    s_autosave.save = *save;
    s_autosave.position[0] = x;
    s_autosave.position[1] = y;
    s_autosave.position[2] = z;
    s_autosave.yaw = yaw;
    s_autosave.valid = true;
}

const ShAutosave *sh_save_autosave_get(void) { return &s_autosave; }

void sh_save_init(ShSavegame *save)
{
    if (!save) {
        return;
    }
    memset(save, 0, sizeof(*save));
    save->health = SH_Q12(100.0f);
    save->inv_slot_count = 8;
    save->equipped_weapon = SH_ITEM_UNEQUIPPED;
    save->item_toggle_flags = 0;
    save->has_map = false;
    /* PS1 New Game: empty pockets until Cafe cutscene / pickups. */
}

void sh_save_reset_session(ShSavegame *save)
{
    if (!save) {
        return;
    }
    save->gameplay_timer = 0;
    save->walk_distance = 0;
    save->run_distance = 0;
    s_autosave.valid = false;
}

bool sh_save_event_get(const ShSavegame *save, unsigned flag_idx)
{
    if (!save || flag_idx >= SH_EVENT_FLAG_COUNT) {
        return false;
    }
    return (save->event_flags[flag_idx >> 5] & (1u << (flag_idx & 31u))) != 0u;
}

void sh_save_event_set(ShSavegame *save, unsigned flag_idx)
{
    if (!save || flag_idx >= SH_EVENT_FLAG_COUNT) {
        return;
    }
    save->event_flags[flag_idx >> 5] |= 1u << (flag_idx & 31u);
}

void sh_save_event_clear(ShSavegame *save, unsigned flag_idx)
{
    if (!save || flag_idx >= SH_EVENT_FLAG_COUNT) {
        return;
    }
    save->event_flags[flag_idx >> 5] &= ~(1u << (flag_idx & 31u));
}

int sh_save_item_add(ShSavegame *save, uint8_t id, uint8_t count)
{
    if (!save || id == SH_ITEM_UNEQUIPPED || count == 0) {
        return -1;
    }
    for (int i = 0; i < SH_INV_SLOT_MAX; i++) {
        if (save->items[i].id == id && save->items[i].count > 0) {
            const unsigned sum = (unsigned)save->items[i].count + count;
            save->items[i].count = (uint8_t)(sum > 255u ? 255u : sum);
            return i;
        }
    }
    for (int i = 0; i < SH_INV_SLOT_MAX; i++) {
        if (save->items[i].count == 0) {
            save->items[i].id = id;
            save->items[i].count = count;
            save->picked_up_item_count++;
            return i;
        }
    }
    return -1;
}

int sh_save_item_count(const ShSavegame *save, uint8_t id)
{
    if (!save) {
        return 0;
    }
    int total = 0;
    for (int i = 0; i < SH_INV_SLOT_MAX; i++) {
        if (save->items[i].id == id) {
            total += save->items[i].count;
        }
    }
    return total;
}

bool sh_save_item_use_health(ShSavegame *save, uint8_t id)
{
    if (!save) {
        return false;
    }
    sh_q19_12 heal = 0;
    if (id == SH_ITEM_HEALTH_DRINK) {
        heal = SH_Q12(40.0f);
    } else if (id == SH_ITEM_FIRST_AID) {
        heal = SH_Q12(80.0f);
    } else if (id == SH_ITEM_AMPOULE) {
        heal = SH_Q12(100.0f);
    } else {
        return false;
    }
    for (int i = 0; i < SH_INV_SLOT_MAX; i++) {
        if (save->items[i].id == id && save->items[i].count > 0) {
            /* Inventory_PlayerItemScroll case 9. */
            if (id == SH_ITEM_AMPOULE) {
                save->health_saturation = SH_Q12(300.0f);
            }
            save->items[i].count--;
            if (save->items[i].count == 0) {
                save->items[i].id = SH_ITEM_UNEQUIPPED;
            }
            save->health = sh_q12_clamp(save->health + heal, 0, SH_Q12(100.0f));
            return true;
        }
    }
    return false;
}

void sh_save_tick(ShSavegame *save, float dt_sec, float moved_m, bool running)
{
    if (!save || dt_sec <= 0.0f) {
        return;
    }
    save->gameplay_timer += SH_Q12(dt_sec);
    if (moved_m > 0.0f) {
        const sh_q19_12 d = SH_Q12(moved_m);
        if (running) {
            save->run_distance += d;
        } else {
            save->walk_distance += d;
        }
    }
    /* Player_LogicUpdate: saturation counts down even at full health. */
    save->health_saturation = sh_q12_clamp(save->health_saturation - SH_Q12(dt_sec),
                                         0, SH_Q12(300.0f));
    if (save->health_saturation > 0) {
        save->health = sh_q12_clamp(save->health + SH_Q12(dt_sec), 0, SH_Q12(100.0f));
    }
}

float sh_save_health_f(const ShSavegame *save)
{
    return save ? SH_Q12_TO_FLOAT(save->health) : 0.0f;
}

bool sh_save_item_remove_one(ShSavegame *save, uint8_t id)
{
    if (!save || id == SH_ITEM_UNEQUIPPED) {
        return false;
    }
    for (int i = 0; i < SH_INV_SLOT_MAX; i++) {
        if (save->items[i].id == id && save->items[i].count > 0) {
            save->items[i].count--;
            if (save->items[i].count == 0) {
                save->items[i].id = SH_ITEM_UNEQUIPPED;
            }
            return true;
        }
    }
    return false;
}

bool sh_save_radio_on(const ShSavegame *save)
{
    return save && (save->item_toggle_flags & SH_ITEM_TOGGLE_RADIO_ON) != 0;
}

bool sh_save_flashlight_on(const ShSavegame *save)
{
    return save && (save->item_toggle_flags & SH_ITEM_TOGGLE_FLASHLIGHT_OFF) == 0 &&
           sh_save_item_count(save, SH_ITEM_FLASHLIGHT) > 0;
}

void sh_save_radio_set(ShSavegame *save, bool on)
{
    if (!save) {
        return;
    }
    if (on) {
        save->item_toggle_flags |= SH_ITEM_TOGGLE_RADIO_ON;
    } else {
        save->item_toggle_flags &= (uint8_t)~SH_ITEM_TOGGLE_RADIO_ON;
    }
}

void sh_save_flashlight_set(ShSavegame *save, bool on)
{
    if (!save) {
        return;
    }
    if (on) {
        save->item_toggle_flags &= (uint8_t)~SH_ITEM_TOGGLE_FLASHLIGHT_OFF;
    } else {
        save->item_toggle_flags |= SH_ITEM_TOGGLE_FLASHLIGHT_OFF;
    }
}

void sh_save_flashlight_toggle(ShSavegame *save)
{
    if (!save || sh_save_item_count(save, SH_ITEM_FLASHLIGHT) <= 0) {
        return;
    }
    sh_save_flashlight_set(save, !sh_save_flashlight_on(save));
}
