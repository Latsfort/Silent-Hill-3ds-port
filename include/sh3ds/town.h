#pragma once
#include "sh3ds/border.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h> /* uint8_t overlay_map_idx */

#include "sh3ds/coll.h"
#include "sh3ds/environment.h"
#include "sh3ds/gfx.h"
#include "sh3ds/input.h"
#include "sh3ds/savegame.h"
#include "sh3ds/school.h"
#include "sh3ds/vc.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * map2_s00 Old Silent Hill street after leaving the cafe (MAP0_S01 door → MAP2_S00).
 * Ports MapEvent_CutsceneExitCafe + Levin St VC roads / free roam.
 * Groaner combat stubs stay dormant — retail map2_s00 chara_spawns is empty at cafe exit.
 *
 * Coordinates are 3DS metres: X = PS1 X, Y = -PS1 Y, Z = -PS1 Z.
 */

/* map0_s01 MAP_POINTS[7] — street spawn outside the cafe. */
#define SH_TOWN_SPAWN_X 22.2f
#define SH_TOWN_SPAWN_Z (-56.5f)
/* triggerParam0 = 192 (Q8) → 270° PS1 → 3DS yaw ≈ +π/2. */
#define SH_TOWN_SPAWN_YAW 1.5707963f

#define SH_EV_EXIT_CAFE 146 /* EventFlag_146 — exit cutscene done, enemies allowed */
#define SH_MAP_IDX_MAP0_S01 1
#define SH_MAP_IDX_MAP0_S02 2
#define SH_MAP_IDX_MAP1_S00 3
#define SH_MAP_IDX_MAP1_S01 4
#define SH_MAP_IDX_MAP1_S02 5
#define SH_MAP_IDX_MAP1_S03 6
#define SH_MAP_IDX_MAP2_S00 10
#define SH_LOC_CHURCH 7 /* SaveLocationId used for this outdoor bucket */

#define SH_TOWN_MAX_DOGS 2 /* matches SH_GFX_MAX_ACTORS */

typedef enum ShTownStage {
    SH_TOWN_OFF = 0,
    SH_TOWN_EXIT_CS, /* MapEvent_CutsceneExitCafe */
    SH_TOWN_EXPLORE, /* free roam + combat + dogs */
    SH_TOWN_SCRIPT,  /* MapEvent_* cutscenes (doghouse, sketchbook, …) */
    SH_TOWN_LEAVING, /* SysState_LoadOverlay / SysState_LoadRoom fade */
    SH_TOWN_DONE
} ShTownStage;

/* Internal script ids (map2_s00 MapEvent_*). */
typedef enum ShTownScript {
    SH_TOWN_SCR_NONE = 0,
    SH_TOWN_SCR_DOG_NOTE,
    SH_TOWN_SCR_DOG_KEY,
    SH_TOWN_SCR_STEEL_PIPE,
    SH_TOWN_SCR_SKETCHBOOK,
    SH_TOWN_SCR_PAPER_MAP,
    SH_TOWN_SCR_WATER_WORKS,
    SH_TOWN_SCR_READ_MSG,
    SH_TOWN_SCR_SKETCH_COVER, /* func_800E9470 */
    SH_TOWN_SCR_WOODMAN_KEY, /* func_800E9A74 */
    SH_TOWN_SCR_ECLIPSE_LOCKS, /* func_800E9DD8 */
    SH_TOWN_SCR_ECLIPSE_ENTER, /* MapEvent_DoorOfEclipseEnter */
    SH_TOWN_SCR_SCHOOL
} ShTownScript;

typedef struct ShTownDog {
    bool alive;
    float pos[3];
    float yaw;
    float hp;
    float anim;
    float cd; /* attack cooldown */
    int state; /* 0 idle, 1 approach, 2 lunge, 3 hurt, 4 dead */
    int spawn; /* charaSpawnInfos slot (0..31), -1 = free */
    int gfx_slot; /* SH_GFX actor mesh 0 or 1 */
    bool air;     /* group 0 = Air Screamer (map2_s00 charaGroupIds[0]) */
} ShTownDog;

typedef struct ShTown {
    ShTownStage stage;
    int step;
    float timer;
    float wait;
    float fade;
    float fade_target;
    float fade_speed;

    bool cam_active;
    ShCamera cam;
    ShVc vc; /* map2_s00 vc_road_data gameplay camera */
    bool map_open; /* paper map overlay */
    /* ExitCafe paper map: D_800F228C, g_Gfx_PaperMapMarkingAlpha, expanding boxes. */
    float map_d;
    float map_scale;
    float map_scroll_x;
    float map_scroll_y;
    float mark_alpha; /* 0..128 */
    int box_mode;     /* 0 none, 1 shrink (case 7), 2 grow (case 8) */
    bool skip_fading;

    bool aiming;
    float attack_cd;
    float fire_anim;
    float gun_flash;
    float hit_flash;
    int ammo;

    const char *subtitle;
    const char *dialogue;
    float msg_time;
    bool letterbox;
    bool controls_locked;

    /* Event_ItemTake (common street pickups via func_800E7B74). */
    bool pickup_active;
    bool prompt_open;
    float pickup_scale;
    float pickup_spin;
    int pickup_item_id;
    int pickup_count;
    int pickup_flag;
    const char *pickup_msg;

    ShTownScript script;
    int script_done_flag; /* EventFlag set on completion */
    int read_msg_idx;     /* MAP_MESSAGES index for SH_TOWN_SCR_READ_MSG */
    bool script_use_cam;  /* fixed camera during script (water works) */
    int harry_state;
    float harry_time;
    int script_substep;
    unsigned bg_image;
    unsigned bg_next_image;
    float bg_transition;
    int32_t script_ticks;
    int32_t script_wait_ticks;
    ShEnvironment environment;
    ShSchool school;
    ShBorder border;
    int32_t school_boiler_volume;

    ShTownDog dogs[SH_TOWN_MAX_DOGS];
    int dog_count;
    uint32_t spawn_used;   /* g_SysWork.field_228C: slot already spawned */
    uint32_t spawn_killed; /* cleared bits of mapEnemyStates */
    bool spawn_in_game;    /* Game_NpcRoomInitSpawn cond: false on room init */

    bool leave_overlay; /* fade done → game.c runs overlay transition */
    uint8_t overlay_map_idx; /* LoadOverlay: e_MapIdx; LoadRoom: BGM song, not a map */
    uint8_t overlay_spawn_pt;
    uint8_t overlay_sys_state; /* SH_SYS_LOAD_OVERLAY or SH_SYS_LOAD_ROOM */
    int overlay_complete_flag;
    uint8_t overlay_transition_flags;
    int8_t overlay_room_idx;   /* SysState_LoadArea: dest map point field_4_5 - 1 */
    float overlay_x; /* 3DS after Chara_PositionSet / school gate nudge */
    float overlay_z;
    float overlay_yaw;

    bool need_ground_snap;
    bool bgm_ready;
} ShTown;

void sh_town_reset(ShTown *town);
void sh_town_overlay_camera_init(ShTown *town, uint8_t map_idx);
int sh_town_item_triggers(const ShTown *town, const ShSavegame *save, float x, float z, float yaw,
                          uint8_t *items, int max);
bool sh_town_item_use(ShTown *town, ShSavegame *save, float x, float z, float yaw, uint8_t item);
/* Call after warping Harry to SH_TOWN_SPAWN_*; starts ExitCafe cutscene. */
void sh_town_begin(ShTown *town, ShSavegame *save);
/* Dev skip / post-portal: free roam at gate spawn (no ExitCafe CS). */
void sh_town_begin_explore_at(ShTown *town, ShSavegame *save, float x, float z, float yaw);
/* Chara_PositionSet on map2_s00 MAP_POINTS[idx], 3DS space. */
void sh_town_map_point_spawn(int idx, float *x, float *z, float *yaw);

bool sh_town_active(const ShTown *town);

bool sh_town_update(ShTown *town, ShSavegame *save, float *player_x, float *player_y, float *player_z,
                    float *player_yaw, const ShInputState *input, float dt);

const char *sh_town_subtitle(const ShTown *town);
const char *sh_town_dialogue(const ShTown *town);
const char *sh_town_hint(const ShTown *town);
bool sh_town_letterbox(const ShTown *town);
float sh_town_fade(const ShTown *town);
bool sh_town_camera(const ShTown *town, ShCamera *out);
bool sh_town_map_open(const ShTown *town);
/* Fills the paper-map overlay fields of `hud` (scale/scroll, markings, boxes). */
void sh_town_map_hud(const ShTown *town, const ShSavegame *save, float player_x, float player_z, float player_yaw,
                     ShMapHud *hud, ShMapSprite *sprites, int max);
unsigned sh_town_world_key(const ShTown *town, const ShSavegame *save);
bool sh_town_aiming(const ShTown *town);
float sh_town_gun_flash(const ShTown *town);
/* Gfx_PickupItemAnimate close-up while Event_ItemTake is active. */
bool sh_town_pickup_item(const ShTown *town, int *item_id, float *scale, float *spin);

/* Groaner poses use actor slots 0..dog_count-1 (cafe must be OFF). */
bool sh_town_actor_pose(const ShTown *town, int slot, ShActorPose *out);
int sh_town_actor_keyframe(const ShTown *town, int slot, int kf_count);

void sh_town_bgm_update(const ShTown *town, const ShSavegame *save, bool player_dead, float dt);

bool sh_town_take_ground_snap(ShTown *town);
void sh_town_snap_enemy_heights(ShTown *town, const ShCollWorld *world);

/* SysState_LoadOverlay / SysState_LoadRoom. */
bool sh_town_debug_fire_event(ShTown *town, int idx); /* autotest: force a LoadRoom/LoadOverlay */
bool sh_town_leave_overlay(const ShTown *town);
uint8_t sh_town_overlay_map_idx(const ShTown *town);
uint8_t sh_town_overlay_spawn_pt(const ShTown *town);
void sh_town_overlay_target(const ShTown *town, float *x, float *z);
float sh_town_overlay_yaw(const ShTown *town);
bool sh_town_overlay_is_room(const ShTown *town);
bool sh_town_overlay_skip_fade_in(const ShTown *town);
/* Event_BgTextureCmd(Draw) active: SKETCHBK.TIM fullscreen. */
const char *sh_town_bg_tim(const ShTown *town); /* event background TIM name, or NULL */
const char *sh_town_bg_next_tim(const ShTown *town, float *alpha);
bool sh_town_harry_keyframe(const ShTown *town, int *keyframe);

#ifdef __cplusplus
}
#endif
