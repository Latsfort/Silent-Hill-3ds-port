#include "sh3ds/piano.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef int16_t s16;
typedef int32_t s32;
typedef int32_t q19_12;
#define Q12(v) ((int32_t)((v) * 4096.0f))
#define Q12_SHIFT 12
#define FP_FROM(v, shift) ((v) >> (shift))
#define ARRAY_SIZE(v) ((int)(sizeof(v) / sizeof((v)[0])))
#define CLAMP_RANGE(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
static struct { int sysStateSteps[2]; } g_SysWork;
static struct Controller {
    struct { int clicked; } buttonFlags;
    struct { struct { int leftX, leftY; } sticks_0; } normalizedSticks;
} controller, *g_Controller0 = &controller;
static struct GameWork {
    struct { struct { int cancel, enter; } controllerConfig; } config;
} game_work, *g_GameWorkPtr = &game_work;
static int g_PianoKeyCounter, g_PianoCursorY, g_PianoCursorX;
static bool D_800DD594;
static int ref_timer, ref_delta;
static ShPianoResult ref_result;
/* The generator checks these arrays against the verified overlay separately. */
typedef struct ShPianoLowerKey { int threshold, key; } ShPianoLowerKey;
typedef struct ShPianoNote { int step; uint16_t sound; int32_t snap_x; bool reset; } ShPianoNote;
#include "../source/game/school_piano_data.h"
#define g_PianoKeys s_piano_keys
#define g_PianoKeySequence s_piano_sequence
static void SysWork_StateStepSet(int index, int step)
{
    g_SysWork.sysStateSteps[index] = step;
    ref_timer = 0;
}
static void SysWork_StateStepIncrement(int index)
{
    SysWork_StateStepSet(index, g_SysWork.sysStateSteps[index] + 1);
    if (!index) { ref_result.finished = true; }
    if (!index) { g_SysWork.sysStateSteps[1] = 0; }
}
static void Event_WaitTimer(int delay, bool substep)
{
    assert(substep);
    ref_timer += ref_delta;
    if (delay < ref_timer) { SysWork_StateStepIncrement(1); }
}
static void SD_Call(int sound) { ref_result.sound = sound; }
static void Savegame_EventFlagSet(int flag) { assert(flag == 75); ref_result.solved = true; }
static void Game_TimerUpdate(void) {}
#define Gfx_CursorDraw(x, y, ...) do { ref_result.draw_cursor = true; ref_result.cursor_x = (x); ref_result.cursor_y = (y); } while (0)
#include "school_piano_reference.h"

static void reset_reference(const ShPiano *piano)
{
    reference_control(false);
    g_PianoCursorX = piano->x;
    g_PianoCursorY = piano->y;
    g_PianoKeyCounter = piano->counter;
    g_SysWork.sysStateSteps[0] = 0;
    g_SysWork.sysStateSteps[1] = piano->step;
    ref_timer = piano->timer;
    memset(&ref_result, 0, sizeof(ref_result));
    game_work.config.controllerConfig.enter = 1;
    game_work.config.controllerConfig.cancel = 2;
}

static ShPianoResult compare(ShPiano *piano, int x, int y, bool enter, bool cancel)
{
    reset_reference(piano);
    controller.normalizedSticks.sticks_0.leftX = x;
    controller.normalizedSticks.sticks_0.leftY = y;
    controller.buttonFlags.clicked = (enter ? 1 : 0) | (cancel ? 2 : 0);
    ref_delta = Q12(1.0f / 30.0f);
    reference_control(true);
    ShPianoResult result = sh_piano_update(piano, x, y, enter, cancel, 1.0f / 30.0f);
    assert(piano->x == g_PianoCursorX && piano->y == g_PianoCursorY);
    assert(piano->counter == g_PianoKeyCounter && piano->step == g_SysWork.sysStateSteps[1]);
    assert(piano->timer == ref_timer);
    assert(result.finished == ref_result.finished && result.solved == ref_result.solved);
    assert(result.sound == ref_result.sound && result.draw_cursor == ref_result.draw_cursor);
    assert(result.cursor_x == ref_result.cursor_x && result.cursor_y == ref_result.cursor_y);
    return result;
}

int main(void)
{
    assert(sh_piano_axis(0, false, false) == 0);
    assert(sh_piano_axis(0, true, false) == -45 && sh_piano_axis(0, false, true) == 45);
    assert(sh_piano_axis(0, true, true) == 0);
    assert(sh_piano_axis(156, true, false) == 64 && sh_piano_axis(-156, false, true) == -64);
    assert(sh_piano_axis(78, true, false) == 1 && sh_piano_axis(-78, false, true) == 45);
    assert(sh_piano_gravity(1.0f / 30.0f) == ((526 * 10419) >> 12));
    assert(sh_piano_gravity(1.0f / 60.0f) == ((263 * 10419) >> 12));
    unsigned cases = 0;
    ShPiano piano;
    /* Every integer hitbox position, both sides of the black/white split. */
    for (int y = -71; y <= 84; y++) {
        for (int x = -89; x <= 85; x++) {
            for (int counter = 0; counter < 5; counter++) {
                sh_piano_init(&piano);
                piano.x = Q12(x);
                piano.y = Q12(y);
                piano.counter = counter;
                compare(&piano, 0, 0, true, false);
                cases++;
            }
        }
    }
    /* All normalized stick values, negative fractional positions and clamps. */
    for (int stick = -75; stick <= 75; stick++) {
        for (int pos = -91; pos <= 87; pos++) {
            sh_piano_init(&piano);
            piano.x = Q12(pos) + 2047;
            piano.y = Q12(pos) - 2047;
            compare(&piano, stick, -stick, false, false);
            cases++;
        }
    }
    for (int step = 0; step <= 6; step++) {
        for (int counter = 0; counter <= 5; counter++) {
            sh_piano_init(&piano);
            piano.step = step;
            piano.counter = counter;
            for (int frame = 0; frame < 45; frame++) {
                compare(&piano, 0, 0, frame % 13 == 0, frame % 17 == 0);
                cases++;
            }
        }
    }
    /* Full solution, including the extra sound and waits of each silent key. */
    sh_piano_init(&piano);
    ShPianoResult result = {0};
    for (unsigned i = 0; i < sizeof(s_piano_sequence); i++) {
        piano.x = s_piano_notes[s_piano_sequence[i]].snap_x;
        result = compare(&piano, 0, 0, true, false);
        assert(result.sound == 1445 && piano.counter == (int)i + 1);
        while (piano.step) {
            result = compare(&piano, 0, 0, false, false);
        }
    }
    assert(result.finished && result.solved);
    /* Presentation-rate scaling at full deflection. */
    for (int stick = -75; stick <= 75; stick++) {
        ShPiano p30, p60;
        sh_piano_init(&p30);
        sh_piano_init(&p60);
        for (int i = 0; i < 120; i++) {
            sh_piano_update(&p30, stick, -stick, false, false, 1.0f / 30.0f);
            sh_piano_update(&p60, stick, -stick, false, false, 1.0f / 60.0f);
            sh_piano_update(&p60, stick, -stick, false, false, 1.0f / 60.0f);
            assert(p30.x == p60.x && p30.y == p60.y);
        }
    }
    printf("PASS: %u retail piano comparisons, complete solution and 30/60 Hz cursor movement\n", cases);
    return 0;
}
