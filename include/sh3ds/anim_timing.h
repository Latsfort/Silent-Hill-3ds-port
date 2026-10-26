#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct ShAnimTiming {
    int status, keyframe;
    int32_t time, alpha;
} ShAnimTiming;

typedef struct ShAnimClipTiming { int first, last, link; } ShAnimClipTiming;
typedef struct ShAnimPoseTiming { int first, second; int32_t alpha; } ShAnimPoseTiming;

void sh_anim_loop_timing(ShAnimTiming *model, const ShAnimClipTiming *clip,
                         int32_t timestep, ShAnimPoseTiming *pose);
void sh_anim_blend_timing(ShAnimTiming *model, const ShAnimClipTiming *clip,
                          int32_t timestep, ShAnimPoseTiming *pose);
void sh_anim_once_timing(ShAnimTiming *model, const ShAnimClipTiming *clip,
                         int32_t timestep, ShAnimPoseTiming *pose);
