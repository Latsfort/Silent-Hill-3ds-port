#pragma once

#include "sh3ds/alley.h"

void sh_alley_progress_update(ShAlley *alley, ShSavegame *save, float x, float z, float dt);
bool sh_alley_nightmare_trigger(const ShSavegame *save, float x, float z);
bool sh_alley_gate_trigger(float x, float z);
bool sh_alley_room_begin(ShAlley *alley, const ShSavegame *save, float x, float z, float yaw,
                        const ShInputState *input);
