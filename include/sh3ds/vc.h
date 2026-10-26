#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sh3ds/gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Gameplay camera (bodyprog/view/vc_main.c): VC road selection, ideal position,
 * camera/watch springs and the keep-Harry-in-screen adjustment.
 * All state is PS1 space (metres, Y down, angles in radians with 0 = +Z).
 */

/* One vc_road_data.h entry; layout matches tools/gen_vc_roads.py output. */
typedef struct ShVcRoad {
    float sw[4]; /* lim_sw: min_x, max_x, min_z, max_z */
    float rd[4]; /* lim_rd */
    uint8_t flags;
    uint8_t area;    /* VC_AREA_* */
    uint8_t rd_type; /* VC_RD_TYPE_* */
    uint8_t mv_y;    /* VC_MV_* used for Y limits */
    float max_hy, min_hy;
    float ofs_watch_hy;
    uint8_t cam_mv; /* VC_MV_* */
    float fix_x_deg, fix_y_deg;
} ShVcRoad;

typedef struct ShVc {
    const ShVcRoad *roads;
    int road_count;
    int cur; /* -1 = no road yet */

    float cam_pos[3];
    float cam_vel[3];
    float cam_tgt[3];
    float ideal_ang;
    float ang[2];     /* pitch (down positive), yaw */
    float ang_spd[2];

    float prev_pos[3];
    float prev_face;
    bool warp; /* next update cuts */
    bool user_camera, user_camera_warp;
    float user_camera_target[3], user_camera_params[4];
} ShVc;

void sh_vc_init(ShVc *vc, const ShVcRoad *roads, int road_count);
/* Cut on the next update (vcReturnPreAutoCamWork / road warp). */
void sh_vc_warp(ShVc *vc);

typedef struct ShVcScriptParams {
    float camera[4]; /* XZ/Y acceleration, XZ/Y maximum speed. */
    float watch[4]; /* X/Y angular acceleration, X/Y maximum speed (turns). */
} ShVcScriptParams;

void sh_vc_camera_target(ShVc *vc, const float position[3], const ShVcScriptParams *params, bool warp);
void sh_vc_camera_release(ShVc *vc, bool warp);

void sh_vc_script_update(ShVc *vc, const float position[3], const float look[3],
                         const ShVcScriptParams *params, bool warp_camera,
                         bool warp_watch, float dt, ShCamera *out);

/* pos = Harry PS1 position (feet), face = PS1 facing angle. */
void sh_vc_update(ShVc *vc, const float pos[3], float face, bool near_enemy, float dt, ShCamera *out);

/* SetGeomScreen(gsScreenHeight=240) on a 240-line screen: 2*atan(120/240). */
#define SH_VC_FOV_DEG 53.13f

#ifdef __cplusplus
}
#endif
