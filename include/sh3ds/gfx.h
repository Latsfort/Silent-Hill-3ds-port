#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sh3ds/chara.h"
#include "sh3ds/environment.h"
#include "sh3ds/map.h"
#include "sh3ds/maptex.h"
#include "sh3ds/tim.h"
#include "sh3ds/water.h"
#include "sh3ds/water_surface.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShColor {
    float r, g, b, a;
} ShColor;

typedef struct ShCamera {
    float pos[3]; /* meters, Y up */
    float yaw;    /* radians, 0 looks toward -Z */
    float pitch;  /* radians, positive looks up */
    float fov_deg; /* vertical FOV; <= 0 = default 48° */
} ShCamera;

bool sh_gfx_init(void);
void sh_gfx_shutdown(void);

/* Waits for the GPU to finish the previous frame: GPU buffers are safe to rewrite after this. */
void sh_gfx_begin_frame(void);
void sh_gfx_end_frame(void);

void sh_gfx_draw_boot_screen(const char *line1, const char *line2, const char *line3);
/* Title / new-game menu (top fog band + bottom controls). */
void sh_gfx_draw_title_screen(const char *line1, const char *line2, const char *line3, const char *bottom);

/* Queue a TIM for upload to the shared texture at the next sh_gfx_begin_frame. */
void sh_gfx_set_image(const ShTim *tim);
/* Crop source rect before upload (PS1 paper maps use a 320x240 window in a taller TIM). sw/sh <= 0 = full image. */
void sh_gfx_set_image_crop(const ShTim *tim, int sx, int sy, int sw, int sh);

/* Draw the current image fitted to the top screen, with text on the bottom screen. */
void sh_gfx_draw_viewer(const char *title, const char *info, const char *hint);

/*
 * Two map vertex buffers in linear memory, allocated once: the front one is
 * drawn while the back one is rebuilt, possibly over several frames.
 */
ShMapVertex *sh_gfx_map_back_storage(size_t *capacity);
/* False when linear memory only fit one VBO: emit must finish before the next draw. */
bool sh_gfx_map_double_buffered(void);
/* Flushes the back buffer and makes it the drawn one. `ranges` use pool page indices. */
void sh_gfx_map_present(size_t vertex_count, const ShMapRange *ranges, int range_count);
/* Stops drawing the map; every atlas page becomes free immediately. */
void sh_gfx_map_clear(void);

/* Pool of atlas pages (SH_ATLAS_SIZE squared, RGBA5551) allocated at init. */
#define SH_GFX_ATLAS_POOL 6
int sh_gfx_atlas_pages(void);
/* Pages referenced by the drawn map must not be written. */
bool sh_gfx_atlas_page_busy(int page);
/* Copies a w*h texel rect of `tim` at (sx, sy), colored with `clut_offset`, to (dx, dy) in `page`. */
void sh_gfx_atlas_clear_page(int page);
void sh_gfx_atlas_blit(int page, int dx, int dy, const ShTim *tim, int sx, int sy, int w, int h, int clut_offset);
void sh_gfx_atlas_flush_page(int page);

typedef struct ShPlayerPose {
    bool visible;
    float pos[3]; /* feet, meters */
    float yaw;    /* radians, 0 faces -Z */
} ShPlayerPose;

/* Bright placeholder body (always works). */
bool sh_gfx_set_player_marker(void);
/* Copies a baked character mesh into the player VBO; false when the VBO is missing. */
bool sh_gfx_set_player_mesh(const ShMapVertex *verts, size_t count);
/*
 * Upload HERO.TIM into the player atlas (1024x512, 256x192 sheets).
 * `clut_deltas[i]` is the PS1 CLUT id for sheet slot i; count <= 8.
 */
bool sh_gfx_set_player_tim(const ShTim *tim, const unsigned *clut_deltas, int count);
/* Upload atlas sheets; each slot may come from `hero` or `held` per ShCharaClutMap::tim_src. */
bool sh_gfx_set_player_clut_atlas(const ShTim *hero, const ShTim *held, const ShCharaClutMap *map);

/* NPC / cutscene actor slots (Air Screamer, Cybil…). */
#define SH_GFX_MAX_ACTORS 3 /* Cheryl + 2, or 3 Grey Children (MapEvent_GreyChildrenSpawn) */
#define SH_GFX_ACTOR_MAX_VERTS (900 * 3)
bool sh_gfx_actor_set_mesh(int slot, const ShMapVertex *verts, size_t count);
/* Upload a character TIM into the actor's 512x256 atlas (256x128 sheets, <=4 CLUT slots). */
bool sh_gfx_actor_set_tim(int slot, const ShTim *tim, const unsigned *clut_deltas, int count);
void sh_gfx_actor_clear(int slot);

/*
 * Pickup close-up item (Gfx_PickupItemAnimate): camera-space, pre-sorted triangles
 * drawn over the scene without depth test; vertex color is half the light intensity.
 */
bool sh_gfx_item_set_tim(const ShTim *tim, unsigned clut_delta);
bool sh_gfx_item_set_mesh(const ShMapVertex *verts, size_t count);

typedef struct ShActorPose {
    bool visible;
    int slot;
    float pos[3];
    float yaw;
} ShActorPose;

/* One paper-map marking (func_80068E0C quad): atlas rect + PS1 320x240 screen rect. */
typedef struct ShMapSprite {
    uint8_t u, v, w, h;
    float x, y, sw, sh;
    float shade; /* 0..1, PS1 vertex colour 0..128 */
} ShMapSprite;

#define SH_MAP_MAX_SPRITES 48
#define SH_MAP_MAX_BOXES 5

typedef struct ShMapHud {
    const char *title;    /* bottom screen */
    const char *info;     /* bottom screen */
    const char *hint;     /* bottom screen */
    const char *subtitle; /* top screen, centered near the bottom */
    const struct ShMapChoice *choice;
    const char *choice_options[3];
    bool letterbox;       /* cutscene bars */
    bool retail_borders;
    int32_t border_shade;
    float fade;           /* 0 = none, 1 = black */
    float gun_flash;      /* brief white flash on shot (0..1) */
    const ShEnvState *environment;
    const ShEnvLight *environment_light;
    uint32_t world_object_mask;
    uint32_t moving_object_mask;
    const struct ShSilverMotion *moving_silver;
    const struct ShDrainValve *moving_valve;
    uint32_t moving_valve_mask;
    float moving_valve_pivot[3];
    uint32_t moving_y_mask;
    int16_t moving_y_rotation;
    float moving_y_pivot[3];
    uint32_t moving_vertical_mask;
    int32_t moving_vertical_q8;
    const int16_t *moving_rotations;
    uint32_t moving_locker_masks[3];
    float moving_locker_pivots[3][3];
    const struct ShSmoke *smoke;
    const ShWaterPool *water;
    const ShWaterConfig *water_config;
    const ShWaterVisibility *water_visibility;
    const ShWaterSurfaceQuad *water_surface;
    size_t water_surface_count;
    const ShWaterSprayConfig *water_spray;
    size_t water_spray_count;
    bool piano_cursor;
    int16_t piano_cursor_x, piano_cursor_y;
    const ShTim *overlay; /* optional fullscreen TIM on the top screen (paper map) */
    /* PaperMap_DrawScaled: scale 1 = full 320x240, 0.5 = 2x zoom; scroll in map px. */
    float overlay_scale;
    float overlay_scroll_x;
    float overlay_scroll_y;
    const ShMapSprite *map_sprites; /* MR_* markings, see sh_gfx_set_map_marks_atlas */
    int map_sprite_count;
    /* PaperMap_ExpandingBoxesDraw outlines in PS1 320x240 space: x0, y0, x1, y1. */
    float map_boxes[SH_MAP_MAX_BOXES][4];
    int map_box_count;
    /* func_80067914 player triangle (PS1 map sprite space, see draw_map_sprites). */
    bool map_player_valid;
    float map_player_x;
    float map_player_y;
    float map_player_yaw; /* PS1 rotation.vy */
} ShMapHud;

/* Paper-map marking atlas (MR_*.TIM); uploaded once, NULL clears it. */
void sh_gfx_set_map_marks_atlas(const ShTim *tim);
bool sh_gfx_set_smoke_atlas(const ShTim *tim);
bool sh_gfx_set_water_atlas(const ShTim *tim);
bool sh_gfx_set_piano_cursor(const ShTim *tim);
bool sh_gfx_prepare_moving_object(uint32_t mask);
size_t sh_gfx_moving_vertex_count(void);
size_t sh_gfx_smoke_vertex_count(void);
size_t sh_gfx_water_vertex_count(void);

void sh_gfx_draw_map(const ShCamera *camera, const ShPlayerPose *player, const ShActorPose *actors, int actor_count,
                     const ShMapHud *hud);

void sh_gfx_set_clear_color(ShColor color);

#ifdef __cplusplus
}
#endif
