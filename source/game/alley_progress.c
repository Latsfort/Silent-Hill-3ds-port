#include "sh3ds/alley_progress.h"
#include "sh3ds/fixed.h"
#include "sh3ds/snd.h"
#include "sh3ds/controls.h"

#include <math.h>
#include <3ds.h>

#define ALLEY_ABS(x) ((x) < 0 ? -(x) : (x))
#define ALLEY_MAX(a, b) ((a) > (b) ? (a) : (b))
#define ALLEY_MIN(a, b) ((a) < (b) ? (a) : (b))
#define ALLEY_SQUARE(x) ((x) * (x))
#define ALLEY_Q8_ANGLE(x) ((x) << 4)
#define ALLEY_DIV(x, y) ((x) / (y))
#define ALLEY_FP_FROM(x, shift) ((x) >> (shift))

/* Math_AngleNormalizeSigned. */
static int16_t alley_signed_angle(int16_t angle)
{
    int16_t normalized = (uint16_t)angle & 0xfff;
    return normalized > 2048 ? (int16_t)(normalized | 0xf000) : normalized;
}

/* Math_Vector2MagCalcSafeQ6. */
static int32_t alley_magnitude(int32_t x, int32_t z)
{
    const int32_t sx = x >> 6;
    const int32_t sz = z >> 6;
    return (int32_t)sh_fixed_sqrt0((uint32_t)(sx * sx + sz * sz)) << 6;
}

#include "alley_progress_data.h"

/* Event_Update TriggerType_TouchAabb, MAP_EVENTS[12..13]. */
bool sh_alley_gate_trigger(float x, float z)
{
    for (unsigned i = 0; i < sizeof(s_alley_gate_events) / sizeof(s_alley_gate_events[0]); i++) {
        const ShMapPoint *point = &s_alley_points[s_alley_gate_events[i].point_idx];
        if (ALLEY_ABS(SH_Q12(x) - SH_Q12(point->x)) <= point->param0 * SH_Q12(0.25f) &&
            ALLEY_ABS(SH_Q12(-z) - SH_Q12(point->z)) <= point->param1 * SH_Q12(0.25f)) {
            return true;
        }
    }
    return false;
}

/* MAP_EVENTS[27], Event_CollideObbCheck. */
bool sh_alley_nightmare_trigger(const ShSavegame *save, float x, float z)
{
    return sh_save_event_get(save, 16) && !sh_save_event_get(save, 24) &&
        alley_obb(&s_alley_points[26], SH_Q12(x), SH_Q12(-z));
}

/* MAP_EVENTS LoadRoom, AreaLoad and Chara_PositionSet. */
bool sh_alley_room_begin(ShAlley *alley, const ShSavegame *save, float x, float z, float yaw,
                        const ShInputState *input)
{
    if (!sh_input_down(input, SH_BTN_ACTION)) { return false; }
    const int32_t ps1_yaw = SH_Q12(-yaw / 6.28318530717958647692f);
    for (unsigned i = 0; i < sizeof(s_alley_room_events) / sizeof(s_alley_room_events[0]); i++) {
        const ShMapEvent *event = &s_alley_room_events[i];
        if ((event->required_flag && !sh_save_event_get(save, event->required_flag)) ||
            (event->complete_flag && sh_save_event_get(save, event->complete_flag)) ||
            !alley_obb_facing(&s_alley_points[event->point_idx], SH_Q12(x), SH_Q12(-z), ps1_yaw)) {
            continue;
        }
        const ShMapPoint *target = &s_alley_points[event->event_param];
        const float heading = (float)target->param0 * (6.28318530717958647692f / 256.0f);
        alley->room_x = target->x + (target->param1 >= 2 ? sinf(heading) * 0.4f : 0.0f);
        alley->room_z = -target->z - (target->param1 >= 2 ? cosf(heading) * 0.4f : 0.0f);
        alley->room_yaw = -heading;
        alley->room_complete_flag = event->complete_flag;
        alley->stage = SH_ALLEY_ROOM;
        alley->controls_locked = true;
        alley->fade_target = 1.0f;
        alley->fade_speed = 3.0f;
        alley->cheryl_visible = false;
        return true;
    }
    return false;
}

/* Map_WorldObjectsUpdate and func_800DCC54. */
void sh_alley_progress_update(ShAlley *alley, ShSavegame *save, float x, float z, float dt)
{
    if (!sh_save_event_get(save, 13) || sh_save_event_get(save, 16)) {
        return;
    }
    if (alley->darkness < 0) {
        alley->darkness = 0;
        sh_snd_play(SH_SFX_NIGHTMARE_AMB, 0, 32);
    }
    const int32_t ps1_x = SH_Q12(x);
    const int32_t ps1_z = SH_Q12(-z);
    if (sh_save_event_get(save, 14)) {
        if (!sh_save_event_get(save, 18)) {
            sh_save_event_set(save, 18);
            sh_snd_play(SH_SFX_NIGHTMARE_AMB, 0, 64);
            alley->darkness = SH_Q12(60.0f);
        }
        alley->darkness = alley_darkness_advance(alley->darkness, alley->controls_locked,
            sh_save_event_get(save, 15), ps1_x, ps1_z, SH_Q12(dt));
    } else {
        alley->darkness = alley_path_darkness(ps1_x, ps1_z);
    }
    if (alley->darkness >= SH_Q12(80.0f)) { sh_save_event_set(save, 15); }
    if (alley->darkness >= SH_Q12(100.0f)) { sh_save_event_set(save, 16); }
}
