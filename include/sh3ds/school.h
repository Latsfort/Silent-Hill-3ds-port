#pragma once

#include "sh3ds/input.h"
#include "sh3ds/mapdata.h"
#include "sh3ds/savegame.h"
#include "sh3ds/vc.h"
#include "sh3ds/smoke.h"
#include "sh3ds/piano.h"
#include "sh3ds/silver.h"
#include "sh3ds/path.h"
#include "sh3ds/drain.h"
#include "sh3ds/roof_water.h"
#include "sh3ds/locker_world.h"
#include "sh3ds/bathroom.h"
#include "sh3ds/choice.h"
#include "sh3ds/boss_doors.h"
#include "sh3ds/phone.h"

struct ShTown;

bool sh_school_map_is_school(uint8_t map);
int sh_school_common_pickup_callback(uint8_t map);
int16_t sh_school_bathroom_rotation(const struct ShTown *town, const ShSavegame *save);
int sh_school_collision_flags(const struct ShTown *town, const ShSavegame *save, float x, float z);

typedef struct ShSchoolProp {
    const char *model;
    float pos[3]; /* PS1 metres */
    float rotation[3];
    uint32_t mask;
} ShSchoolProp;

typedef struct ShSchool {
    uint8_t overlay;
    int callback;
    int substep;
    int message;
    int completion_flag;
    uint8_t transition_flags;
    uint8_t activation_type;
    const char *image;
    const char *next_image;
    unsigned brightness;
    bool fade_started;
    ShMapChoice choice;
    bool choice_active;
    const char *choice_options[3];
    float source_pos[3];
    int32_t boiler_ticks;
    bool boiler_glow;
    bool player_warp;
    float player_warp_position[2], player_warp_yaw;
    ShPiano piano;
    ShPianoResult piano_result;
    ShSilverMotion silver;
    bool silver_active;
    ShPathPlayer card_player;
    bool empty_hand;
    ShDrainValve valve;
    bool valve_active;
    ShCharaBone hand_world;
    bool hand_world_valid;
    uint16_t drain_ticks;
    int16_t valve_animation_calls;
    bool player_hidden, drain_light_override, drain_flare_suppressed;
    ShDrainLight drain_light;
    ShPathPoint player_position;
    ShLockerWorld locker;
    ShBathroomMotion bathroom;
    ShBossDoors boss_doors;
    ShPhoneScene phone;
    unsigned arrival_audio_index;
    int arrival_camera_stage;
    uint8_t particle_mode;
    bool particle_reset;
    uint8_t boss_camera_started;
    int16_t descent_speed;
    int32_t descent_fade_speed, boss_box_y;
    ShLockerClock locker_clock;
    ShLockerFrame locker_frame;
    bool locker_render_active;
    uint8_t locker_camera_mode;
    bool locker_camera_active;
    ShVc locker_camera_vc;
    unsigned ladder_point;
    struct {
        struct { int32_t vx, vy, vz; } position;
        struct { int16_t vy; } rotation;
    } ladder_player;
    ShSmoke smoke;
} ShSchool;

const ShMapPoint *sh_school_points(int *count);
const ShMapEvent *sh_school_events(int *count);
const ShMapCommonPickup *sh_school_pickups(int *count);
const ShMapPoint *sh_school_map_points(uint8_t map, int *count);
const ShMapEvent *sh_school_map_events(uint8_t map, int *count);
const ShMapCommonPickup *sh_school_map_pickups(uint8_t map, int *count);
void sh_school_map_camera_init(ShVc *vc, uint8_t map);
uint8_t sh_school_map_room_idx(uint8_t map, float x, float z);
void sh_school_camera_init(ShVc *vc);
uint8_t sh_school_room_idx(float x, float z);
bool sh_school_event_enabled(const ShMapEvent *event, const ShSavegame *save);
bool sh_school_begin(struct ShTown *town, ShSavegame *save, const ShMapEvent *event);
void sh_school_update(struct ShTown *town, ShSavegame *save, const ShInputState *input, float dt);
void sh_school_piano_update(struct ShTown *town, ShSavegame *save, const ShInputState *input,
                             float dt, int stick_x, int stick_y, int32_t gravity);
bool sh_school_harry_keyframe(const struct ShTown *town, int *keyframe);
void sh_school_seed_harry_keyframe(struct ShTown *town, int keyframe);
void sh_school_seed_harry_hand(struct ShTown *town, const ShCharaBone *hand);
void sh_school_seed_harry_position(struct ShTown *town, float x, float y, float z);
const ShRoofWater *sh_school_roof_water(const struct ShTown *town);
void sh_school_roof_water_reset(void);
void sh_school_roof_world_reset(struct ShTown *town);
void sh_school_roof_water_update(struct ShTown *town, int yaw, const ShEnvState *environment,
    const ShEnvLight *light, float dt);
bool sh_school_take_player_warp(struct ShTown *town, float *x, float *z, float *yaw);
const char *sh_school_background(const struct ShTown *town);
const char *sh_school_background_next(const struct ShTown *town, float *alpha);
const ShSchoolProp *sh_school_props(int *count);
const ShSchoolProp *sh_school_map_props(uint8_t map, int *count);
uint32_t sh_school_prop_mask(const struct ShTown *town, const ShSavegame *save, float x, float z);
uint32_t sh_school_roof_moving_mask(const struct ShTown *town, const ShSavegame *save, float x, float z);
void sh_school_world_update(struct ShTown *town, ShSavegame *save, float x, float z, float dt);
void sh_school_phone_assets(bool phone_ready, bool previous_ready);
