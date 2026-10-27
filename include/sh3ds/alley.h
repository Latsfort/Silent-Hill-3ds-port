#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sh3ds/dms.h"
#include "sh3ds/gfx.h"
#include "sh3ds/input.h"
#include "sh3ds/savegame.h"
#include "sh3ds/vc.h"
#include "sh3ds/path.h"
#include "sh3ds/border.h"
#include "sh3ds/environment.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * map0_s00 intro — MapEvent_CutsceneOpening through cafe load (EventFlag_26).
 * Coords: 3DS metres, X = PS1 X, Y = -PS1 Y, Z = -PS1 Z.
 */

#define SH_ALLEY_DMS_MAX 4096

#define SH_EV_OPENING 1
#define SH_EV_FOOTSTEPS 2
#define SH_EV_ALLEY_FMV 3
#define SH_EV_SPOTTED 4
#define SH_EV_INTO_ALLEY 7
#define SH_EV_CHASE_GATE 12
#define SH_EV_REDIRECT 13
#define SH_EV_ALLEY_DARK 17
#define SH_EV_NIGHTMARE 24
#define SH_EV_INTRO_DONE 26

#define SH_ALLEY_MAX_GREY 3 /* MapEvent_GreyChildrenSpawn */

typedef enum ShAlleyStage {
    SH_ALLEY_OFF = 0,
    SH_ALLEY_OPENING,
    SH_ALLEY_STREET,
    SH_ALLEY_FOOTSTEPS,
    SH_ALLEY_FMV,
    SH_ALLEY_SPOTTED,
    SH_ALLEY_CHASE,
    SH_ALLEY_REDIRECT,
    SH_ALLEY_INTO,      /* MapEvent_CutsceneCherylIntoTheAlley */
    SH_ALLEY_ROAM,      /* free roam down the alley after Cheryl */
    SH_ALLEY_DARKER,    /* MapEven_CutsceneAlleyGetsDarker */
    SH_ALLEY_DEAD_END,  /* "A dead end? What the hell!?" */
    SH_ALLEY_FIGHT,     /* Grey Children after EventFlag_17 */
    SH_ALLEY_NIGHTMARE, /* MapEvent_CutsceneAlleyNightmare → blackout */
    SH_ALLEY_BLACKOUT,  /* hold black, stop SFX, hand off to cafe */
    SH_ALLEY_ROOM,
    SH_ALLEY_TO_CAFE
} ShAlleyStage;

typedef struct ShAlleyGrey {
    bool alive;
    float pos[3];
    float yaw;
    float hp;
    float anim;
    float cd;
} ShAlleyGrey;

typedef struct ShAlley {
    ShAlleyStage stage;
    int step;
    float timer;
    float wait;
    float fade;
    float fade_target;
    float fade_speed;
    int32_t darkness;
    ShEnvironment environment;
    ShBorder border;
    ShPathPlayer torch_player;
    int32_t torch_environment_time;
    bool torch_active, torch_match, grey_assets_loaded;
    int border_step;
    float room_x, room_z, room_yaw;
    uint16_t room_complete_flag;
    bool room_ready;
    bool redirected;
    bool movie_requested;
    bool movie_finished;
    bool flame_requested;

    bool cam_active;
    ShCamera cam;
    ShVc vc;
    bool vc_scripted;

    bool cheryl_visible;
    float cheryl_pos[3];
    float cheryl_yaw;
    float cheryl_anim;
    int cheryl_wp; /* chase / into-alley waypoint index */

    int harry_anim_status;
    float harry_anim_time;

    /* Distant Cheryl footstep SFX timer (Cheryl_DistantFootstepSfxPlay). */
    float foot_sfx_t;

    /* Voice: XA cmd currently playing / pending stop. */
    uint16_t voice_cmd;
    float voice_t;
    bool voice_on;

    const char *subtitle;
    const char *dialogue;
    float msg_time;
    bool letterbox;
    bool controls_locked;
    bool go_to_cafe;
    /* Set when a cutscene warps Harry — game snaps to ground/collision. */
    bool need_ground_snap;
    /* Pass alley gate mesh during IntoTheAlley / fight approach. */
    bool skip_collision;
    /* game.c should load CAT.* into actor slots 0..2. */
    bool need_grey_assets;

    ShAlleyGrey grey[SH_ALLEY_MAX_GREY];
    int grey_count;

    ShDms dms;
    uint8_t dms_data[SH_ALLEY_DMS_MAX];
    size_t dms_size;
    bool dms_ok;
} ShAlley;

void sh_alley_reset(ShAlley *alley);
void sh_alley_begin(ShAlley *alley, ShSavegame *save);

bool sh_alley_active(const ShAlley *alley);
bool sh_alley_go_to_cafe(const ShAlley *alley);

bool sh_alley_update(ShAlley *alley, ShSavegame *save, float *player_x, float *player_y, float *player_z,
                     float *player_yaw, const ShInputState *input, float dt);

const char *sh_alley_subtitle(const ShAlley *alley);
const char *sh_alley_dialogue(const ShAlley *alley);
const char *sh_alley_hint(const ShAlley *alley);
bool sh_alley_letterbox(const ShAlley *alley);
float sh_alley_fade(const ShAlley *alley);
bool sh_alley_camera(const ShAlley *alley, ShCamera *out);
bool sh_alley_harry_keyframe(const ShAlley *alley, int *keyframe);
bool sh_alley_gameplay_camera(ShAlley *alley, const float position[3], float yaw, float dt, ShCamera *out);

bool sh_alley_actor_pose(const ShAlley *alley, int slot, ShActorPose *out);
int sh_alley_actor_keyframe(const ShAlley *alley, int slot, int kf_count);

bool sh_alley_harry_cutscene_anim(const ShAlley *alley, int *status_out, float *time_out);

/* True once after a cutscene warp; clears the flag. */
bool sh_alley_take_ground_snap(ShAlley *alley);
bool sh_alley_skip_collision(const ShAlley *alley);
bool sh_alley_needs_grey_assets(const ShAlley *alley);
void sh_alley_clear_grey_assets_flag(ShAlley *alley);

#ifdef __cplusplus
}
#endif
