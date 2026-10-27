#include "../build/cafe_combat_under_test.h"

#include <assert.h>
#include <stdio.h>

bool sh_input_down(const ShInputState *input, uint32_t key) { return (input->down & key) != 0; }
bool sh_input_held(const ShInputState *input, uint32_t key) { return (input->held & key) != 0; }
void sh_snd_at(uint16_t id, const float position[3], float volume)
{ (void)id; (void)position; (void)volume; }
void sh_snd_stop(uint16_t id) { (void)id; }

static void run(float dt)
{
    ShCafe cafe = {0};
    ShSavegame save;
    ShInputState input = {0};
    sh_save_init(&save);
    sh_save_item_add(&save, SH_ITEM_HANDGUN, 1);
    sh_save_item_add(&save, SH_ITEM_HANDGUN_BULLETS, 15);
    save.equipped_weapon = SH_ITEM_HANDGUN;
    cafe.stage = SH_CAFE_BIRD_FIGHT;
    cafe.bird_state = SH_BIRD_HOVER;
    cafe.bird_hp = 10000.0f;
    cafe.bird_pos[2] = -3.0f;
    input.held = SH_BTN_AIM | SH_BTN_ACTION;
    for (int frame = 0; frame < (int)(1.0f / dt); frame++) {
        input.down = frame == 0 ? input.held : 0;
        combat_update(&cafe, &save, 0.0f, 0.0f, 0.0f, &input, dt);
    }
    assert(sh_save_item_count(&save, SH_ITEM_HANDGUN_BULLETS) == 12);
    const float health = cafe.bird_hp;
    input.held = SH_BTN_AIM;
    input.down = 0;
    for (int frame = 0; frame < (int)(1.0f / dt); frame++) {
        combat_update(&cafe, &save, 0.0f, 0.0f, 0.0f, &input, dt);
    }
    assert(sh_save_item_count(&save, SH_ITEM_HANDGUN_BULLETS) == 12 && cafe.bird_hp == health);
    sh_save_item_add(&save, SH_ITEM_POCKET_KNIFE, 1);
    save.equipped_weapon = SH_ITEM_POCKET_KNIFE;
    cafe.bird_pos[2] = -1.0f;
    input.held = SH_BTN_AIM | SH_BTN_ACTION;
    input.down = SH_BTN_ACTION;
    for (int frame = 0; frame < (int)(1.0f / dt); frame++) {
        combat_update(&cafe, &save, 0.0f, 0.0f, 0.0f, &input, dt);
        input.down = 0;
    }
    assert(cafe.bird_hp == health - SH_KNIFE_DAMAGE);
    assert(sh_save_item_count(&save, SH_ITEM_HANDGUN_BULLETS) == 12);
}

int main(void)
{
    run(1.0f / 30.0f);
    run(1.0f / 60.0f);
    puts("PASS: cafe held handgun fire, release and single-press melee at 30/60 fps");
    return 0;
}
