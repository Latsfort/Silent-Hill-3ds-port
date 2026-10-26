#include "sh3ds/phone.h"
#include <string.h>
#include <limits.h>
#include "sh3ds/rng.h"
#include "phone_data.h"

#include "phone_text_steps.h"
#include "phone_audio_steps.h"

/* Event_TweenLinear advances each named timer once per invocation. */
static int32_t phone_tween(ShPhoneMotion *motion, int index, int32_t target, int32_t duration, int32_t delta)
{
    motion->tween[index] += delta;
    if (motion->tween[index] > duration) { motion->tween[index] = duration; }
    return (target * motion->tween[index]) / duration;
}

#include "phone_motion_steps.h"

const ShPhonePage *sh_phone_pages(unsigned *count)
{
    *count = sizeof(s_phone_pages) / sizeof(s_phone_pages[0]);
    return s_phone_pages;
}

/* sharedFunc_800CDAA8_0_s02 player states97/98, before animation playback. */
bool sh_phone_animation_events(ShSavegame *save, int state, int keyframe, bool *held_hidden)
{
    if (state == SH_PHONE_PICKUP_STATE) {
        if (keyframe >= SH_PHONE_PICKUP_FLAG_FRAME) { sh_save_event_set(save, SH_PHONE_PICKUP_STATE_FLAG); }
        *held_hidden = keyframe < SH_PHONE_HELD_VISIBLE_FRAME;
        return true;
    }
    if (state == SH_PHONE_RETURN_STATE && keyframe >= SH_PHONE_RETURN_FLAG_FRAME) {
        sh_save_event_clear(save, SH_PHONE_PICKUP_STATE_FLAG);
    }
    return false;
}
