#pragma once

#include <stdbool.h>
#include "sh3ds/chara.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reload actor mesh slot 0/1 (town: slot0 Air Screamer, slot1 Groaner). */
void sh_npc_load_slot(int slot, bool air_screamer);
bool sh_npc_load_split_head(int slot, int keyframe, int variant, int16_t twist);
bool sh_npc_pose_split_head(int slot, int keyframe, int variant, int16_t twist,
    const ShCharaBone **model_bones, unsigned *bone_count);
bool sh_npc_pose_split_head_blend(int slot, const ShAnimPoseTiming *pose, int variant, int16_t twist,
    const ShCharaBone **model_bones, unsigned *bone_count);

#ifdef __cplusplus
}
#endif
