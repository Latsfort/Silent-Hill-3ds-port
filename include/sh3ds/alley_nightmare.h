#pragma once

#include "sh3ds/path.h"
#include "sh3ds/phone.h"
#include "sh3ds/chara.h"

typedef struct ShNightmare {
    ShPath path;
    ShPathPlayer player;
    ShPathPoint waypoints[2];
    ShCharaBone head;
    bool head_valid, hidden, initialized;
    int camera_phase, border_step;
    ShPhoneText text;
    double text_frames;
    int64_t text_delta;
    bool text_skip;
    unsigned voice_index;
} ShNightmare;

int sh_alley_nightmare_head_bone(void);
