#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sh3ds/dms.h"
#include "sh3ds/gfx.h"
#include "sh3ds/input.h"
#include "sh3ds/savegame.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * map0_s01 Cafe intro, ported from decomp map0_s01_events.c:
 *   MapEvent_CafeCutscene (CAFE.DMS, Cybil), item pickups, MapEvent_MapItemTake
 *   (bird perched outside, CAFE2.DMS frame 0), MapEvent_AirScreamerIntroCutscene
 *   (CAFE2.DMS 26..154), Air Screamer fight, MapEvent_AirScreamerDeath, radio.
 * Coordinates handed to the game are 3DS meters: X = PS1 X, Y = -PS1 Y, Z = -PS1 Z.
 */

/* Authentic event flags from event_flags.h */
#define SH_EV_KNIFE 35
#define SH_EV_FLASHLIGHT 36
#define SH_EV_RADIO 37
#define SH_EV_MAP 38
#define SH_EV_FMV 39
#define SH_EV_CAFE_CS 40
#define SH_EV_BIRD_PERCH_FLY 41
#define SH_EV_BIRD_PERCH_DONE 42
#define SH_EV_KNIFE_AND_MAP 43
#define SH_EV_BIRD_INTRO 45
#define SH_EV_46 46
#define SH_EV_47 47
#define SH_EV_BIRD_DIED 48
#define SH_EV_CAFE_DONE 49
#define SH_EV_RADIO_ON_50 50
#define SH_EV_BIRD_DEATH_PLAYED 51
#define SH_EV_RADIO_STATIC 52
#define SH_EV_HEALTH_DRINK0 596 /* EventFlag_M0S01_HealthDrink0 */
#define SH_EV_HEALTH_DRINK1 597

typedef enum ShCafeStage {
    SH_CAFE_OFF = 0,
    SH_CAFE_CUTSCENE,   /* MapEvent_CafeCutscene */
    SH_CAFE_EXPLORE,    /* pickups, map peek, door */
    SH_CAFE_BIRD_INTRO, /* MapEvent_AirScreamerIntroCutscene */
    SH_CAFE_BIRD_FIGHT, /* Air Screamer AI + combat */
    SH_CAFE_BIRD_DEATH, /* MapEvent_AirScreamerDeath */
    SH_CAFE_DONE,       /* radio taken; free roam until door exit */
    SH_CAFE_LEAVING     /* fade out → load town street (MAP2_S00 door) */
} ShCafeStage;

typedef enum ShCafeActor { SH_CAFE_ACTOR_CYBIL = 0, SH_CAFE_ACTOR_BIRD = 1 } ShCafeActor;

typedef enum ShBirdState {
    SH_BIRD_HIDDEN = 0,
    SH_BIRD_PERCH,   /* on the sign outside (CAFE2 frame 0) */
    SH_BIRD_SCRIPTED, /* DMS driven */
    SH_BIRD_HOVER,
    SH_BIRD_APPROACH,
    SH_BIRD_DIVE,
    SH_BIRD_RETREAT,
    SH_BIRD_RECOIL,
    SH_BIRD_DYING,
    SH_BIRD_DEAD
} ShBirdState;

#define SH_CAFE_DMS_MAX 5120

typedef struct ShCafe {
    ShCafeStage stage;
    int step;      /* decomp g_SysWork.sysStateSteps[0] */
    float timer;   /* g_Cutscene_Timer in DMS frames, < 0 = none */
    float wait;    /* Event_WaitTimer */
    int sub0;      /* D_800DE250: Cybil visible */
    int sub1;      /* D_800DE251 */
    float sub_time;

    /* Map message player (Gfx_MapMsg_Draw) */
    int msg_idx;      /* MAP_MESSAGES index, -1 none */
    int msg_step;     /* step that started the chain */
    float msg_time;
    bool msg_wait_input;
    bool msg_voice;      /* page has an XA clip (BgmStatusFlag_VoiceDialog) */
    int msg_load;        /* Gfx_MapMsg_Draw loadAudio: 1 waiting for XaLoadPending, 2 pending seen */
    float msg_load_time;

    /* Sound (map0_s01 D_800E23A1 + per-stage one-shots) */
    int radio_vol;       /* D_800E23A1: radio loop volume (Q8) */
    float bird_sfx_time;

    /* Screen fade (0 clear .. 1 black) */
    float fade;
    float fade_target;
    float fade_speed;

    /* Cutscene camera (3DS space) */
    bool cam_active;
    ShCamera cam;
    int cam_road_idx;     /* sticky VC road (-1 = pick fresh) */
    float walk_yaw;       /* camera yaw (render + screen-forward) */
    float move_yaw;       /* stick basis; frozen briefly on VC road switch */
    float cam_road_hold;  /* unused (kept for layout) */
    float cam_ideal_ang;  /* vcWork.cam_chara2ideal_ang_y (PS1 radians) */
    float cam_watch[3];   /* smoothed look-at (3DS) */
    float cam_prev_px, cam_prev_pz;
    bool cam_snap;        /* next gameplay camera update cuts instead of moving */

    /* Cybil (3DS space) */
    bool cybil_visible;
    float cybil_pos[3];
    float cybil_yaw;
    float cybil_anim;     /* seconds into current SBL.ANM clip */
    int cybil_clip;       /* CybilAnim_* (1, 5..11) — see CYBIL_ANIM_INFOS */

    /* Harry cutscene anim — CharaAnimCmd_SetState status (HARRY_BASE / HB_M0S01). */
    int harry_anim_status; /* 0 = use locomotion idle */
    float harry_anim_time; /* seconds into current clip */

    /* Harry death (SysState_GameOver stub) */
    bool harry_dying;
    float die_timer;
    bool game_over; /* true once the death fade finishes → title */

    /* Air Screamer */
    ShBirdState bird_state;
    float bird_pos[3];
    float bird_yaw;
    float bird_hp;      /* float HP, BASE 380 */
    float bird_timer;
    float bird_cd;      /* bite cooldown */
    float bird_anim;
    int bird_anim_state; /* state the bird_anim clock was reset for */
    float bird_phase;
    bool bird_bit;      /* bite landed this dive */
    float perch_time;   /* D_800E2560 */
    float player_warp[2];

    /* Prompts (bottom screen) */
    int prompt_kind; /* 0 none, 1 knife, 2 light, 3 map, 4 radio, 5 door, 6 leave, 7/8 health drink */
    bool prompt_open;
    int prompt_msg;

    /* Event_ItemTake — grow then spin item, then Yes/No (Gfx_PickupItemAnimate). */
    bool pickup_active;
    float pickup_scale; /* 0..1, Q12(0.5) * 2 on PS1 */
    float pickup_spin;
    /* Combat */
    bool aiming;
    float attack_cd;
    float fire_anim; /* >0: play handgun/knife attack clip */
    int ammo;
    float hit_flash;
    float gun_flash; /* muzzle flash overlay (seconds) */
    const char *combat_text;
    float combat_text_time;

    /* Post-pickup confirm / paper map */
    const char *got_text;
    float got_time;
    bool map_open;

    float radio;          /* -1 = let the game decide */
    bool controls_locked;
    bool leave_town; /* true once SH_CAFE_LEAVING fade finishes */

    /* World objects (CutsceneObjects_Add state + item flags) */
    unsigned world_key;

    /* DMS */
    ShDms dms;
    uint8_t dms_data[SH_CAFE_DMS_MAX];
    size_t dms_size;
    int dms_file; /* 0 none, 1 CAFE.DMS, 2 CAFE2.DMS */
} ShCafe;

void sh_cafe_reset(ShCafe *cafe);
void sh_cafe_begin(ShCafe *cafe, ShSavegame *save);
/* Dev skip: jump past Cybil cutscene into free explore (knife/map not given). */
void sh_cafe_begin_explore(ShCafe *cafe, ShSavegame *save);
/* Dev skip: knife+map+gun ready, Air Screamer fight starts immediately. */
void sh_cafe_begin_bird_fight(ShCafe *cafe, ShSavegame *save);

/* true = tank controls disabled this frame. */
bool sh_cafe_update(ShCafe *cafe, ShSavegame *save, float *player_x, float *player_y, float *player_z,
                    float *player_yaw, float *radio_intensity, const ShInputState *input, float dt);

/* Bgm_Update(false) for map0_s01 (Map_RoomBgmInit_CondFalse); call once per in-game frame. */
void sh_cafe_bgm_update(const ShCafe *cafe, const ShSavegame *save, bool player_dead, float dt);

const char *sh_cafe_subtitle(const ShCafe *cafe); /* top screen (MAP_MESSAGES) */
const char *sh_cafe_dialogue(const ShCafe *cafe); /* bottom screen info */
const char *sh_cafe_hint(const ShCafe *cafe);
bool sh_cafe_letterbox(const ShCafe *cafe);
float sh_cafe_fade(const ShCafe *cafe);

/* Cutscene camera override. */
bool sh_cafe_camera(const ShCafe *cafe, ShCamera *out);
bool sh_cafe_pickup_active(const ShCafe *cafe);
/* PS1 InvItemId of the item being shown (UNQ<id>.TMD), its grow factor and spin (radians). */
bool sh_cafe_pickup_item(const ShCafe *cafe, int *item_id, float *scale, float *spin);

/* Actor poses: index SH_CAFE_ACTOR_*; returns visibility. */
bool sh_cafe_actor_pose(const ShCafe *cafe, int actor, ShActorPose *out);
int sh_cafe_actor_keyframe(const ShCafe *cafe, int actor, int kf_count);

/* Cutscene Harry: true while a scripted anim status is active (not locomotion). */
bool sh_cafe_harry_cutscene_anim(const ShCafe *cafe, int *status_out, float *time_out);

/* CutsceneObjects_Add state: 0 interior, 1 exterior (intro), 2 broken glass. */
int sh_cafe_world_state(const ShCafe *cafe, const ShSavegame *save);
/* Changes whenever the set of world objects to draw changes. */
unsigned sh_cafe_world_key(const ShCafe *cafe, const ShSavegame *save);

/* Player wants to run (R) only when not aiming. */
bool sh_cafe_aiming(const ShCafe *cafe);
float sh_cafe_gun_flash(const ShCafe *cafe);

/* True when the cafe sequence wants the game to warp Harry onto the town street. */
bool sh_cafe_leave_town(const ShCafe *cafe);
/* True after Harry dies (0 HP) and the fade finishes — return to title. */
bool sh_cafe_game_over(const ShCafe *cafe);

/* Cycle knife <-> handgun among owned weapons (L). */
void sh_cafe_cycle_weapon(ShCafe *cafe, ShSavegame *save);

/* True while the residential paper map should cover the top screen. */
bool sh_cafe_map_open(const ShCafe *cafe);
void sh_cafe_toggle_map(ShCafe *cafe, const ShSavegame *save);

#ifdef __cplusplus
}
#endif
