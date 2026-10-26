#pragma once

#include <stdbool.h>

#include "sh3ds/gfx.h"
#include "sh3ds/savegame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* bodyprog paper map: marking quads (func_80068E0C) and PaperMap_ExpandingBoxesDraw. */

#define SH_PAPERMAP_OLD_TOWN 1

/* Mode 1: every marking whose event flag is set. Returns sprite count. */
int sh_papermap_marks(const ShSavegame *save, int map_idx, float scroll_x, float scroll_y, float scale,
                      ShMapSprite *out, int max);

/* Mode 2: the single marking tied to `flag`, drawn at `shade` (0..1). */
bool sh_papermap_mark_flag(int map_idx, unsigned flag, float shade, float scroll_x, float scroll_y, float scale,
                           ShMapSprite *out);

/*
 * Rects are PS1 centred coords {x, y, w, h} in the 320x480 interlaced space.
 * Writes SH_MAP_MAX_BOXES outlines as {x0, y0, x1, y1} in 320x240 space.
 */
void sh_papermap_boxes(float progress, const float start[4], const float end[4], float out[SH_MAP_MAX_BOXES][4]);

/* func_80067914 — MAP2_S00 / PaperMapIdx_OldTown near-cafe chunk formula. */
/* map_idx = e_MapIdx (MAP2_S00 or MAP0_S02); out_turn_add = retail angle offset in turns. */
bool sh_papermap_player_map2_s00(int map_idx, float ps1_x, float ps1_z, float ps1_yaw, float scroll_x, float scroll_y,
                                 float scale, float *out_x, float *out_y, float *out_turn_add);

#ifdef __cplusplus
}
#endif
