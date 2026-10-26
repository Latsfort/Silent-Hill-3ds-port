#include "sh3ds/radio.h"

void sh_radio_init(ShRadio *radio)
{
    if (!radio) {
        return;
    }
    radio->intensity = 0.0f;
    radio->powered = false;
    radio->noise_on = false;
}

void sh_radio_update(ShRadio *radio, bool has_radio, bool radio_on, float danger_nearness, float dt)
{
    (void)dt;
    if (!radio) {
        return;
    }
    radio->powered = has_radio && radio_on;
    if (!radio->powered) {
        radio->intensity = 0.0f;
        return;
    }
    if (danger_nearness < 0.0f) {
        danger_nearness = 0.0f;
    }
    if (danger_nearness > 1.0f) {
        danger_nearness = 1.0f;
    }
    /* Game_NpcUpdate: no interference without a detectable enemy. */
    radio->intensity = danger_nearness;
}
