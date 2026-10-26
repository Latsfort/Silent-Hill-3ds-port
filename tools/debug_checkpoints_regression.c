#include "sh3ds/debug_checkpoints.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    ShSavegame save;
    ShDebugArrival arrival;
    assert(sh_debug_checkpoint_count() == 9);
    memset(&save, 0xff, sizeof(save));
    for (int checkpoint = 8; checkpoint >= 0; checkpoint--) {
        const bool silver_ground = checkpoint == 8;
        const int i = silver_ground ? 5 : checkpoint;
        assert(sh_debug_checkpoint_name(checkpoint));
        assert(sh_debug_checkpoint_apply(checkpoint, &save, &arrival));
        assert(save.map_idx == arrival.map && save.has_map);
        assert(save.equipped_weapon == SH_ITEM_HANDGUN);
        assert(sh_save_flashlight_on(&save) && sh_save_radio_on(&save));
        assert(sh_save_item_count(&save, SH_ITEM_HANDGUN) == 1);
        assert(sh_save_item_count(&save, SH_ITEM_HANDGUN_BULLETS) == 15);
        assert(sh_save_item_count(&save, SH_ITEM_NOTE_SCHOOL) == 1);
        assert(sh_save_item_count(&save, SH_ITEM_NOTE_DOGHOUSE) == 0);
        assert(sh_save_item_count(&save, SH_ITEM_HOUSE_KEY) == 0);
        assert(save.picked_up_item_count == (i >= 6 ? 14 : i >= 4 ? 13 : i >= 3 ? 12 : 11));
        assert(sh_save_event_get(&save, 146) && sh_save_event_get(&save, 147));
        assert(sh_save_event_get(&save, 148) && sh_save_event_get(&save, 149));
        assert(sh_save_event_get(&save, 153));
        assert(sh_save_item_count(&save, 66) == (i == 0));
        assert(sh_save_item_count(&save, 67) == (i == 0));
        assert(sh_save_item_count(&save, 68) == (i == 0));
        assert(sh_save_event_get(&save, 154) == (i >= 1));
        assert(sh_save_event_get(&save, 157) == (i >= 1));
        assert(sh_save_event_get(&save, 159) == (i >= 1));
        assert(sh_save_item_count(&save, 96) == (i >= 3));
        assert(sh_save_event_get(&save, 68) == (i >= 3));
        assert(sh_save_event_get(&save, 74) == (i >= 4));
        assert(sh_save_item_count(&save, 97) == (i >= 4));
        assert(sh_save_event_get(&save, 69) == (i >= 4));
        assert(sh_save_event_get(&save, 71) == (i >= 5));
        assert(sh_save_event_get(&save, 75) == (i >= 6 || silver_ground));
        assert(sh_save_event_get(&save, 70) == (i >= 6));
        assert(sh_save_item_count(&save, 98) == (i >= 6));
        assert(sh_save_event_get(&save, 72) == (i >= 7));
        assert(!sh_save_event_get(&save, 73));
        assert(!sh_save_event_get(&save, 83));
        assert(!sh_save_event_get(&save, 84));
        assert(sh_save_event_get(&save, 67) == (i >= 3));
        assert(save.paper_map_flags == (i >= 3 ? 8162u : 2u));
        assert(!save.map_enemy_slots && !save.gameplay_timer && !save.health_saturation);
        assert(save.health == SH_Q12(100.0f));
    }
    const ShSavegame before = save;
    assert(!sh_debug_checkpoint_apply(-1, &save, &arrival));
    assert(!sh_debug_checkpoint_apply(9, &save, &arrival));
    assert(memcmp(&save, &before, sizeof(save)) == 0);
    puts("PASS: nine cumulative checkpoints, consumed keys, retained medallions, maps, pre-silver pickup and clean reverse reloads");
    return 0;
}
