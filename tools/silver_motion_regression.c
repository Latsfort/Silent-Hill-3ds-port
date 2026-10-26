#include "sh3ds/silver.h"
#include "sh3ds/fixed.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define Q12(v) ((int32_t)((v) * 4096.0f))
#define Q12_ANGLE(v) ((int16_t)Q12((v) / 360.0f))
#define Q12_SHIFT 12
#define FP_FROM(v, shift) ((v) >> (shift))
#define Q12_MULT_PRECISE(a, b) ((int32_t)(((int64_t)(a) * (b)) >> 12))
#define Math_Cos sh_fixed_cos
#define Math_Sin sh_fixed_sin
static struct {
    struct { int32_t vx, vy, vz; } position;
    struct { int16_t vx, vy; } rotation;
} g_WorldObject_SilverH;
static int32_t D_800DD4FC, g_DeltaTime, g_GravitySpeed;
static bool ref_hit;
static void Event_WaitTimer(int32_t delay, bool substep) { assert(delay == Q12(0.5f) && !substep); }
static void SD_Call(int sound) { assert(sound == 1417); }
static void SysWork_StateStepIncrement(int step) { assert(step == 0); ref_hit = true; }
#include "silver_motion_reference.h"

static void copy_reference(const ShSilverMotion *silver)
{
    g_WorldObject_SilverH.position.vx = silver->position[0];
    g_WorldObject_SilverH.position.vy = silver->position[1];
    g_WorldObject_SilverH.position.vz = silver->position[2];
    g_WorldObject_SilverH.rotation.vx = silver->rotation[0];
    g_WorldObject_SilverH.rotation.vy = silver->rotation[1];
    D_800DD4FC = silver->velocity;
    ref_hit = false;
}

static void check(const ShSilverMotion *silver)
{
    assert(silver->position[0] == g_WorldObject_SilverH.position.vx);
    assert(silver->position[1] == g_WorldObject_SilverH.position.vy);
    assert(silver->position[2] == g_WorldObject_SilverH.position.vz);
    assert(silver->rotation[0] == g_WorldObject_SilverH.rotation.vx);
    assert(silver->rotation[1] == g_WorldObject_SilverH.rotation.vy);
    assert(silver->velocity == D_800DD4FC);
}

int main(void)
{
    unsigned comparisons = 0;
    for (int delta = 1; delta <= 1024; delta++) {
        ShSilverMotion silver;
        sh_silver_init(&silver);
        assert(!silver.velocity && silver.rotation[0] == 0 && silver.rotation[1] == 0);
        copy_reference(&silver);
        g_DeltaTime = delta;
        for (int frame = 0; frame < 45; frame++) {
            reference_swing();
            sh_silver_swing(&silver, delta);
            check(&silver);
            comparisons++;
        }
        /* Supply the same engine gravity to both implementations. */
        g_GravitySpeed = delta * 10;
        bool hit = false;
        for (int frame = 0; frame < 20000 && !hit; frame++) {
            reference_fall();
            hit = sh_silver_fall(&silver, delta, g_GravitySpeed);
            check(&silver);
            assert(hit == ref_hit);
            comparisons++;
        }
        assert(hit);
        const int32_t velocity = silver.velocity;
        sh_silver_ground(&silver);
        assert(silver.velocity == velocity);
        assert(silver.position[0] == Q12(-98.8f) && silver.position[1] == 0 && silver.position[2] == Q12(22.8f));
        assert(silver.rotation[0] == Q12_ANGLE(-90.0f) && silver.rotation[1] == Q12_ANGLE(28.5f));
        float position[3], rotation[3];
        sh_silver_pose(&silver, position, rotation);
        for (int axis = 0; axis < 3; axis++) {
            assert(position[axis] * 4096.0f == silver.position[axis]);
        }
        assert(rotation[0] == -90.0f && rotation[2] == 0.0f);
    }
    puts("PASS: original silver swing/fall arithmetic, floor trigger, ground warp and retained velocity");
    printf("%u per-frame comparisons with original C across 1024 timestep values\n", comparisons);
    return 0;
}
