#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sh3ds/alley.h"
#include "sh3ds/cafe.h"
#include "sh3ds/town.h"
#include "sh3ds/gfx.h"
#include "sh3ds/coll.h"
#include "sh3ds/input.h"
#include "sh3ds/inventory.h"
#include "sh3ds/maptex.h"
#include "sh3ds/movie.h"
#include "sh3ds/radio.h"
#include "sh3ds/savegame.h"
#include "sh3ds/save_menu.h"
#include "sh3ds/tim.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Game shell bridging the 3DS platform layer to logic eventually ported from
 * vendor/silent-hill-decomp (bodyprog / maps / screens).
 *
 * Upstream README states the PS1 decomp is not port-ready: we do not compile
 * PsyQ / MIPS overlays here. Modules are stubbed and replaced incrementally.
 */

#define SH_GAME_MAX_TEXTURES 256
#define SH_GAME_NAME_LEN 16
#define SH_GAME_MAX_AREAS 32
#define SH_GAME_AREA_LEN 8
#define SH_GAME_MAX_CHUNKS 192

typedef enum ShGamePhase {
    SH_PHASE_BOOT = 0,
    SH_PHASE_TITLE,
    SH_PHASE_LOADING, /* building first map window after New Game */
    SH_PHASE_MAP,
    SH_PHASE_GAME_OVER, /* Harry at 0 HP — fade then back to title */
    SH_PHASE_VIEWER, /* debug TIM browser — not the main path */
    SH_PHASE_SAVE,
    SH_PHASE_MOVIE,
    SH_PHASE_EXIT
} ShGamePhase;

typedef struct ShMapChunk {
    int x;
    int z;
    const uint8_t *data;
    size_t size;
} ShMapChunk;

typedef enum ShWindowStage {
    SH_WINDOW_IDLE = 0,
    SH_WINDOW_BUILD,    /* one chunk per step */
    SH_WINDOW_PACK,
    SH_WINDOW_TEXTURES, /* one (TIM, palette) pair per step */
    SH_WINDOW_EMIT      /* a slice of triangles per step */
} ShWindowStage;

/* Rebuild of the 3x3 chunk window into the back buffers, spread over frames. */
typedef struct ShWindowObject {
    size_t first;
    size_t count;
    uint32_t mask;
} ShWindowObject;

typedef struct ShWindowJob {
    ShWindowStage stage;
    int cx, cz;
    int chunks[9];
    int chunk_count;
    int next;
    ShMapBuilder builder;
    ShMapTexSet textures;
    ShMapTexLayout layout;
    int pages;
    int page_map[SH_MAPTEX_MAX_PAGES]; /* logical atlas page -> pool page */
    int pair_order[SH_MAPTEX_MAX_PAIRS];
    char tim_name[8];
    bool tim_ok;
    bool inplace; /* reuses pages the current view draws; finish before the next draw */
    ShTim tim;
    int lost;
    unsigned world_key; /* sh_cafe_world_key at build time */
    size_t static_vert_count;
    ShWindowObject objects[SH_MAP_MAX_OBJECTS];
    int object_count;
} ShWindowJob;

typedef struct ShGame {
    ShGamePhase phase;
    bool loading_ambient;
    unsigned frame;
    bool wants_exit;

    char texture_names[SH_GAME_MAX_TEXTURES][SH_GAME_NAME_LEN];
    int texture_count;
    int texture_index;
    ShTim texture;
    bool texture_loaded;

    char area_names[SH_GAME_MAX_AREAS][SH_GAME_AREA_LEN];
    int area_count;
    int area_index;
    bool area_loaded;
    const char *load_error;
    bool smoke_ready;
    bool water_ready;
    bool piano_cursor_ready;
    ShMapChunk chunks[SH_GAME_MAX_CHUNKS];
    int chunk_count;
    const uint8_t *glb;
    size_t glb_size;

    ShCamera camera;
    float player_pos[3]; /* feet, meters */
    float player_yaw;    /* radians, 0 faces -Z */
    float cam_pitch;     /* look offset; chase cam yaw follows player */
    bool harry_ready;
    float harry_anim_time; /* fractional keyframe index */
    int harry_anim_kf;     /* last baked integer keyframe */
    ShCollWorld collision;
    ShSavegame save;
    ShRadio radio;
    ShAlley alley;
    ShCafe cafe;
    ShTown town;
    ShInventory inventory;
    ShSaveMenu save_menu;
    ShGamePhase save_return_phase;
    ShMovie movie;
    uint64_t movie_start_ms;
    int title_sel;
    float radio_time; /* seconds, for static pulse */
    bool noclip; /* L+R together toggles; when off, walk on IPD collision */
    int window_x;
    int window_z;
    unsigned window_world_key;
    bool window_valid;
    size_t window_triangles;
    bool window_overflow;
    int window_pairs;
    int window_pages;
    int window_textures_lost; /* pairs that did not fit in the atlas or whose TIM is missing */

    ShWindowJob job;
    ShMapVertex *job_verts; /* stream arena */
    uint8_t *job_tri_slots;
    size_t job_capacity;
    uint8_t *job_tim_file;

    uint64_t last_tick_ms;
    float fps;
} ShGame;

bool sh_game_init(ShGame *game);
void sh_game_shutdown(ShGame *game);

/* Must run after sh_gfx_begin_frame: it may rewrite GPU buffers. */
void sh_game_update(ShGame *game, const ShInputState *input);
void sh_game_render(const ShGame *game);

#ifdef __cplusplus
}
#endif
