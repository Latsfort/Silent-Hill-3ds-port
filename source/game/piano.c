#include "sh3ds/piano.h"
#include "sh3ds/fixed.h"
#include "sh3ds/piano_adapter_data.h"

#include <string.h>

typedef struct ShPianoLowerKey {
    int threshold, key;
} ShPianoLowerKey;

typedef struct ShPianoNote {
    int step;
    uint16_t sound;
    int32_t snap_x;
    bool reset;
} ShPianoNote;

#include "school_piano_data.h"

void sh_piano_init(ShPiano *piano)
{
    memset(piano, 0, sizeof(*piano));
}

/* joy.c: signed analog, original deadzone, then D-pad fallback. Circle Pad scale matches game.c. */
int sh_piano_axis(int circle, bool negative, bool positive)
{
    int raw = circle * 128 / 156;
    if (raw < -128) { raw = -128; }
    if (raw > 127) { raw = 127; }
    int normalized = 0;
    if (raw < -SH_PIANO_STICK_DEADZONE) { normalized = raw + SH_PIANO_STICK_DEADZONE; }
    else if (raw >= SH_PIANO_STICK_DEADZONE) { normalized = raw - (SH_PIANO_STICK_DEADZONE - 1); }
    if (!normalized && negative != positive) { normalized = negative ? -SH_PIANO_DIGITAL_STICK : SH_PIANO_DIGITAL_STICK; }
    return normalized;
}

/* game_main.c g_GravitySpeed, elapsed seconds converted to its H-blank counter. */
int32_t sh_piano_gravity(float dt)
{
    return ((int32_t)(dt * SH_PIANO_HBLANKS) * SH_PIANO_GRAVITY_SCALE) >> 12;
}

static void move_cursor(int32_t *position, float *fraction, int stick, float dt, int axis)
{
    const float motion = (stick * SH_Q12(4.0f) / 75) * (30.0f * dt) + *fraction;
    const int32_t delta = (int32_t)motion;
    *fraction = motion - delta;
    const int32_t next = *position + delta;
    *position = sh_q12_clamp(next, s_piano_limits[axis][0], s_piano_limits[axis][1]);
    if (*position != next) { *fraction = 0.0f; }
}

/* map1_s01.c PianoPuzzle_Control. Sticks use retail normalized range -75..75. */
ShPianoResult sh_piano_update(ShPiano *piano, int stick_x, int stick_y,
                             bool enter, bool cancel, float dt)
{
    ShPianoResult result = {0};
    if (piano->step & 1) {
        if (piano->step == 1 || piano->step == 3 || piano->step == 5) {
            piano->timer += SH_Q12(dt);
            if (s_piano_waits[piano->step] < piano->timer) {
                piano->step++;
                piano->timer = 0;
            }
        }
        return result;
    }
    if (piano->step == 4) {
        result.sound = SH_PIANO_MISS_SOUND;
        piano->step = 5;
        piano->timer = 0;
        return result;
    }
    piano->step = 0;
    if (piano->counter == (int)sizeof(s_piano_sequence)) {
        result.finished = true;
        result.solved = true;
        return result;
    }
    if (cancel) {
        result.finished = true;
        return result;
    }

    move_cursor(&piano->x, &piano->motion_fraction[0], stick_x, dt, 0);
    move_cursor(&piano->y, &piano->motion_fraction[1], stick_y, dt, 1);
    result.draw_cursor = true;
    result.cursor_x = (piano->x >> 12) + 8;
    result.cursor_y = (piano->y >> 12) + 8;
    if (!enter) { return result; }

    int key = (int)sizeof(s_piano_keys);
    const int x = piano->x >> 12;
    if ((piano->y >> 12) >= SH_PIANO_SPLIT_Y) {
        for (unsigned i = 0; i < sizeof(s_piano_lower) / sizeof(s_piano_lower[0]); i++) {
            if (x < s_piano_lower[i].threshold) {
                key = s_piano_lower[i].key;
                break;
            }
        }
    } else {
        for (unsigned i = 0; i < sizeof(s_piano_keys); i++) {
            if (x < s_piano_keys[i]) {
                key = (int)i;
                break;
            }
        }
    }
    const ShPianoNote *note = &s_piano_notes[key];
    piano->step = note->step;
    piano->timer = 0;
    result.sound = note->sound;
    piano->x = note->snap_x;
    piano->motion_fraction[0] = 0.0f;
    if (note->reset) {
        piano->counter = 0;
    } else if (key == s_piano_sequence[0]) {
        piano->counter = 1;
    } else if (key == s_piano_sequence[piano->counter]) {
        piano->counter++;
    } else {
        piano->counter = 0;
    }
    return result;
}
