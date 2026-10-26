#include "sh3ds/savegame.h"
#include "sh3ds/radio.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    ShSavegame save;
    ShRadio radio;
    sh_save_init(&save);
    save.health = SH_Q12(1.0f);
    sh_save_item_add(&save, SH_ITEM_FIRST_AID, 1);
    assert(sh_save_item_use_health(&save, SH_ITEM_FIRST_AID));
    assert(save.health == SH_Q12(81.0f));
    assert(sh_save_item_count(&save, SH_ITEM_FIRST_AID) == 0);
    assert(!sh_save_item_use_health(&save, SH_ITEM_AMPOULE));
    assert(save.health_saturation == 0);

    sh_save_item_add(&save, SH_ITEM_AMPOULE, 1);
    assert(sh_save_item_use_health(&save, SH_ITEM_AMPOULE));
    assert(save.health == SH_Q12(100.0f));
    assert(save.health_saturation == SH_Q12(300.0f));
    assert(sh_save_item_count(&save, SH_ITEM_AMPOULE) == 0);
    sh_save_tick(&save, 1.0f, 0.0f, false);
    assert(save.health_saturation == SH_Q12(299.0f));
    assert(save.health == SH_Q12(100.0f));
    save.health = SH_Q12(50.0f);
    sh_save_tick(&save, 0.5f, 0.0f, false);
    assert(save.health == SH_Q12(50.5f));
    save.health_saturation = SH_Q12(0.5f);
    sh_save_tick(&save, 0.5f, 0.0f, false);
    assert(save.health_saturation == 0);
    assert(save.health == SH_Q12(50.5f));
    sh_save_tick(&save, 1.0f, 0.0f, false);
    assert(save.health == SH_Q12(50.5f));

    sh_radio_init(&radio);
    sh_radio_update(&radio, true, true, 0.0f, 1.0f);
    assert(radio.powered && radio.intensity == 0.0f);
    sh_radio_update(&radio, true, true, 1.0f, 1.0f);
    assert(radio.intensity == 1.0f);
    sh_radio_update(&radio, true, false, 1.0f, 1.0f);
    assert(!radio.powered && radio.intensity == 0.0f);
    sh_radio_update(&radio, false, true, 1.0f, 1.0f);
    assert(!radio.powered && radio.intensity == 0.0f);
    puts("PASS: first aid, ampoule use/expiry/regeneration, radio silence/toggle/ownership");
    return 0;
}
