#include "../source/game/town.c"
#include "sd_port.h"

#include <assert.h>
#include <stdio.h>

bool sh_input_down(const ShInputState *input, uint32_t key)
{
    return input && (input->down & key) != 0;
}

bool sh_input_held(const ShInputState *input, uint32_t key)
{
    return input && (input->held & key) != 0;
}

void sh_npc_load_slot(int slot, bool air_screamer)
{
    (void)air_screamer;
    assert(slot >= 0 && slot < SH_GFX_MAX_ACTORS);
}

static void setup(ShTown *town, ShSavegame *save, bool air)
{
    memset(town, 0, sizeof(*town));
    sh_save_init(save);
    sh_save_item_add(save, SH_ITEM_HANDGUN, 1);
    sh_save_item_add(save, SH_ITEM_HANDGUN_BULLETS, 15);
    save->equipped_weapon = SH_ITEM_HANDGUN;
    town->dog_count = 2;
    for (int i = 0; i < 2; i++) {
        town->dogs[i].alive = true;
        town->dogs[i].hp = SH_DOG_HP;
        town->dogs[i].air = air;
        town->dogs[i].spawn = (air ? 0 : 16) + i;
    }
    /* Front target is 30 degrees off axis; second enemy is behind Harry. */
    town->dogs[0].pos[0] = 2.5f;
    town->dogs[0].pos[2] = -4.330127f;
    town->dogs[1].pos[2] = 3.0f;
}

int main(void)
{
    sd_port_set_roots("romfs/game/snd", "romfs/game/xa");
    assert(sh_sound_init());
    ShTown town;
    ShSavegame save;
    ShInputState input = {0};
    input.held = SH_BTN_AIM | SH_BTN_ACTION;
    for (int air = 0; air <= 1; air++) {
        setup(&town, &save, air != 0);
        for (int frame = 0; frame < 60; frame++) {
            input.down = frame == 0 ? input.held : 0;
            combat_update(&town, &save, 0.0f, 0.0f, 0.0f, &input, 1.0f / 30.0f);
        }
        assert(!town.dogs[0].alive && town.dogs[0].hp <= 0.0f);
        assert(town.dogs[1].hp == SH_DOG_HP);
        assert(town.spawn_killed & (1u << town.dogs[0].spawn));
        assert(sh_save_item_count(&save, SH_ITEM_HANDGUN_BULLETS) < 14);
        printf("PASS: held fire kills %s at 30 degrees; rear enemy is untouched\n", air ? "Air Screamer" : "Groaner");
    }
    setup(&town, &save, false);
    town.dogs[1].pos[0] = 0.5f;
    town.dogs[1].pos[2] = -5.0f;
    combat_update(&town, &save, 0.0f, 0.0f, 0.0f, &input, 1.0f / 30.0f);
    assert(town.dogs[0].hp == SH_DOG_HP && town.dogs[1].hp < SH_DOG_HP);
    assert(sh_save_item_count(&save, SH_ITEM_HANDGUN_BULLETS) == 14);
    puts("PASS: one bullet damages one target, selected by facing angle");

    bool checked_pair = false;
    for (int i = 0; i < 16 && !checked_pair; i++) {
        const ShMapEnemySpawn *spawn = &s_map_enemy_spawns_map2_s00[0][0][i];
        memset(&town, 0, sizeof(town));
        town_spawn_reset(&town);
        sh_save_init(&save);
        sh_save_event_set(&save, SH_EV_EXIT_CAFE);
        town_spawn_npcs(&town, &save, spawn->x, -spawn->z, false);
        if (town.dogs[0].alive && town.dogs[1].alive) {
            assert(town.dogs[0].gfx_slot != town.dogs[1].gfx_slot);
            checked_pair = true;
        }
    }
    assert(checked_pair);
    puts("PASS: simultaneously spawned retail enemies have separate graphics slots");
    sh_sound_shutdown();
    return 0;
}
