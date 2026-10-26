#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShRadio {
    float intensity; /* 0..1 static level */
    bool powered;    /* inventory has radio AND toggle On */
    bool noise_on;   /* town interference loop */
} ShRadio;

void sh_radio_init(ShRadio *radio);
/* `radio_on` = ItemToggleFlag_RadioOn (inventory On/Off). */
void sh_radio_update(ShRadio *radio, bool has_radio, bool radio_on, float danger_nearness, float dt);

#ifdef __cplusplus
}
#endif
