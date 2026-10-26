#include "sh3ds/bathroom.h"
#include "sh3ds/fixed.h"
#include <string.h>

/* events_util.c Event_TweenLinear: each invocation advances its timer. */
static int32_t bathroom_tween(ShBathroomMotion *motion, int32_t target, int32_t duration, int index, int32_t delta)
{
    motion->tween[index] += delta;
    if (motion->tween[index] > duration) { motion->tween[index] = duration; }
    return (target * motion->tween[index]) / duration;
}

#include "bathroom_motion_steps.h"
