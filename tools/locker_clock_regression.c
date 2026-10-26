#include "sh3ds/locker_world.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Reference {
    ShLockerWorld world;
    ShSavegame save;
    ShLockerWorld *actual;
    ShSavegame *actual_save;
    uint32_t seed;
    unsigned ticks, commands;
} Reference;

static void compare_tick(const ShLockerFrame *frame, void *context)
{
    Reference *reference = context;
    if (sh_save_event_get(reference->actual_save, 105)) { sh_save_event_set(&reference->save, 105); }
    const uint32_t actual_seed = sh_rng_get_seed();
    sh_rng_set_seed(reference->seed);
    ShLockerFrame expected;
    sh_locker_world_step(&reference->world, &reference->save, 136, false, &expected);
    reference->seed = sh_rng_get_seed();
    sh_rng_set_seed(actual_seed);
    assert(reference->seed == actual_seed);
    assert(reference->world.phase == reference->actual->phase && reference->world.timer == reference->actual->timer);
    assert(reference->world.speed == reference->actual->speed);
    assert(!memcmp(reference->world.rotation, reference->actual->rotation, sizeof(reference->world.rotation)));
    for (unsigned flag = 100; flag <= 108; flag++) {
        assert(sh_save_event_get(&reference->save, flag) == sh_save_event_get(reference->actual_save, flag));
    }
    assert(frame->draw_mask == expected.draw_mask && frame->command_count == expected.command_count);
    for (unsigned i = 0; i < frame->command_count; i++) {
        const ShLockerCommand *a = &frame->commands[i], *b = &expected.commands[i];
        assert(a->effect == b->effect && a->id == b->id && a->volume == b->volume && a->flags == b->flags);
        assert(!memcmp(&a->position, &b->position, sizeof(a->position)));
    }
    reference->ticks++;
    reference->commands += frame->command_count;
}

int main(void)
{
    for (int rate = 30; rate <= 60; rate *= 2) {
        ShLockerWorld world = {0};
        ShSavegame save = {0};
        ShLockerClock clock = {0};
        ShLockerFrame frame = {0};
        Reference reference = {.actual = &world, .actual_save = &save, .seed = 0x12345678};
        sh_save_event_set(&save, 101); sh_save_event_set(&save, 102);
        sh_save_event_set(&reference.save, 101); sh_save_event_set(&reference.save, 102);
        sh_rng_set_seed(reference.seed);
        unsigned ticks = 0;
        for (int render = 0; render < rate * 1000 / 30; render++) {
            if (ticks >= 500) { sh_save_event_set(&save, 105); }
            ticks += sh_locker_world_advance(&world, &save, &clock, 1.0f / rate, false, &frame, compare_tick, &reference);
            if (render % 137 == 0) {
                const ShLockerClock before = clock;
                const ShLockerWorld pose = world;
                const uint32_t seed = sh_rng_get_seed();
                assert(sh_locker_world_advance(&world, &save, &clock, 0, false, &frame, compare_tick, &reference) == 0);
                assert(clock.pending_frames == before.pending_frames && clock.pending_delta == before.pending_delta);
                assert(!memcmp(&world, &pose, sizeof(world)) && sh_rng_get_seed() == seed && !frame.command_count);
            }
        }
        assert(ticks == 1000 && reference.ticks == ticks && reference.commands > 0);
        assert(sh_save_event_get(&save, 103) && sh_save_event_get(&save, 106) && !sh_save_event_get(&save, 107));
        printf("PASS: %dfps matches1000 original locker ticks, flags, ordered commands, RNG and pauses\n", rate);
    }
    ShLockerWorld world = {0};
    ShSavegame save = {0};
    ShLockerClock clock = {0};
    ShLockerFrame frame = {0};
    Reference reference = {.actual = &world, .actual_save = &save, .seed = 0x12345678};
    sh_rng_set_seed(reference.seed);
    for (int render = 0; render < 100; render++) {
        assert(sh_locker_world_advance(&world, &save, &clock, 0.1f, false, &frame, compare_tick, &reference) == 3);
    }
    assert(reference.ticks == 300);
    puts("PASS: multi-tick frames preserve every command and original RNG order");
    return 0;
}
