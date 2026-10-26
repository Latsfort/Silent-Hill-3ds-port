#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sh3ds/fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Portable subset of bodyprog s_Savegame / event flags / inventory.
 * Layout is not binary-compatible with PS1 saves yet — logic API only.
 */

#define SH_INV_SLOT_MAX 40
#define SH_EVENT_FLAG_WORDS 52
#define SH_EVENT_FLAG_COUNT (SH_EVENT_FLAG_WORDS * 32)

/* Subset of e_InvItemId used by the shell. */
typedef enum ShInvItemId {
    SH_ITEM_UNEQUIPPED = 0,
    SH_ITEM_HEALTH_DRINK = 32,
    SH_ITEM_FIRST_AID = 33,
    SH_ITEM_AMPOULE = 34,
    SH_ITEM_POCKET_KNIFE = 128,
    SH_ITEM_STEEL_PIPE = 129,
    SH_ITEM_HOUSE_KEY = 65,
    SH_ITEM_NOTE_DOGHOUSE = 84,
    SH_ITEM_NOTE_SCHOOL = 83,
    SH_ITEM_HANDGUN = 160,
    SH_ITEM_HANDGUN_BULLETS = 192,
    SH_ITEM_RIFLE_SHELLS = 193,
    SH_ITEM_SHOTGUN_SHELLS = 194,
    SH_ITEM_FLASHLIGHT = 224,
    SH_ITEM_RADIO = 225
} ShInvItemId;

/* itemToggleFlags — decomp items.h */
#define SH_ITEM_TOGGLE_RADIO_ON (1u << 0)
#define SH_ITEM_TOGGLE_FLASHLIGHT_OFF (1u << 1)

typedef struct ShInvItem {
    uint8_t id;    /* ShInvItemId */
    uint8_t count; /* stack size; 0 = empty slot */
    uint8_t unk0;
    uint8_t unk1;
} ShInvItem;

typedef struct ShSavegame {
    ShInvItem items[SH_INV_SLOT_MAX];
    uint32_t event_flags[SH_EVENT_FLAG_WORDS];
    sh_q19_12 health;            /* Q12, default 100 */
    sh_q19_12 health_saturation; /* Q12 seconds of ampoule regeneration */
    uint8_t equipped_weapon;
    uint8_t item_toggle_flags; /* radio on, flashlight off */
    uint8_t inv_slot_count;
    uint16_t picked_up_item_count;
    bool has_map; /* paper map (event 38 mirrored for inventory Map option) */
    uint32_t paper_map_flags;
    sh_q19_12 gameplay_timer; /* Q12 seconds accumulated */
    sh_q19_12 walk_distance;  /* Q12 metres */
    sh_q19_12 run_distance;
    int8_t map_idx;
    int8_t map_room_idx; /* Map_RoomIdxGet / LoadRoom dest field_4_5 - 1 */
    uint8_t location_id;
    uint32_t map_enemy_slots; /* Game_NpcRoomInitSpawn: bits 16..31 groaner slots, 1 = dead */
} ShSavegame;

typedef struct ShAutosave {
    ShSavegame save;
    int32_t position[3];
    int16_t yaw;
    bool valid;
} ShAutosave;

void sh_save_autosave_set(const ShSavegame *save, int32_t x, int32_t y, int32_t z, int16_t yaw);
const ShAutosave *sh_save_autosave_get(void);

void sh_save_init(ShSavegame *save);
void sh_save_reset_session(ShSavegame *save);

bool sh_save_event_get(const ShSavegame *save, unsigned flag_idx);
void sh_save_event_set(ShSavegame *save, unsigned flag_idx);
void sh_save_event_clear(ShSavegame *save, unsigned flag_idx);

/* Adds `count` of `id` into the first matching / empty slot. Returns slot or -1. */
int sh_save_item_add(ShSavegame *save, uint8_t id, uint8_t count);
int sh_save_item_count(const ShSavegame *save, uint8_t id);
bool sh_save_item_use_health(ShSavegame *save, uint8_t id);
bool sh_save_item_remove_one(ShSavegame *save, uint8_t id);

bool sh_save_radio_on(const ShSavegame *save);
bool sh_save_flashlight_on(const ShSavegame *save);
void sh_save_radio_set(ShSavegame *save, bool on);
void sh_save_flashlight_set(ShSavegame *save, bool on);
void sh_save_flashlight_toggle(ShSavegame *save);

void sh_save_tick(ShSavegame *save, float dt_sec, float moved_m, bool running);

float sh_save_health_f(const ShSavegame *save);

#ifdef __cplusplus
}
#endif
