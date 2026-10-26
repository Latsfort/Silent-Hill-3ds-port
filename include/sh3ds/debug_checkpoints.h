#pragma once

#include "sh3ds/savegame.h"

typedef struct ShDebugArrival {
    uint8_t map;
    float x;
    float z;
    float yaw;
} ShDebugArrival;

int sh_debug_checkpoint_count(void);
const char *sh_debug_checkpoint_name(int index);
bool sh_debug_checkpoint_apply(int index, ShSavegame *save, ShDebugArrival *arrival);
