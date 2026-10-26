#pragma once
#include "sh3ds/path.h"
#include "sh3ds/savegame.h"
#include "sh3ds/map.h"
#include "sh3ds/locker_world_data.h"

typedef struct ShLockerWorld {
    int16_t rotation[3];
    int32_t speed;
    int16_t timer;
    uint8_t phase;
} ShLockerWorld;

typedef struct ShLockerCommand {
    bool effect;
    int id, volume, flags;
    ShPathPoint position;
} ShLockerCommand;

typedef struct ShLockerFrame {
    uint32_t draw_mask;
    unsigned command_count;
    ShLockerCommand commands[SH_LOCKER_COMMAND_CAPACITY];
} ShLockerFrame;

typedef struct ShLockerClock {
    double pending_frames;
    int64_t pending_delta;
} ShLockerClock;

typedef void (*ShLockerEmit)(const ShLockerFrame *frame, void *context);

/* One original 30Hz world update; dt is the original Q12 delta. */
void sh_locker_world_step(ShLockerWorld *world, ShSavegame *save, int32_t dt,
    bool fade_finished, ShLockerFrame *frame);
unsigned sh_locker_world_advance(ShLockerWorld *world, ShSavegame *save, ShLockerClock *clock,
    float dt, bool fade_finished, ShLockerFrame *frame, ShLockerEmit emit, void *context);
void sh_locker_transform_vertices(int16_t rotation, const float pivot[3],
    const ShMapVertex *source, ShMapVertex *destination, size_t count);
void sh_world_vertical_vertices(int32_t offset_q8, const ShMapVertex *source, ShMapVertex *destination, size_t count);
