#include "sh3ds/debug_checkpoints.h"

#include <stddef.h>
#include <string.h>

typedef struct ShDebugCheckpoint {
    const char *name;
    uint8_t map;
    float x;
    float z;
    float yaw;
    uint32_t paper_flags;
    uint16_t picked_count;
    const uint16_t *flags;
    size_t flag_count;
    const ShInvItem *items;
    size_t item_count;
} ShDebugCheckpoint;

#include "debug_checkpoints_data.h"

int sh_debug_checkpoint_count(void)
{
    return (int)(sizeof(s_debug_checkpoints) / sizeof(s_debug_checkpoints[0]));
}

const char *sh_debug_checkpoint_name(int index)
{
    return index >= 0 && index < sh_debug_checkpoint_count() ? s_debug_checkpoints[index].name : NULL;
}

bool sh_debug_checkpoint_apply(int index, ShSavegame *save, ShDebugArrival *arrival)
{
    if (!save || !arrival || index < 0 || index >= sh_debug_checkpoint_count()) {
        return false;
    }
    const ShDebugCheckpoint *point = &s_debug_checkpoints[index];
    sh_save_init(save);
    for (size_t i = 0; i < point->flag_count; i++) {
        sh_save_event_set(save, point->flags[i]);
    }
    for (size_t i = 0; i < point->item_count; i++) {
        sh_save_item_add(save, point->items[i].id, point->items[i].count);
    }
    save->has_map = true;
    save->paper_map_flags = point->paper_flags;
    save->picked_up_item_count = point->picked_count;
    save->map_idx = point->map;
    save->equipped_weapon = SH_ITEM_HANDGUN;
    sh_save_flashlight_set(save, true);
    sh_save_radio_set(save, true);
    *arrival = (ShDebugArrival){point->map, point->x, point->z, point->yaw};
    return true;
}
