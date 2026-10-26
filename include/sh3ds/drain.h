#pragma once
#include "sh3ds/path.h"
#include "sh3ds/chara.h"

bool sh_drain_camera_sample(int32_t ticks, bool ball, ShPathPoint *position, ShPathPoint *look);

typedef struct ShDrainLight {
    ShPathPoint position;
    struct { int16_t vx, vy, vz; } rotation;
} ShDrainLight;

void sh_drain_camera_light(ShPathPoint position, ShPathPoint look, ShDrainLight *light);
void sh_drain_light_direction(const ShDrainLight *light, int32_t direction[3]);

typedef struct ShDrainValve {
    uint16_t previous_angle;
    int16_t rotation, opening;
    uint8_t running, sound_phase;
} ShDrainValve;

enum {
    SH_DRAIN_VALVE_SQUEAK = 1,
    SH_DRAIN_VALVE_CLICK = 2,
    SH_DRAIN_WATER_START = 4,
    SH_DRAIN_WATER_STOP = 8
};

unsigned sh_drain_valve_hand(ShDrainValve *valve, int step, int hand_angle, int relative_frame);
unsigned sh_drain_valve_hand_bone(void);
void sh_drain_valve_hand_local(int16_t point[3]);
/* Original PS1 world-space bone matrix: Q12 rotation, Q8 translation. */
int sh_drain_valve_hand_angle(const ShCharaBone *world_bone);
void sh_drain_valve_world_bone(const ShCharaBone *model_bone, const int32_t position[3],
    int yaw, ShCharaBone *world_bone);
uint8_t sh_drain_water_attenuation(int32_t distance, int opening);
int32_t sh_drain_water_distance(ShPathPoint player, ShPathPoint camera);
void sh_drain_valve_transform_vertices(int16_t rotation, const float pivot[3],
    const ShMapVertex *source, ShMapVertex *destination, size_t count);
