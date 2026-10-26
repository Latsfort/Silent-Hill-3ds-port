#include "../source/game/town.c"

#include <assert.h>
#include <stdio.h>

bool sh_input_down(const ShInputState *input, uint32_t key)
{
    return input && (input->down & key) != 0;
}

int main(void)
{
    for (unsigned k = 0; k < sizeof(s_town_keys) / sizeof(s_town_keys[0]); k++) {
        const ShTownKey *key = &s_town_keys[k];
        for (int accept = 0; accept <= 1; accept++) {
            for (int marked = 0; marked <= 1; marked++) {
                ShTown town = {0};
                ShSavegame save;
                ShInputState input = {0};
                sh_save_init(&save);
                town.pickup_active = true;
                town.controls_locked = true;
                town.pickup_item_id = key->item_id;
                town.pickup_count = 1;
                town.pickup_flag = key->flag;
                if (marked) {
                    sh_save_event_set(&save, (unsigned)key->marker_required);
                }
                item_take_update(&town, &save, &input, 1.0f / 30.0f);
                assert(town.step == 2);
                assert(!sh_save_event_get(&save, (unsigned)key->flag));
                item_take_update(&town, &save, &input, 1.0f / 30.0f);
                assert(sh_save_event_get(&save, (unsigned)key->flag));
                assert(sh_save_event_get(&save, (unsigned)key->marker_set) == (bool)marked);
                assert(!town.prompt_open);
                for (int frame = 0; frame < 300; frame++) {
                    item_take_update(&town, &save, &input, 1.0f / 30.0f);
                }
                assert(town.pickup_active && town.controls_locked && town.prompt_open);
                assert(sh_save_item_count(&save, (uint8_t)key->item_id) == 0);
                assert(strcmp(town.subtitle, key->message) == 0);
                input.down = accept ? SH_BTN_ACTION : SH_BTN_CANCEL;
                item_take_update(&town, &save, &input, 1.0f / 30.0f);
                assert(town.step == (accept ? 3 : -1));
                input.down = 0;
                item_take_update(&town, &save, &input, 1.0f / 30.0f);
                assert(!town.pickup_active && !town.controls_locked);
                assert(sh_save_item_count(&save, (uint8_t)key->item_id) == accept);
                assert(sh_save_event_get(&save, (unsigned)key->flag) == (bool)accept);
                assert(sh_save_event_get(&save, (unsigned)key->marker_set) == (bool)marked);
                assert(!town.subtitle);
            }
        }
    }
    puts("PASS: both retail keys wait for input; accept/cancel preserve inventory, pickup and map flags");
    return 0;
}
