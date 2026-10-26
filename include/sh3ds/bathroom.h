#pragma once
#include "sh3ds/path.h"
#include "sh3ds/vc.h"

typedef struct ShBathroomMotion {
    int32_t elapsed, tween[2];
    int16_t rotation;
} ShBathroomMotion;

typedef struct ShBathroomFrame {
    bool camera, finished;
    ShPathPoint position, look;
    ShVcScriptParams params;
} ShBathroomFrame;

void sh_bathroom_motion_step(ShBathroomMotion *motion, int32_t delta, ShBathroomFrame *frame);
