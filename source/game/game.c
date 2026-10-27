#include "sh3ds/game.h"
#include "sh3ds/autotest.h"
#include "sh3ds/assets.h"
#include "sh3ds/cafe.h"
#include "sh3ds/chara.h"
#include "sh3ds/coll.h"
#include "sh3ds/controls.h"
#include "sh3ds/debug_checkpoints.h"
#include "sh3ds/fixed.h"
#include "sh3ds/event_image.h"
#include "sh3ds/gfx.h"
#include "sh3ds/inventory.h"
#include "sh3ds/item_model.h"
#include "sh3ds/piano_render_data.h"
#include "sh3ds/map.h"
#include "sh3ds/memory.h"
#include "sh3ds/npc_load.h"
#include "sh3ds/radio.h"
#include "sh3ds/savegame.h"
#include "sh3ds/snd.h"
#include "sh3ds/sound.h"
#include "sh3ds/smoke_render_data.h"
#include "sh3ds/water_render_data.h"
#include "sh3ds/config.h"

#include "map_world_objects_map2_s00.h"
#include "overlay_data.h"

#include <3ds.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool development_enabled(void)
{
    return SH3DS_DEVELOPMENT != 0 || sh_autotest_active();
}

/*
 * Mapping note (reference only — see vendor/silent-hill-decomp):
 *   src/main      -> process bootstrap / memory manager (PS1)
 *   src/bodyprog  -> core game logic overlay
 *   src/screens   -> UI / title / inventory overlays
 *   src/maps      -> map stage overlays
 *
 * None of those units compile on ARM11/3DS without a full platform rewrite.
 */

#define SH_TEXTURE_DIR "romfs:/game/tim"
#define SH_BG_DIR "romfs:/game/bg"
#define SH_CHARA_DIR "romfs:/game/chara"
#define SH_ANIM_DIR "romfs:/game/anim"

#define SH_CHUNK_METERS 40.0f
#define SH_CAM_DISTANCE 2.35f
#define SH_CAM_MIN_DISTANCE 0.6f
#define SH_CAM_WALL_MARGIN 0.3f
#define SH_CAM_HEIGHT 1.55f
#define SH_LOOK_HEIGHT 1.20f
#define SH_WALK_SPEED 2.8f  /* m/s — default tank walk */
#define SH_RUN_SPEED 5.2f   /* m/s — hold R */
/* map0_s00 intro spawn — MAP_POINTS[0] (PS1 XZ → 3DS x,-z). */
#define SH_INTRO_SPAWN_X (-6.2f)
#define SH_INTRO_SPAWN_Z (-160.5f)
/* map0_s01 Cafe interior (PS1 XZ → 3DS x,-z). */
#define SH_CAFE_SPAWN_X 1.0f
#define SH_CAFE_SPAWN_Z (-260.0f)
/* Post-Cybil explore start near the booth. */
#define SH_CAFE_EXPLORE_X 4.4f
#define SH_CAFE_EXPLORE_Z (-269.9f)
#define SH_LIFT_SPEED 3.0f
#define SH_TURN_SPEED 2.2f  /* rad/s */
#define SH_PITCH_LIMIT 0.55f
#define SH_WALK_KF_RATE 14.0f /* keyframes / s while walking (HB_BASE 1..23) */
#define SH_RUN_KF_RATE 18.0f  /* keyframes / s while running (26..45) */
#define SH_IDLE_KF_RATE 8.0f  /* keyframes / s while idle (503..542) */
#define SH_HERO_ILM_MAX (16u * 1024u)
#define SH_HERO_ANM_MAX (96u * 1024u)  /* HB_BASE.ANM ~89 KB — locomotion / idle */
#define SH_HERO_WEP_MAX (16u * 1024u) /* HB_WEPn.ANM — headerless keyframes, index 568+ */
#define SH_HERO_MAP_MAX (80u * 1024u) /* HB_M0S00/01.ANM — cutscene keyframes from 676 */
#define SH_HELD_PLM_MAX (4u * 1024u)
#define SH_HERO_TIM_MAX (32u * 1024u)
#define SH_MAP_TIM_MAX (160u * 1024u) /* MP_0TOWN.TIM ~154 KB */
/* PS1 g_PaperMapImg: 320x240 viewport at v=16 inside the 320x480 TIM. */
#define SH_PAPER_MAP_CROP_Y 16
#define SH_PAPER_MAP_VIEW_W 320
#define SH_PAPER_MAP_VIEW_H 240

/*
 * HARRY_WEAPON_ANIM_INFOS + D_800294F4 — keyframes in HB_WEP4 (handgun) / HB_WEP3 (knife).
 * HandgunRecoil (605..658) is the RELOAD clip — fire / short recoil is Unk29 (582..592).
 */
#define SH_KF_GUN_AIM_START 570
#define SH_KF_GUN_AIM_END 579
#define SH_KF_GUN_FIRE_START 582
#define SH_KF_GUN_FIRE_END 592
#define SH_KF_KNIFE_AIM_START 568
#define SH_KF_KNIFE_AIM_END 577
/* Kitchen knife — D_800294F4 anim 29 playback 579..598 */
#define SH_KF_KNIFE_FIRE_START 579
#define SH_KF_KNIFE_FIRE_END 598
/* Handgun reload / pump (anim 31) starts here — never play when firing handgun. */
#define SH_KF_GUN_RELOAD_START 605
#define SH_HARRY_HELD_BONE 10 /* HarryBone_RightHand */
#define SH_AIM_KF_RATE 30.0f
#define SH_FIRE_KF_RATE 40.0f /* HARRY_WEAPON_ANIM_INFOS[81]: Q12(40) */

static int compare_names(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

static bool has_suffix(const char *name, const char *suffix)
{
    const size_t len = strlen(name);
    const size_t slen = strlen(suffix);
    return len >= slen && strcasecmp(name + len - slen, suffix) == 0;
}

/* Reads a whole romfs file into `buffer`; fails if it does not fit. */
static bool read_file_into(const char *path, uint8_t *buffer, size_t capacity, size_t *size_out)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    const bool ok = size > 0 && (size_t)size <= capacity && fread(buffer, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    if (ok) {
        *size_out = (size_t)size;
    }
    return ok;
}

/* Reads a whole romfs file into the level arena. */
static const uint8_t *load_file(const char *path, size_t *size_out)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        sh_assets_log("file open failed errno=%d %s", errno, path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *data = size > 0 ? sh_arena_alloc(sh_memory_level_arena(), (size_t)size, 16) : NULL;
    const bool ok = data && fread(data, 1, (size_t)size, file) == (size_t)size;
    if (!ok) {
        sh_assets_log("file read failed errno=%d size=%ld buffer=%d memory=%lu/%lu %s", errno, size,
                      data != NULL, (unsigned long)sh_memory_level_arena()->used,
                      (unsigned long)sh_memory_level_arena()->capacity, path);
    }
    fclose(file);

    if (!ok) {
        return NULL;
    }
    *size_out = (size_t)size;
    return data;
}

/* ---- Texture viewer ---------------------------------------------------- */

static void scan_textures(ShGame *game)
{
    game->texture_count = 0;

    DIR *dir = opendir(SH_TEXTURE_DIR);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && game->texture_count < SH_GAME_MAX_TEXTURES) {
        const size_t len = strlen(entry->d_name);
        if (len >= SH_GAME_NAME_LEN || !has_suffix(entry->d_name, ".TIM")) {
            continue;
        }
        memcpy(game->texture_names[game->texture_count], entry->d_name, len + 1);
        game->texture_count++;
    }
    closedir(dir);

    qsort(game->texture_names, (size_t)game->texture_count, SH_GAME_NAME_LEN, compare_names);

    /* Start on the real title screen when present. */
    for (int i = 0; i < game->texture_count; i++) {
        if (strcmp(game->texture_names[i], "TITLE_E.TIM") == 0) {
            game->texture_index = i;
            break;
        }
    }
}

static void load_texture(ShGame *game, int index)
{
    char path[64];

    game->texture_loaded = false;
    game->area_loaded = false;
    if (game->texture_count == 0) {
        return;
    }

    game->texture_index = (index % game->texture_count + game->texture_count) % game->texture_count;
    snprintf(path, sizeof(path), "%s/%s", SH_TEXTURE_DIR, game->texture_names[game->texture_index]);

    sh_arena_reset(sh_memory_level_arena());
    size_t size = 0;
    const uint8_t *data = load_file(path, &size);
    if (data && sh_tim_parse(data, size, &game->texture)) {
        game->texture_loaded = true;
        sh_gfx_set_image(&game->texture);
    }
}

/* ---- Map explorer ------------------------------------------------------ */

/* Chunk files are named <AREA><XX><ZZ>.IPD with signed hex chunk coordinates. */
static bool parse_chunk_name(const char *name, char *area, int *x, int *z)
{
    const size_t len = strlen(name);
    if (len < 9 || len - 8 >= SH_GAME_AREA_LEN || !has_suffix(name, ".IPD")) {
        return false;
    }

    char hex[3] = {0};
    const size_t area_len = len - 8;
    memcpy(hex, name + area_len, 2);
    *x = (int8_t)strtol(hex, NULL, 16);
    memcpy(hex, name + area_len + 2, 2);
    *z = (int8_t)strtol(hex, NULL, 16);

    memcpy(area, name, area_len);
    area[area_len] = '\0';
    return true;
}

static void scan_areas(ShGame *game)
{
    game->area_count = 0;

    DIR *dir = opendir(SH_BG_DIR);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char area[SH_GAME_AREA_LEN];
        int x, z;
        if (!parse_chunk_name(entry->d_name, area, &x, &z)) {
            continue;
        }

        bool known = false;
        for (int i = 0; i < game->area_count && !known; i++) {
            known = strcmp(game->area_names[i], area) == 0;
        }
        if (!known && game->area_count < SH_GAME_MAX_AREAS) {
            strcpy(game->area_names[game->area_count++], area);
        }
    }
    closedir(dir);

    qsort(game->area_names, (size_t)game->area_count, SH_GAME_AREA_LEN, compare_names);

    /* Open on the Old Silent Hill streets when present. */
    for (int i = 0; i < game->area_count; i++) {
        if (strcmp(game->area_names[i], "THR") == 0) {
            game->area_index = i;
            break;
        }
    }
}

static void update_chase_camera(ShGame *game)
{
    const float s = sinf(game->player_yaw);
    const float c = cosf(game->player_yaw);
    const float look_y = game->player_pos[1] + SH_LOOK_HEIGHT;

    /* Stop the camera in front of the first building wall between Harry and it. */
    float dist = SH_CAM_DISTANCE;
    if (!game->noclip) {
        const float reach = SH_CAM_DISTANCE + SH_CAM_WALL_MARGIN;
        const float t = sh_coll_ray(&game->collision, game->player_pos[0], game->player_pos[2],
                                    game->player_pos[0] + s * reach, game->player_pos[2] + c * reach,
                                    game->player_pos[1], 0.6f);
        const float free_dist = t * reach - SH_CAM_WALL_MARGIN;
        if (free_dist < dist) {
            dist = free_dist > SH_CAM_MIN_DISTANCE ? free_dist : SH_CAM_MIN_DISTANCE;
        }
    }

    game->camera.pos[0] = game->player_pos[0] + s * dist;
    game->camera.pos[1] = game->player_pos[1] + SH_CAM_HEIGHT;
    game->camera.pos[2] = game->player_pos[2] + c * dist;

    /* Look at Harry's torso so he stays framed (fixes "invisible" + FOV feel). */
    const float dx = game->player_pos[0] - game->camera.pos[0];
    const float dy = look_y - game->camera.pos[1];
    const float dz = game->player_pos[2] - game->camera.pos[2];
    const float horiz = sqrtf(dx * dx + dz * dz);
    game->camera.yaw = atan2f(-dx, -dz);
    float pitch = (horiz > 1e-4f) ? atan2f(dy, horiz) : 0.0f;
    pitch += game->cam_pitch;
    if (pitch > SH_PITCH_LIMIT) {
        pitch = SH_PITCH_LIMIT;
    }
    if (pitch < -SH_PITCH_LIMIT) {
        pitch = -SH_PITCH_LIMIT;
    }
    game->camera.pitch = pitch;
}

static bool try_place_at(ShGame *game, float x, float z)
{
    ShCollHit hit;
    if (!sh_coll_ground(&game->collision, x, z, &hit)) {
        return false;
    }
    float px = x;
    float pz = z;
    if (!sh_alley_skip_collision(&game->alley)) {
        sh_coll_nudge(&game->collision, &px, &pz, hit.height, SH_COLL_PLAYER_RADIUS);
        if (!sh_coll_ground(&game->collision, px, pz, &hit)) {
            return false;
        }
    }
    game->player_pos[0] = px;
    game->player_pos[1] = hit.height;
    game->player_pos[2] = pz;
    return true;
}

static void place_player_in_chunk(ShGame *game, const ShMapChunk *chunk)
{
    game->player_yaw = 0.0f;
    game->cam_pitch = 0.0f;

    const float cx = (chunk->x + 0.5f) * SH_CHUNK_METERS;
    const float cz = -(chunk->z + 0.5f) * SH_CHUNK_METERS;
    if (!game->noclip && try_place_at(game, cx, cz)) {
        update_chase_camera(game);
        return;
    }

    /* Interiors (APU…): chunk center is often void or inside a wall — probe nearby. */
    if (!game->noclip) {
        const float offsets[] = {0.0f, 2.0f, -2.0f, 4.0f, -4.0f, 8.0f, -8.0f};
        for (size_t iz = 0; iz < sizeof(offsets) / sizeof(offsets[0]); iz++) {
            for (size_t ix = 0; ix < sizeof(offsets) / sizeof(offsets[0]); ix++) {
                if (try_place_at(game, cx + offsets[ix], cz + offsets[iz])) {
                    update_chase_camera(game);
                    return;
                }
            }
        }
        for (int i = 0; i < game->chunk_count; i++) {
            const float ox = (game->chunks[i].x + 0.5f) * SH_CHUNK_METERS;
            const float oz = -(game->chunks[i].z + 0.5f) * SH_CHUNK_METERS;
            if (try_place_at(game, ox, oz)) {
                update_chase_camera(game);
                return;
            }
        }
    }

    game->player_pos[0] = cx;
    game->player_pos[1] = 0.0f;
    game->player_pos[2] = cz;
    update_chase_camera(game);
}

/* Kept in BSS: level arena is wiped on map load; anim needs live ILM/ANM/TIM. */
static ShMapVertex s_harry_bake[SH_CHARA_MAX_VERTS];
static uint8_t s_harry_ilm[SH_HERO_ILM_MAX];
static uint8_t s_harry_anm[SH_HERO_ANM_MAX]; /* HB_BASE */
static uint8_t s_harry_wep_gun[SH_HERO_WEP_MAX];   /* HB_WEP4 — handgun, raw kf from 568 */
static uint8_t s_harry_wep_knife[SH_HERO_WEP_MAX]; /* HB_WEP3 — kitchen knife */
static uint8_t s_harry_map_anm[SH_HERO_MAP_MAX];   /* HB_M0Sxx — cutscene kf from 676 */
static size_t s_harry_wep_gun_size;
static size_t s_harry_wep_knife_size;
static size_t s_harry_map_anm_size;
static uint8_t s_plm_handgun[SH_HELD_PLM_MAX];
static uint8_t s_plm_knife[SH_HELD_PLM_MAX];
static uint8_t s_plm_phone[SH_HELD_PLM_MAX];
static uint8_t s_phone_tim_file[2048];
static ShTim s_phone_tim;
static bool s_phone_tim_ready;
static uint8_t s_plm_pipe[SH_HELD_PLM_MAX];
static uint8_t s_harry_tim[SH_HERO_TIM_MAX];
static uint8_t s_map_tim_file[SH_MAP_TIM_MAX];
static size_t s_harry_ilm_size;
static size_t s_harry_anm_size;
static size_t s_plm_handgun_size;
static size_t s_plm_knife_size;
static size_t s_plm_phone_size;
static size_t s_plm_pipe_size;
static ShTim s_harry_tim_parsed;
static ShTim s_map_tim_parsed;
static uint8_t s_sketch_tim_file[160u * 1024u]; /* Event images, including interlaced paper maps. */
static char s_sketch_tim_name[16];
static ShTim s_sketch_tim_parsed;
static bool s_sketch_tim_ok;
static uint8_t s_event_next_file[80u * 1024u];
static char s_event_next_name[16];
static ShTim s_event_next_tim;
static bool s_event_next_ok;
/* Screen_BackgroundImgTransition draws the retail 320x240 inspection image. */
static uint8_t s_event_blend_pixels[2][320u * 480u * 2u];
static ShTim s_event_blend_tim[2];
static uint8_t s_marks_tim_file[24u * 1024u]; /* MR_0TOWN.TIM, 4bpp 128x256 */
static ShTim s_marks_tim_parsed;
static ShMapSprite s_map_sprites[SH_MAP_MAX_SPRITES];
static bool s_harry_have_tim;
static bool s_map_tim_ok;
static bool s_harry_tim_uploaded;
static ShCharaClutMap s_harry_cluts;
static int s_harry_hand_flags = SH_CHARA_F_HARRY_HANDS;
static int s_harry_base_kf_count;
static bool s_harry_using_wep; /* last bake used weapon overlay */
static bool s_harry_phone_visible;
static ShCharaBone s_harry_light_bone;
static bool s_harry_light_bone_valid;
static ShCharaBone s_harry_hand_bone;
static bool s_harry_hand_bone_valid;
static ShEnvLight s_environment_light;

static unsigned harry_hand_flags(const ShGame *game)
{
    if (sh_alley_active(&game->alley) && game->alley.torch_match) {
        return SH_CHARA_F_HARRY_HANDS | SH_CHARA_F_HARRY_MATCH_HAND |
            (game->alley.torch_active ? 0 : SH_CHARA_F_HARRY_MATCH_ARM);
    }
    if (game->town.stage == SH_TOWN_SCRIPT && game->town.script == SH_TOWN_SCR_SCHOOL &&
          (game->town.harry_state == 83 || game->town.harry_state == 87 || game->town.harry_state == 88 ||
           game->town.school.empty_hand)) {
        /* sharedFunc_800CDAA8_0_s02_Switch: RightHandEmpty during pouring/climbing. */
        return SH_CHARA_F_HARRY_HANDS;
    }
    const uint8_t w = game->save.equipped_weapon;
    /* Keep weapon hand meshes whenever equipped (PS1 WorldGfx_CharaMeshSwap). */
    if (w == SH_ITEM_HANDGUN) {
        return SH_CHARA_F_HARRY_GUN; /* 10RHAND3, not rifle 10RHAND4 */
    }
    if (w == SH_ITEM_POCKET_KNIFE) {
        return SH_CHARA_F_HARRY_KNIFE;
    }
    return SH_CHARA_F_HARRY_HANDS;
}

static bool harry_phone_visible(const ShGame *game)
{
    return s_phone_tim_ready && s_plm_phone_size > 0 && game->town.stage == SH_TOWN_SCRIPT &&
        game->town.script == SH_TOWN_SCR_SCHOOL && game->town.school.overlay == SH_MAP_IDX_MAP1_S02 &&
        game->town.school.callback == 14 && game->town.school.phone.held && !game->town.school.phone.hidden;
}

/* Packed anim status → keyframe clip (ANIM_STATUS = animIdx<<1 | active).
 * m0s00: HARRY_M0S00_ANIM_INFOS (alley intro). Else HARRY_M0S01 (cafe). */
static bool harry_status_to_clip(int status, int *start, int *end, float *fps, bool *loop, bool *use_map,
                                 bool m0s00)
{
    const int anim_idx = status >> 1;
    *use_map = false;
    *loop = false;
    *fps = 15.0f;
    /* Map overlays (animIdx >= 38). */
    if (anim_idx >= 38) {
        *use_map = true;
        if (m0s00) {
            /* HARRY_M0S00_ANIM_INFOS — ends at anim 54 / kf 1026. */
            switch (anim_idx) {
            case 38:
                *start = 676;
                *end = 677;
                *fps = 10.0f;
                break;
            case 39:
                *start = 678;
                *end = 713;
                *fps = 10.0f;
                break;
            case 40:
                *start = 718;
                *end = 732;
                break;
            case 41:
                *start = 737;
                *end = 751;
                break;
            case 42:
                *start = 758;
                *end = 770;
                break;
            case 43:
                *start = 777;
                *end = 789;
                break;
            case 44:
                *start = 790;
                *end = 807;
                *fps = 18.0f;
                *loop = true;
                break;
            case 45:
                *start = 808;
                *end = 825;
                *fps = 18.0f;
                *loop = true;
                break;
            case 46:
                *start = 826;
                *end = 843;
                *fps = 18.0f;
                break;
            case 47:
                *start = 844;
                *end = 861;
                *fps = 18.0f;
                break;
            case 48:
                *start = 862;
                *end = 882;
                break;
            case 49:
                *start = 883;
                *end = 903;
                break;
            case 50:
                *start = 904;
                *end = 914;
                break;
            case 51:
                *start = 915;
                *end = 925;
                break;
            case 52:
                *start = 926;
                *end = 972;
                *fps = 4.0f;
                break;
            case 53:
                *start = 973;
                *end = 993;
                break;
            case 54:
                *start = 994;
                *end = 1026;
                *fps = 25.0f;
                break;
            default:
                *start = 973;
                *end = 993;
                break;
            }
            return true;
        }
        /* HARRY_M0S01_ANIM_INFOS */
        switch (anim_idx) {
        case 38:
            *start = 676;
            *end = 677;
            *fps = 10.0f;
            break;
        case 39:
            *start = 678;
            *end = 713;
            *fps = 10.0f;
            break;
        case 40:
            *start = 718;
            *end = 732;
            break;
        case 41:
            *start = 737;
            *end = 751;
            break;
        case 42:
            *start = 758;
            *end = 770;
            break;
        case 43:
            *start = 777;
            *end = 789;
            break;
        case 44:
            *start = 790;
            *end = 804;
            break;
        case 45:
            *start = 805;
            *end = 815;
            *fps = 14.0f;
            break;
        case 46:
            *start = 816;
            *end = 826;
            *fps = 18.0f;
            break;
        case 47:
            *start = 827;
            *end = 872;
            *fps = 9.0f;
            break;
        case 48:
            *start = 873;
            *end = 900;
            *fps = 8.0f;
            break;
        case 49:
            *start = 901;
            *end = 916;
            *fps = 4.0f;
            break;
        case 50:
            *start = 917;
            *end = 947;
            break;
        case 51:
            *start = 948;
            *end = 1026;
            *fps = 18.0f;
            break;
        case 52:
            *start = 1027;
            *end = 1037;
            break;
        case 53:
            *start = 1038;
            *end = 1054;
            break;
        case 54:
            *start = 1055;
            *end = 1074;
            *fps = 10.0f;
            break;
        case 55:
            *start = 1075;
            *end = 1080;
            *fps = 8.0f;
            *loop = true;
            break;
        case 56:
            *start = 1081;
            *end = 1084;
            *fps = 2.0f;
            *loop = true;
            break;
        case 57:
            *start = 815;
            *end = 815;
            *fps = 15.0f;
            break;
        default:
            *start = 676;
            *end = 677;
            *fps = 10.0f;
            break;
        }
        return true;
    }
    /* HARRY_BASE_ANIM_INFOS (locomotion / idle). */
    switch (anim_idx) {
    case 2:
        *start = 1;
        *end = 23;
        *fps = 14.0f;
        *loop = true;
        break; /* walk */
    case 3:
        *start = 26;
        *end = 45;
        *fps = 18.0f;
        *loop = true;
        break; /* run */
    case 17:
        *start = 248;
        *end = 320;
        *fps = 7.0f;
        *loop = true;
        break; /* look around */
    case 25:
        *start = 485;
        *end = 502;
        *fps = 20.0f;
        break; /* stomp */
    case 26:
        *start = 503;
        *end = 542;
        *fps = 10.0f;
        *loop = true;
        break; /* idle */
    case 27:
        *start = 543;
        *end = 567;
        *fps = 10.0f;
        *loop = true;
        break; /* exhausted */
    default:
        *start = 503;
        *end = 542;
        *fps = 10.0f;
        *loop = true;
        break;
    }
    return true;
}

static bool bake_harry_frame(ShGame *game, int keyframe, bool use_wep)
{
    size_t count = 0;
    ShCharaClutMap clut_map = s_harry_cluts; /* keep stable atlas slots */
    const unsigned flags = harry_hand_flags(game);
    const bool gun = game->save.equipped_weapon == SH_ITEM_HANDGUN;
    const bool knife = game->save.equipped_weapon == SH_ITEM_POCKET_KNIFE;
    const uint8_t *wep_kf = NULL;
    size_t wep_sz = 0;
    int wep_base = SH_CHARA_WEP_KF_BASE;
    const uint8_t *held_plm = NULL;
    size_t held_sz = 0;
    const bool phone_scene = game->town.stage == SH_TOWN_SCRIPT && game->town.script == SH_TOWN_SCR_SCHOOL &&
        game->town.school.overlay == SH_MAP_IDX_MAP1_S02 && game->town.school.callback == 14;
    const bool phone = harry_phone_visible(game);
    if (keyframe >= SH_CHARA_MAP_KF_BASE && s_harry_map_anm_size > 0) {
        /* Cutscene overlay (HB_M0Sxx) — same headerless layout as weapons. */
        wep_kf = s_harry_map_anm;
        wep_sz = s_harry_map_anm_size;
        wep_base = SH_CHARA_MAP_KF_BASE;
    } else if (use_wep && gun && s_harry_wep_gun_size > 0) {
        wep_kf = s_harry_wep_gun;
        wep_sz = s_harry_wep_gun_size;
        /* WorldGfx_HeldItemDraw: HANDGUN.PLM on RightHand, textured from HERO. */
        held_plm = s_plm_handgun_size > 0 ? s_plm_handgun : NULL;
        held_sz = s_plm_handgun_size;
    } else if (use_wep && knife && s_harry_wep_knife_size > 0) {
        wep_kf = s_harry_wep_knife;
        wep_sz = s_harry_wep_knife_size;
        held_plm = s_plm_knife_size > 0 ? s_plm_knife : NULL;
        held_sz = s_plm_knife_size;
    }

    if (phone) { held_plm = s_plm_phone; held_sz = s_plm_phone_size; }
    if (phone_scene && game->town.school.phone.hidden) { held_plm = NULL; held_sz = 0; }

    if (keyframe < 0) {
        keyframe = 0;
    }

    if (!sh_chara_bake(s_harry_bake, SH_CHARA_MAX_VERTS, &count, s_harry_ilm, s_harry_ilm_size, s_harry_anm,
                       s_harry_anm_size, s_harry_have_tim ? &s_harry_tim_parsed : NULL, keyframe,
                       s_harry_have_tim ? &clut_map : NULL, flags, wep_kf, wep_sz, wep_base,
                       phone ? &s_phone_tim : NULL, held_plm, held_sz,
                       phone ? sh_phone_held_bone() : SH_HARRY_HELD_BONE)) {
        /* Keep the previous mesh — a failed weapon bake must not blank Harry mid-fight. */
        return false;
    }
    s_harry_light_bone_valid = sh_chara_last_bone((unsigned)sh_env_flashlight_bone(), &s_harry_light_bone);
    s_harry_hand_bone_valid = sh_chara_last_bone(sh_drain_valve_hand_bone(), &s_harry_hand_bone);
    if (s_harry_have_tim && s_harry_cluts.count > 0 && !s_harry_tim_uploaded) {
        s_harry_tim_uploaded = sh_gfx_set_player_clut_atlas(&s_harry_tim_parsed, s_phone_tim_ready ? &s_phone_tim : NULL, &s_harry_cluts);
    }
    if (!sh_gfx_set_player_mesh(s_harry_bake, count)) {
        return false;
    }
    game->harry_anim_kf = keyframe;
    s_harry_hand_flags = (int)flags;
    s_harry_using_wep = use_wep && wep_kf != NULL;
    s_harry_phone_visible = phone;
    game->harry_ready = true;
    return true;
}

static void update_harry_anim(ShGame *game, float move_amount, bool running, float dt)
{
    if (!game->harry_ready || s_harry_anm_size == 0 || dt <= 0.0f) {
        return;
    }

    /* Cutscene scripted poses (CharaAnimCmd_SetState) override locomotion. */
    {
        int keyframe;
        if (sh_alley_harry_keyframe(&game->alley, &keyframe) || sh_town_harry_keyframe(&game->town, &keyframe)) {
            if (keyframe != game->harry_anim_kf || s_harry_using_wep ||
                harry_hand_flags(game) != (unsigned)s_harry_hand_flags || harry_phone_visible(game) != s_harry_phone_visible) {
                bake_harry_frame(game, keyframe, false);
            }
            return;
        }
    }
    {
        int st = 0;
        float tm = 0.0f;
        const bool from_alley = sh_alley_harry_cutscene_anim(&game->alley, &st, &tm);
        const bool from_cafe = !from_alley && sh_cafe_harry_cutscene_anim(&game->cafe, &st, &tm);
        if (from_alley || from_cafe) {
            int start = 503, end = 542;
            float fps = 10.0f;
            bool loop = true, use_map = false;
            harry_status_to_clip(st, &start, &end, &fps, &loop, &use_map, from_alley);
            (void)use_map;
            const float span = (float)(end - start + 1);
            float t = tm * fps;
            if (loop) {
                while (t >= span) {
                    t -= span;
                }
            } else if (t > span - 1.0f) {
                t = span - 1.0f;
            }
            const int kf = start + (int)t;
            if (kf != game->harry_anim_kf || s_harry_using_wep) {
                bake_harry_frame(game, kf, false);
            }
            return;
        }
    }

    const bool aiming = sh_cafe_aiming(&game->cafe) || sh_town_aiming(&game->town);
    const bool gun = game->save.equipped_weapon == SH_ITEM_HANDGUN;
    const bool knife = game->save.equipped_weapon == SH_ITEM_POCKET_KNIFE;
    const bool firing = game->cafe.fire_anim > 0.0f || game->town.fire_anim > 0.0f;
    /* Never bake weapon overlay during bird death scripts. */
    const bool combat_ok = game->cafe.stage != SH_CAFE_BIRD_DEATH && game->cafe.bird_state != SH_BIRD_DYING &&
                           game->cafe.bird_state != SH_BIRD_DEAD;
    const bool have_wep = (gun && s_harry_wep_gun_size > 0) || (knife && s_harry_wep_knife_size > 0);
    const bool use_wep = combat_ok && have_wep && (gun || knife) && (aiming || firing);
    int start, end;
    float rate;

    if (use_wep) {
        if (firing) {
            /* Handgun Unk29 = 582..592 only. Never enter reload/pump 605+. */
            start = gun ? SH_KF_GUN_FIRE_START : SH_KF_KNIFE_FIRE_START;
            end = gun ? SH_KF_GUN_FIRE_END : SH_KF_KNIFE_FIRE_END;
            rate = SH_FIRE_KF_RATE;
            if (game->harry_anim_time < (float)start || game->harry_anim_time > (float)end) {
                game->harry_anim_time = (float)start;
            }
            game->harry_anim_time += rate * dt;
            if (game->harry_anim_time > (float)end) {
                game->harry_anim_time = (float)end;
            }
        } else {
            start = gun ? SH_KF_GUN_AIM_START : SH_KF_KNIFE_AIM_START;
            end = gun ? SH_KF_GUN_AIM_END : SH_KF_KNIFE_AIM_END;
            rate = SH_AIM_KF_RATE;
            if (!s_harry_using_wep || game->harry_anim_time < (float)start ||
                game->harry_anim_time > (float)end) {
                game->harry_anim_time = (float)end;
            }
            game->harry_anim_time = (float)end; /* hold aim pose */
        }
    } else {
        const float mag = fabsf(move_amount);
        const bool moving = mag > 0.12f;
        const bool use_run = moving && running;
        start = !moving ? SH_CHARA_KF_IDLE_START : (use_run ? SH_CHARA_KF_RUN_START : SH_CHARA_KF_WALK_START);
        end = !moving ? SH_CHARA_KF_IDLE_END : (use_run ? SH_CHARA_KF_RUN_END : SH_CHARA_KF_WALK_END);
        rate = !moving ? SH_IDLE_KF_RATE : (use_run ? SH_RUN_KF_RATE : SH_WALK_KF_RATE);
        const float span = (float)(end - start + 1);
        const float speed = moving ? (mag < 1.0f ? mag : 1.0f) : 1.0f;

        if (s_harry_using_wep || game->harry_anim_time < (float)start ||
            game->harry_anim_time >= (float)(end + 1)) {
            game->harry_anim_time = (float)start;
        }
        const float prev = game->harry_anim_time;
        game->harry_anim_time += rate * speed * dt;
        bool wrapped = false;
        while (game->harry_anim_time >= (float)(end + 1)) {
            game->harry_anim_time -= span;
            wrapped = true;
        }
        if (moving) {
            /* Player_FootstepSfxPlay: WalkForward kf 6/18, RunForward kf 31/41 (Player_FootstepSfxIdGet pitches). */
            const int k0 = use_run ? 31 : 6;
            const int k1 = use_run ? 41 : 18;
            const float now = game->harry_anim_time;
            for (int i = 0; i < 2; i++) {
                const float k = (float)(i == 0 ? k0 : k1);
                const bool crossed = wrapped ? (prev < k || now >= k) : (prev < k && now >= k);
                if (crossed) {
                    const int pitch = use_run ? (rand() % 64) + 32 : (rand() % 32) - 16;
                    sh_snd_at_pitch(SH_SFX_FOOTSTEP_CONCRETE, game->player_pos, use_run ? 0.5f : 0.25f, pitch);
                }
            }
        }
    }

    const int kf = (int)game->harry_anim_time;
    const int hands = (int)harry_hand_flags(game);
    if (kf != game->harry_anim_kf || hands != s_harry_hand_flags || use_wep != s_harry_using_wep) {
        if (!bake_harry_frame(game, kf, use_wep) && use_wep) {
            bake_harry_frame(game, SH_CHARA_KF_IDLE_START, false);
            game->harry_anim_time = (float)SH_CHARA_KF_IDLE_START;
        }
    }
}

/* Game_FlashlightAttributesFix / Gfx_FlashlightPositionUpdate, using the current Harry torso pose. */
static bool environment_light_from_harry(const ShGame *game, ShEnvLight *light)
{
    if (game->save.map_idx == SH_MAP_IDX_MAP1_S03 && game->town.school.drain_light_override) {
        const ShDrainLight *attributes = &game->town.school.drain_light;
        const int32_t position[3] = {attributes->position.vx, attributes->position.vy, attributes->position.vz};
        int32_t direction[3];
        sh_drain_light_direction(attributes, direction);
        sh_env_light_build(light, game->town.environment.current.preset.spot_light, position, direction);
        return true;
    }
    if (!s_harry_light_bone_valid) {
        return false;
    }
    int32_t offset[3];
    int32_t direction[3];
    int32_t local_position[3];
    int32_t local_direction[3];
    sh_env_flashlight_local(offset, direction);
    for (int axis = 0; axis < 3; axis++) {
        int64_t position_sum = 0;
        int64_t direction_sum = 0;
        for (int column = 0; column < 3; column++) {
            position_sum += (int64_t)s_harry_light_bone.rotation[axis][column] * offset[column];
            direction_sum += (int64_t)s_harry_light_bone.rotation[axis][column] * direction[column];
        }
        local_position[axis] = (int32_t)(position_sum >> 12) + s_harry_light_bone.translation[axis] * 16;
        local_direction[axis] = (int32_t)(direction_sum >> 12);
    }
    const int32_t yaw = SH_Q12(-game->player_yaw / (2.0f * 3.14159265358979323846f));
    const int32_t sine = sh_fixed_sin(yaw);
    const int32_t cosine = sh_fixed_cos(yaw);
    int32_t position[3];
    position[0] = SH_Q12(game->player_pos[0]) + (int32_t)(((int64_t)cosine * local_position[0] + (int64_t)sine * local_position[2]) >> 12);
    position[1] = -SH_Q12(game->player_pos[1]) + local_position[1];
    position[2] = -SH_Q12(game->player_pos[2]) + (int32_t)((-(int64_t)sine * local_position[0] + (int64_t)cosine * local_position[2]) >> 12);
    direction[0] = (int32_t)(((int64_t)cosine * local_direction[0] + (int64_t)sine * local_direction[2]) >> 12);
    direction[1] = local_direction[1];
    direction[2] = (int32_t)((-(int64_t)sine * local_direction[0] + (int64_t)cosine * local_direction[2]) >> 12);
    sh_env_light_build(light, game->town.environment.current.preset.spot_light, position, direction);
    return true;
}

static bool load_harry(ShGame *game)
{
    s_harry_hand_bone_valid = false;
    game->harry_ready = false;
    game->harry_anim_time = (float)SH_CHARA_KF_IDLE_START;
    game->harry_anim_kf = -1;
    s_harry_ilm_size = 0;
    s_harry_anm_size = 0;
    s_harry_wep_gun_size = 0;
    s_harry_wep_knife_size = 0;
    s_harry_map_anm_size = 0;
    s_harry_have_tim = false;
    s_phone_tim_ready = false;
    s_harry_tim_uploaded = false;
    s_harry_base_kf_count = 0;
    s_harry_hand_flags = SH_CHARA_F_HARRY_HANDS;
    s_harry_using_wep = false;
    s_harry_phone_visible = false;
    sh_chara_clut_map_init(&s_harry_cluts, false);

    size_t tim_size = 0;
    if (!read_file_into(SH_CHARA_DIR "/HERO.ILM", s_harry_ilm, sizeof(s_harry_ilm), &s_harry_ilm_size) ||
        !read_file_into(SH_ANIM_DIR "/HB_BASE.ANM", s_harry_anm, sizeof(s_harry_anm), &s_harry_anm_size)) {
        game->harry_ready = sh_gfx_set_player_marker();
        return game->harry_ready;
    }
    s_harry_base_kf_count = sh_chara_anm_keyframe_count(s_harry_anm, s_harry_anm_size);

    read_file_into(SH_ANIM_DIR "/HB_WEP4.ANM", s_harry_wep_gun, sizeof(s_harry_wep_gun), &s_harry_wep_gun_size);
    read_file_into(SH_ANIM_DIR "/HB_WEP3.ANM", s_harry_wep_knife, sizeof(s_harry_wep_knife),
                   &s_harry_wep_knife_size);
    read_file_into("romfs:/game/item/HANDGUN.PLM", s_plm_handgun, sizeof(s_plm_handgun), &s_plm_handgun_size);
    read_file_into("romfs:/game/item/KNIFE.PLM", s_plm_knife, sizeof(s_plm_knife), &s_plm_knife_size);
    read_file_into("romfs:/game/item/PHONE.PLM", s_plm_phone, sizeof(s_plm_phone), &s_plm_phone_size);
    size_t phone_tim_size = 0;
    s_phone_tim_ready = read_file_into("romfs:/game/item/PHONE.TIM", s_phone_tim_file, sizeof(s_phone_tim_file), &phone_tim_size) &&
        sh_tim_parse(s_phone_tim_file, phone_tim_size, &s_phone_tim);
    read_file_into("romfs:/game/item/PIPE.PLM", s_plm_pipe, sizeof(s_plm_pipe), &s_plm_pipe_size);

    s_harry_have_tim = read_file_into(SH_CHARA_DIR "/HERO.TIM", s_harry_tim, sizeof(s_harry_tim), &tim_size) &&
                       sh_tim_parse(s_harry_tim, tim_size, &s_harry_tim_parsed);
    if (s_harry_have_tim) {
        sh_chara_collect_cluts(s_harry_ilm, s_harry_ilm_size, &s_harry_cluts, 0);
        if (s_plm_handgun_size > 0) {
            sh_chara_collect_cluts(s_plm_handgun, s_plm_handgun_size, &s_harry_cluts, 0);
        }
        if (s_plm_knife_size > 0) {
            sh_chara_collect_cluts(s_plm_knife, s_plm_knife_size, &s_harry_cluts, 0);
        }
        if (s_phone_tim_ready && s_plm_phone_size > 0) {
            sh_chara_collect_cluts(s_plm_phone, s_plm_phone_size, &s_harry_cluts, 1);
        }
    }

    size_t map_sz = 0;
    s_map_tim_ok = read_file_into(SH_TEXTURE_DIR "/MP_0TOWN.TIM", s_map_tim_file, sizeof(s_map_tim_file), &map_sz) &&
                   sh_tim_parse(s_map_tim_file, map_sz, &s_map_tim_parsed);
    size_t marks_sz = 0;
    if (read_file_into(SH_TEXTURE_DIR "/MR_0TOWN.TIM", s_marks_tim_file, sizeof(s_marks_tim_file), &marks_sz) &&
        sh_tim_parse(s_marks_tim_file, marks_sz, &s_marks_tim_parsed)) {
        sh_gfx_set_map_marks_atlas(&s_marks_tim_parsed);
    }

    if (!bake_harry_frame(game, SH_CHARA_KF_IDLE_START, false)) {
        game->harry_ready = sh_gfx_set_player_marker();
        return game->harry_ready;
    }
    return true;
}

/* ---- NPC actors (Cybil, Air Screamer) ------------------------------------ */

#define SH_NPC_ILM_MAX (20u * 1024u)
#define SH_NPC_ANM_MAX (80u * 1024u)
#define SH_NPC_TIM_MAX (20u * 1024u)

typedef struct ShNpcAsset {
    uint8_t ilm[SH_NPC_ILM_MAX];
    uint8_t anm[SH_NPC_ANM_MAX];
    uint8_t tim[SH_NPC_TIM_MAX];
    size_t ilm_size;
    size_t anm_size;
    ShTim tim_parsed;
    bool have_tim;
    bool tim_uploaded;
    bool ready;
    int kf_count;
    int kf;
    int kf_second;
    int32_t pose_alpha;
    bool split_head;
    int variant;
    int16_t twist;
    ShCharaBone model_bones[SH_CHARA_MAX_BONES];
    unsigned bone_count;
} ShNpcAsset;

static ShNpcAsset s_npcs[SH_GFX_MAX_ACTORS];
static ShMapVertex s_npc_bake[SH_CHARA_MAX_VERTS];

static bool bake_npc_pose_blend(int slot, int keyframe, int variant, int16_t twist, const ShAnimPoseTiming *pose)
{
    ShNpcAsset *npc = &s_npcs[slot];
    if (!npc->ready) {
        return false;
    }
    if (pose && (pose->first < 0 || pose->first >= npc->kf_count || pose->second < 0 ||
        pose->second >= npc->kf_count || pose->alpha < 0 || pose->alpha > 4096)) { return false; }
    if (keyframe < 0) {
        keyframe = 0;
    }
    if (npc->kf_count > 0 && keyframe >= npc->kf_count) {
        keyframe = npc->kf_count - 1;
    }
    const int second = pose ? pose->second : keyframe;
    const int32_t alpha = pose ? pose->alpha : 0;
    if (keyframe == npc->kf && second == npc->kf_second && alpha == npc->pose_alpha &&
        variant == npc->variant && twist == npc->twist) {
        return true;
    }
    size_t count = 0;
    ShCharaClutMap clut_map;
    sh_chara_clut_map_init(&clut_map, true);
    bool baked;
    if (npc->split_head && pose) {
        baked = sh_chara_bake_split_head_blend(s_npc_bake, SH_CHARA_MAX_VERTS, &count, npc->ilm, npc->ilm_size,
            npc->anm, npc->anm_size, npc->have_tim ? &npc->tim_parsed : NULL, pose,
            npc->have_tim ? &clut_map : NULL, variant, twist);
    } else if (npc->split_head) {
        baked = sh_chara_bake_split_head(s_npc_bake, SH_CHARA_MAX_VERTS, &count, npc->ilm, npc->ilm_size,
            npc->anm, npc->anm_size, npc->have_tim ? &npc->tim_parsed : NULL, keyframe,
            npc->have_tim ? &clut_map : NULL, variant, twist);
    } else {
        baked = sh_chara_bake(s_npc_bake, SH_CHARA_MAX_VERTS, &count, npc->ilm, npc->ilm_size, npc->anm, npc->anm_size,
                       npc->have_tim ? &npc->tim_parsed : NULL, keyframe, npc->have_tim ? &clut_map : NULL,
                       SH_CHARA_F_NONE, NULL, 0, SH_CHARA_WEP_KF_BASE, NULL, NULL, 0, -1);
    }
    if (!baked) {
        return false;
    }
    if (npc->have_tim && clut_map.count > 0 && !npc->tim_uploaded) {
        npc->tim_uploaded = sh_gfx_actor_set_tim(slot, &npc->tim_parsed, clut_map.delta, clut_map.count);
    }
    if (!sh_gfx_actor_set_mesh(slot, s_npc_bake, count)) {
        return false;
    }
    npc->kf = keyframe;
    npc->kf_second = second;
    npc->pose_alpha = alpha;
    npc->variant = variant;
    npc->twist = twist;
    npc->bone_count = 0;
    if (npc->split_head) {
        while (npc->bone_count < SH_CHARA_MAX_BONES &&
            sh_chara_last_bone(npc->bone_count, &npc->model_bones[npc->bone_count])) {
            npc->bone_count++;
        }
    }
    return true;
}

static bool bake_npc_pose(int slot, int keyframe, int variant, int16_t twist)
{
    return bake_npc_pose_blend(slot, keyframe, variant, twist, NULL);
}

static bool bake_npc_frame(int slot, int keyframe)
{
    return bake_npc_pose(slot, keyframe, s_npcs[slot].variant, s_npcs[slot].twist);
}

static bool load_npc_pose(int slot, const char *ilm_path, const char *anm_path, const char *tim_path,
    bool split_head, int keyframe, int variant, int16_t twist)
{
    ShNpcAsset *npc = &s_npcs[slot];
    size_t tim_size = 0;
    npc->ready = false;
    npc->kf = -1;
    npc->kf_second = -1;
    npc->pose_alpha = 0;
    npc->tim_uploaded = false;
    npc->split_head = split_head;
    npc->variant = 0;
    npc->twist = 0;
    npc->bone_count = 0;
    if (!read_file_into(ilm_path, npc->ilm, sizeof(npc->ilm), &npc->ilm_size) ||
        !read_file_into(anm_path, npc->anm, sizeof(npc->anm), &npc->anm_size)) {
        return false;
    }
    npc->have_tim = read_file_into(tim_path, npc->tim, sizeof(npc->tim), &tim_size) &&
                    sh_tim_parse(npc->tim, tim_size, &npc->tim_parsed);
    npc->kf_count = sh_chara_anm_keyframe_count(npc->anm, npc->anm_size);
    npc->ready = true;
    if (!bake_npc_pose(slot, keyframe, variant, twist)) {
        npc->ready = false;
    }
    return npc->ready;
}

static bool load_npc(int slot, const char *ilm_path, const char *anm_path, const char *tim_path)
{
    return load_npc_pose(slot, ilm_path, anm_path, tim_path, false, 0, 0, 0);
}

bool sh_npc_load_split_head(int slot, int keyframe, int variant, int16_t twist)
{
    if (slot < 0 || slot >= SH_GFX_MAX_ACTORS || (variant != 1 && variant != 2)) { return false; }
    return load_npc_pose(slot, SH_CHARA_DIR "/FAT.ILM", SH_ANIM_DIR "/FAT.ANM", SH_CHARA_DIR "/FAT.TIM",
        true, keyframe, variant, twist);
}

bool sh_npc_pose_split_head(int slot, int keyframe, int variant, int16_t twist,
    const ShCharaBone **model_bones, unsigned *bone_count)
{
    if (slot < 0 || slot >= SH_GFX_MAX_ACTORS || !model_bones || !bone_count ||
        !s_npcs[slot].split_head || (variant != 1 && variant != 2) ||
        !bake_npc_pose(slot, keyframe, variant, twist)) { return false; }
    *model_bones = s_npcs[slot].model_bones;
    *bone_count = s_npcs[slot].bone_count;
    return true;
}

bool sh_npc_pose_split_head_blend(int slot, const ShAnimPoseTiming *pose, int variant, int16_t twist,
    const ShCharaBone **model_bones, unsigned *bone_count)
{
    if (slot < 0 || slot >= SH_GFX_MAX_ACTORS || !pose || !model_bones || !bone_count ||
        !s_npcs[slot].split_head || (variant != 1 && variant != 2) ||
        !bake_npc_pose_blend(slot, pose->first, variant, twist, pose)) { return false; }
    *model_bones = s_npcs[slot].model_bones;
    *bone_count = s_npcs[slot].bone_count;
    return true;
}

static void load_harry_map_anm(const char *path)
{
    s_harry_map_anm_size = 0;
    read_file_into(path, s_harry_map_anm, sizeof(s_harry_map_anm), &s_harry_map_anm_size);
}

static bool load_smoke_atlas(void)
{
    static uint8_t data[SH_SMOKE_TIM_SIZE];
    size_t size = 0;
    ShTim tim;
    return read_file_into(SH_TEXTURE_DIR "/BLD.TIM", data, sizeof(data), &size) &&
           sh_tim_parse(data, size, &tim) && sh_gfx_set_smoke_atlas(&tim);
}

static bool load_water_atlas(void)
{
    static uint8_t data[SH_WATER_TIM_SIZE];
    size_t size = 0;
    ShTim tim;
    return read_file_into(SH_TEXTURE_DIR "/WATER.TIM", data, sizeof(data), &size) &&
        sh_tim_parse(data, size, &tim) && sh_gfx_set_water_atlas(&tim);
}

static bool load_piano_cursor(void)
{
    static uint8_t data[SH_PIANO_CURSOR_TIM_SIZE];
    size_t size = 0;
    ShTim tim;
    return read_file_into(SH_TEXTURE_DIR "/BG_ETC.TIM", data, sizeof(data), &size) &&
           sh_tim_parse(data, size, &tim) && sh_gfx_set_piano_cursor(&tim);
}

static void load_npcs_cafe(void)
{
    load_npc(SH_CAFE_ACTOR_CYBIL, SH_CHARA_DIR "/SIBYL.ILM", SH_ANIM_DIR "/SBL.ANM", SH_CHARA_DIR "/SIBYL.TIM");
    load_npc(SH_CAFE_ACTOR_BIRD, SH_CHARA_DIR "/BIRD.ILM", SH_ANIM_DIR "/BIRD.ANM", SH_CHARA_DIR "/REBIRD.TIM");
    load_harry_map_anm(SH_ANIM_DIR "/HB_M0S01.ANM");
}

static void load_npcs_alley(void)
{
    /* Cheryl (SRL.*) on slot 0; Grey Children (CAT.*) load later via take_grey_assets. */
    load_npc(0, SH_CHARA_DIR "/SRL.ILM", SH_ANIM_DIR "/SRL.ANM", SH_CHARA_DIR "/SRL.TIM");
    s_npcs[1].ready = false;
    s_npcs[2].ready = false;
    load_harry_map_anm(SH_ANIM_DIR "/HB_M0S00.ANM");
}

static void load_npcs_alley_grey(void)
{
    /* MapEvent_GreyChildrenSpawn — CAT.* on slots 0..2. */
    load_npc(0, SH_CHARA_DIR "/CAT.ILM", SH_ANIM_DIR "/CAT.ANM", SH_CHARA_DIR "/CAT.TIM");
    load_npc(1, SH_CHARA_DIR "/CAT.ILM", SH_ANIM_DIR "/CAT.ANM", SH_CHARA_DIR "/CAT.TIM");
    load_npc(2, SH_CHARA_DIR "/CAT.ILM", SH_ANIM_DIR "/CAT.ANM", SH_CHARA_DIR "/CAT.TIM");
}

void sh_npc_load_slot(int slot, bool air_screamer)
{
    if (slot < 0 || slot >= SH_GFX_MAX_ACTORS) {
        return;
    }
    if (air_screamer) {
        load_npc(slot, SH_CHARA_DIR "/BIRD.ILM", SH_ANIM_DIR "/BIRD.ANM", SH_CHARA_DIR "/REBIRD.TIM");
    } else {
        load_npc(slot, SH_CHARA_DIR "/DOG.ILM", SH_ANIM_DIR "/DOG.ANM", SH_CHARA_DIR "/DOG.TIM");
    }
}

static void load_npcs_town(void)
{
    /* Slot 0 Air Screamer, slot 1 Groaner (Game_NpcRoomInitSpawn groups). */
    sh_npc_load_slot(0, true);
    sh_npc_load_slot(1, false);
    load_harry_map_anm(SH_ANIM_DIR "/HB_M2S00.ANM");
}

static void load_npcs(void)
{
    load_npcs_cafe();
}

/* ---- Pickup close-up (Gfx_PickupItemAnimate, ITEM/UNQxx.TMD + TIM00) ---- */

#define SH_ITEM_TMD_MAX (8u * 1024u)
#define SH_ITEM_TIM_MAX (34u * 1024u)

typedef struct ShPickupAsset {
    int item_id;
    const char *path;
    float rot_x_deg, rot_z_deg; /* INV_ITEM_ROTATIONS[item_id - 32] */
    uint8_t tmd[SH_ITEM_TMD_MAX];
    size_t tmd_size;
    ShItemModel model;
} ShPickupAsset;

static ShPickupAsset s_pickups[] = {
    {0x80, "romfs:/game/item/UNQ80.TMD", 45.0f, 0.0f},
    {0xE0, "romfs:/game/item/UNQE0.TMD", 45.0f, 0.0f},
    {0xE1, "romfs:/game/item/UNQE1.TMD", 225.0f, 180.0f},
    {0x20, "romfs:/game/item/UNQ20.TMD", 45.0f, 0.0f}, /* Health drink */
    {0x21, "romfs:/game/item/UNQ21.TMD", 45.0f, 0.0f}, /* First aid */
    {0x22, "romfs:/game/item/UNQ22.TMD", 45.0f, 0.0f}, /* Ampoule */
    {0xC0, "romfs:/game/item/UNQC0.TMD", 45.0f, 0.0f}, /* Handgun bullets */
    {0xC1, "romfs:/game/item/UNQC1.TMD", 45.0f, 0.0f}, /* Rifle shells */
    {0xC2, "romfs:/game/item/UNQC2.TMD", 45.0f, 0.0f}, /* Shotgun shells */
    {0x81, "romfs:/game/item/UNQ81.TMD", 45.0f, 0.0f}, /* Steel pipe */
    {0x41, "romfs:/game/item/UNQ41.TMD", 45.0f, 0.0f}, /* House key */
    {0x53, "romfs:/game/item/UNQ83.TMD", 45.0f, 0.0f}, /* Note to school */
    {0x54, "romfs:/game/item/UNQ84.TMD", 45.0f, 0.0f}, /* Doghouse note */
#define SH_TOWN_KEY_ASSETS
#include "town_keys.h"
#undef SH_TOWN_KEY_ASSETS
#define SH_TOWN_STORY_ASSETS
#include "town_story_data.h"
#undef SH_TOWN_STORY_ASSETS
#define SH_SCHOOL_UPPER_ASSETS
#include "school_upper_data.h"
#undef SH_SCHOOL_UPPER_ASSETS
#define SH_SCHOOL_ALT_ASSETS
#include "school_alternative_data.h"
#undef SH_SCHOOL_ALT_ASSETS
#define SH_SCHOOL_ROOF_PICKUP_ASSETS
#include "school_roof_pickup_data.h"
#undef SH_SCHOOL_ROOF_PICKUP_ASSETS
};
#define SH_PICKUP_ITEMS ((int)(sizeof(s_pickups) / sizeof(s_pickups[0])))
static uint8_t s_item_tim_file[SH_ITEM_TIM_MAX];
static ShTim s_item_tim;
static bool s_item_tim_ok;
static int s_item_uploaded = -1;
static ShMapVertex s_item_bake[SH_ITEM_MAX_VERTS];

static void load_pickup_items(void)
{
    size_t tim_size = 0;
    s_item_tim_ok = read_file_into("romfs:/game/item/TIM00.TIM", s_item_tim_file, sizeof(s_item_tim_file),
                                   &tim_size) &&
                    sh_tim_parse(s_item_tim_file, tim_size, &s_item_tim);
    s_item_uploaded = -1;
    for (int i = 0; i < SH_PICKUP_ITEMS; i++) {
        ShPickupAsset *a = &s_pickups[i];
        a->tmd_size = 0;
        memset(&a->model, 0, sizeof(a->model));
        if (s_item_tim_ok && read_file_into(a->path, a->tmd, sizeof(a->tmd), &a->tmd_size)) {
            sh_item_model_init(&a->model, a->tmd, a->tmd_size, s_item_tim_file, tim_size);
        }
    }
}

static void refresh_pickup_item(const ShGame *game, const ShCamera *camera)
{
    int id = 0;
    float scale = 0.0f, spin = 0.0f;
    if (!camera ||
        (!sh_cafe_pickup_item(&game->cafe, &id, &scale, &spin) &&
         !sh_town_pickup_item(&game->town, &id, &scale, &spin))) {
        sh_gfx_item_set_mesh(NULL, 0);
        return;
    }
    ShPickupAsset *a = NULL;
    for (int i = 0; i < SH_PICKUP_ITEMS; i++) {
        if (s_pickups[i].item_id == id && s_pickups[i].model.ok) {
            a = &s_pickups[i];
        }
    }
    if (!a) {
        sh_gfx_item_set_mesh(NULL, 0);
        return;
    }
    if (s_item_uploaded != id) {
        if (!sh_gfx_item_set_tim(&s_item_tim, a->model.clut_delta)) {
            sh_gfx_item_set_mesh(NULL, 0);
            return;
        }
        s_item_uploaded = id;
    }
    const float deg = 3.14159265f / 180.0f;
    size_t count = 0;
    if (sh_item_model_bake(&a->model, s_item_bake, SH_ITEM_MAX_VERTS, &count, a->rot_x_deg * deg, spin,
                           a->rot_z_deg * deg, scale, camera->fov_deg)) {
        sh_gfx_item_set_mesh(s_item_bake, count);
    } else {
        sh_gfx_item_set_mesh(NULL, 0);
    }
}

static void update_npc_anims(const ShGame *game)
{
    if (sh_alley_active(&game->alley)) {
        for (int slot = 0; slot < SH_GFX_MAX_ACTORS; slot++) {
            ShActorPose pose;
            if (!s_npcs[slot].ready || !sh_alley_actor_pose(&game->alley, slot, &pose)) {
                continue;
            }
            bake_npc_frame(slot, sh_alley_actor_keyframe(&game->alley, slot, s_npcs[slot].kf_count));
        }
        return;
    }
    if (sh_town_active(&game->town)) {
        for (int slot = 0; slot < SH_GFX_MAX_ACTORS; slot++) {
            if (s_npcs[slot].split_head) { continue; }
            ShActorPose pose;
            if (!s_npcs[slot].ready || !sh_town_actor_pose(&game->town, slot, &pose)) {
                continue;
            }
            bake_npc_frame(slot, sh_town_actor_keyframe(&game->town, slot, s_npcs[slot].kf_count));
        }
        return;
    }
    for (int slot = 0; slot < SH_GFX_MAX_ACTORS; slot++) {
        ShActorPose pose;
        if (!s_npcs[slot].ready || !sh_cafe_actor_pose(&game->cafe, slot, &pose)) {
            continue;
        }
        bake_npc_frame(slot, sh_cafe_actor_keyframe(&game->cafe, slot, s_npcs[slot].kf_count));
    }
}

/*
 * Load area chunks. If radius >= 0, only keep cells within Chebyshev distance of
 * (focus_x, focus_z) so New Game does not stall on all 128 THR tiles.
 */
static void load_area_ex(ShGame *game, int index, int focus_x, int focus_z, int radius)
{
    char path[sizeof(SH_BG_DIR) + 1 + sizeof(((struct dirent *)0)->d_name)];

    game->area_loaded = false;
    game->load_error = NULL;
    sh_assets_log("load area=%d areas=%d focus=%d,%d radius=%d", index, game->area_count, focus_x, focus_z, radius);
    game->texture_loaded = false;
    game->window_valid = false;
    game->chunk_count = 0;
    game->glb = NULL;
    game->glb_size = 0;
    game->job.stage = SH_WINDOW_IDLE;
    sh_coll_world_reset(&game->collision);
    sh_gfx_map_clear();

    if (game->area_count == 0) {
        game->load_error = "Cartes introuvables dans le jeu.\nRelance le fichier .3dsx complet.\nDiagnostic SD : sh3ds_load.log";
        sh_assets_log("load failed: no areas discovered");
        return;
    }
    game->area_index = (index % game->area_count + game->area_count) % game->area_count;
    const char *area = game->area_names[game->area_index];

    sh_arena_reset(sh_memory_level_arena());

    snprintf(path, sizeof(path), "%s/%s_GLB.PLM", SH_BG_DIR, area);
    game->glb = load_file(path, &game->glb_size);

    DIR *dir = opendir(SH_BG_DIR);
    if (!dir) {
        game->load_error = "Les ressources ne sont plus lisibles.\nFerme le jeu puis relance-le.\nDiagnostic SD : sh3ds_load.log";
        sh_assets_log("load failed: cannot open %s", SH_BG_DIR);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && game->chunk_count < SH_GAME_MAX_CHUNKS) {
        char name_area[SH_GAME_AREA_LEN];
        int x, z;
        if (!parse_chunk_name(entry->d_name, name_area, &x, &z) || strcmp(name_area, area) != 0) {
            continue;
        }
        if (radius >= 0) {
            const int dx = x - focus_x;
            const int dz = z - focus_z;
            const int adx = dx < 0 ? -dx : dx;
            const int adz = dz < 0 ? -dz : dz;
            if (adx > radius || adz > radius) {
                continue;
            }
        }

        snprintf(path, sizeof(path), "%s/%s", SH_BG_DIR, entry->d_name);
        ShMapChunk *chunk = &game->chunks[game->chunk_count];
        chunk->data = load_file(path, &chunk->size);
        if (!chunk->data) {
            continue;
        }
        chunk->x = x;
        chunk->z = z;
        sh_coll_world_add(&game->collision, chunk->data, chunk->size);
        game->chunk_count++;
    }
    closedir(dir);

    if (game->chunk_count == 0) {
        game->load_error = "Impossible de lire les cartes de la zone.\nRelance le fichier .3dsx complet.\nDiagnostic SD : sh3ds_load.log";
        sh_assets_log("load failed: no readable chunks for %s", area);
        return;
    }

    const ShMapChunk *start = &game->chunks[0];
    for (int i = 0; i < game->chunk_count; i++) {
        if (game->chunks[i].x == focus_x && game->chunks[i].z == focus_z) {
            start = &game->chunks[i];
            break;
        }
        if (game->chunks[i].x == 0 && game->chunks[i].z == 0) {
            start = &game->chunks[i];
        }
    }
    place_player_in_chunk(game, start);
    game->area_loaded = true;
    sh_assets_log("loaded %s chunks=%d memory=%lu/%lu", area, game->chunk_count,
                  (unsigned long)sh_memory_level_arena()->used, (unsigned long)sh_memory_level_arena()->capacity);
}

static void load_area(ShGame *game, int index)
{
    load_area_ex(game, index, 0, 0, -1);
}

static int find_area_index(const ShGame *game, const char *name)
{
    for (int i = 0; i < game->area_count; i++) {
        if (strcmp(game->area_names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static void window_job_start(ShGame *game, int cx, int cz);
static void window_job_run(ShGame *game, bool blocking);

static void load_npcs_town(void);

/* MAP2_S00 door exit: leave cafe onto Old Silent Hill street (map0_s01 MAP_POINTS[7]). */
/* SysState_LoadOverlay from map2_s00. */
static void town_overlay_transition(ShGame *game)
{
    const uint8_t map_idx = sh_town_overlay_map_idx(&game->town);
    const bool room = sh_town_overlay_is_room(&game->town);
    const unsigned target_map = room ? game->save.map_idx : map_idx;
    if (target_map >= sizeof(s_overlay_info) / sizeof(s_overlay_info[0])) {
        return;
    }
    const int target_area = find_area_index(game, s_overlay_info[target_map].area);
    if (target_area < 0) {
        return;
    }
    float wx = SH_TOWN_SPAWN_X;
    float wz = SH_TOWN_SPAWN_Z;
    const float wyaw = sh_town_overlay_yaw(&game->town);
    sh_town_overlay_target(&game->town, &wx, &wz);
    game->town.leave_overlay = false;

    if (!room && map_idx == SH_MAP_IDX_MAP0_S01) {
        sh_snd_call(19);
        sh_bgm_crossfade_reset();
        sh_town_reset(&game->town);
        sh_alley_reset(&game->alley);
        const int spawn_cx = (int)floorf(SH_CAFE_EXPLORE_X / SH_CHUNK_METERS);
        const int spawn_cz = (int)floorf(-SH_CAFE_EXPLORE_Z / SH_CHUNK_METERS);
        int thr = find_area_index(game, "THR");
        if (thr < 0) {
            thr = game->area_index;
        }
        load_area_ex(game, thr, spawn_cx, spawn_cz, 3);
        if (!try_place_at(game, SH_CAFE_EXPLORE_X, SH_CAFE_EXPLORE_Z)) {
            try_place_at(game, SH_CAFE_SPAWN_X, SH_CAFE_SPAWN_Z);
        }
        game->player_yaw = 0.0f;
        game->save.map_idx = SH_MAP_IDX_MAP0_S01;
        load_npcs_cafe();
        sh_cafe_begin_explore(&game->cafe, &game->save);
        update_chase_camera(game);
        window_job_start(game, spawn_cx, spawn_cz);
        game->phase = SH_PHASE_LOADING;
        return;
    }

    const int spawn_cx = (int)floorf(wx / SH_CHUNK_METERS);
    const int spawn_cz = (int)floorf(-wz / SH_CHUNK_METERS);
    if (!room) {
        sh_snd_call(19);
        sh_bgm_crossfade_reset();
    }
    load_area_ex(game, target_area, spawn_cx, spawn_cz, 5);
    /* Collision is rebuilt by the window job; the ground snap after loading sets Y. */
    game->player_pos[0] = wx;
    game->player_pos[1] = 0.0f;
    game->player_pos[2] = wz;
    game->player_yaw = wyaw;
    game->cam_pitch = 0.12f;
    if (!room) {
        game->save.map_idx = map_idx;
        if (map_idx != SH_MAP_IDX_MAP1_S03) { sh_school_roof_water_reset(); }
        game->save.map_room_idx = 0;
        if (map_idx == SH_MAP_IDX_MAP1_S01) {
            load_harry_map_anm(SH_ANIM_DIR "/HB_M1S01.ANM");
            if (!game->piano_cursor_ready) {
                game->piano_cursor_ready = load_piano_cursor();
                sh_assets_log("school piano cursor ready=%d", game->piano_cursor_ready);
                if (!game->piano_cursor_ready) {
                    game->load_error = "Le curseur du piano n'a pas pu etre charge.\nDiagnostic SD : sh3ds_load.log";
                }
            }
            if (!game->smoke_ready) {
                game->smoke_ready = load_smoke_atlas();
                sh_assets_log("school smoke atlas ready=%d", game->smoke_ready);
                if (!game->smoke_ready) {
                    game->load_error = "L'effet de fumee n'a pas pu etre charge.\nDiagnostic SD : sh3ds_load.log";
                }
            }
        } else if (map_idx == SH_MAP_IDX_MAP1_S00) {
            load_harry_map_anm(SH_ANIM_DIR "/HB_M1S00.ANM");
        } else if (map_idx == SH_MAP_IDX_MAP1_S02) {
            game->town.school.bathroom = (ShBathroomMotion){0};
            game->town.school.boss_doors = (ShBossDoors){0};
            game->town.school.boss_box_y = 0;
            load_harry_map_anm(SH_ANIM_DIR "/HB_M1S02.ANM");
        } else if (map_idx == SH_MAP_IDX_MAP1_S03) {
            sh_school_roof_world_reset(&game->town);
            load_harry_map_anm(SH_ANIM_DIR "/HB_M1S03.ANM");
            if (!game->water_ready) { game->water_ready = load_water_atlas(); }
            sh_assets_log("school water atlases ready=%d", game->water_ready);
            if (!game->water_ready) {
                game->load_error = "L'effet d'eau n'a pas pu etre charge.\nDiagnostic SD : sh3ds_load.log";
            }
        } else if (map_idx == SH_MAP_IDX_MAP2_S00) {
            load_harry_map_anm(SH_ANIM_DIR "/HB_M2S00.ANM");
        }
        game->town.school_boiler_volume = 0;
        sh_env_set(&game->town.environment, s_overlay_info[target_map].presets[0],
                   s_overlay_info[target_map].presets[1]);
        if (sh_school_map_is_school(map_idx)) {
            /* Sd_AmbientSfxInit / Sd_AmbientSfxSet, background_sound_init.c. */
            sh_snd_call(17);
            sh_snd_call(s_overlay_info[target_map].ambient_task);
            game->loading_ambient = true;
        }
    } else {
        game->save.map_room_idx = game->town.overlay_room_idx;
    }
    game->town.stage = SH_TOWN_EXPLORE;
    game->town.controls_locked = false;
    game->town.fade = 1.0f;
    game->town.fade_target = sh_town_overlay_skip_fade_in(&game->town) ? 1.0f : 0.0f;
    game->town.fade_speed = 3.0f;
    game->town.need_ground_snap = true;
    sh_town_overlay_camera_init(&game->town, game->save.map_idx);
    sh_vc_warp(&game->town.vc); /* camera re-init on area load (GameBoot) */
    update_chase_camera(game);
    window_job_start(game, spawn_cx, spawn_cz);
    game->phase = SH_PHASE_LOADING;
}

static void leave_cafe_to_town(ShGame *game)
{
    const int spawn_cx = (int)floorf(SH_TOWN_SPAWN_X / SH_CHUNK_METERS);
    const int spawn_cz = (int)floorf(-SH_TOWN_SPAWN_Z / SH_CHUNK_METERS);
    int thr = find_area_index(game, "THR");
    if (thr < 0) {
        thr = game->area_index;
    }
    sh_snd_call(19);
    sh_bgm_crossfade_reset();
    load_area_ex(game, thr, spawn_cx, spawn_cz, 5);
    game->player_yaw = SH_TOWN_SPAWN_YAW;
    game->cam_pitch = 0.12f;
    if (!try_place_at(game, SH_TOWN_SPAWN_X, SH_TOWN_SPAWN_Z)) {
        bool ok = false;
        const float ox[] = {0.0f, 2.0f, -2.0f, 4.0f, -4.0f};
        const float oz[] = {0.0f, 2.0f, -2.0f, 4.0f, -4.0f};
        for (size_t i = 0; i < sizeof(ox) / sizeof(ox[0]) && !ok; i++) {
            for (size_t j = 0; j < sizeof(oz) / sizeof(oz[0]) && !ok; j++) {
                ok = try_place_at(game, SH_TOWN_SPAWN_X + ox[i], SH_TOWN_SPAWN_Z + oz[j]);
            }
        }
        if (!ok) {
            for (int i = 0; i < game->chunk_count; i++) {
                if (game->chunks[i].x == spawn_cx && game->chunks[i].z == spawn_cz) {
                    place_player_in_chunk(game, &game->chunks[i]);
                    break;
                }
            }
            game->player_yaw = SH_TOWN_SPAWN_YAW;
        }
    }
    game->cafe.leave_town = false;
    sh_cafe_reset(&game->cafe);
    sh_alley_reset(&game->alley);
    load_npcs_town();
    sh_town_begin(&game->town, &game->save);
    update_chase_camera(game);
    {
        const int cx = (int)floorf(game->player_pos[0] / SH_CHUNK_METERS);
        const int cz = (int)floorf(-game->player_pos[2] / SH_CHUNK_METERS);
        window_job_start(game, cx, cz);
    }
    game->phase = SH_PHASE_LOADING;
}

static void boot_thr_at(ShGame *game, float x, float z, int radius)
{
    const int spawn_cx = (int)floorf(x / SH_CHUNK_METERS);
    const int spawn_cz = (int)floorf(-z / SH_CHUNK_METERS);

    sh_save_init(&game->save);
    sh_save_reset_session(&game->save);
    sh_snd_stop_all();
    sh_radio_init(&game->radio);
    sh_snd_reset();
    sh_alley_reset(&game->alley);
    sh_cafe_reset(&game->cafe);
    sh_town_reset(&game->town);
    sh_inv_close(&game->inventory);
    game->noclip = false;

    int thr = find_area_index(game, "THR");
    if (thr < 0) {
        thr = game->area_index;
    }
    load_area_ex(game, thr, spawn_cx, spawn_cz, radius);

    if (!try_place_at(game, x, z)) {
        for (int i = 0; i < game->chunk_count; i++) {
            if (game->chunks[i].x == spawn_cx && game->chunks[i].z == spawn_cz) {
                place_player_in_chunk(game, &game->chunks[i]);
                break;
            }
        }
    }
    game->cam_pitch = 0.08f;
    update_chase_camera(game);
    window_job_start(game, spawn_cx, spawn_cz);
    game->phase = SH_PHASE_LOADING;
}

/* Full New Game: map0_s00 alley intro → cafe. */
static void start_new_game_full(ShGame *game)
{
    boot_thr_at(game, SH_INTRO_SPAWN_X, SH_INTRO_SPAWN_Z, 5);
    game->player_yaw = 3.14159265f;
    load_npcs_alley();
    sh_alley_begin(&game->alley, &game->save);
}

/* Dev skip: Cybil cutscene at the cafe. */
static void start_skip_cafe(ShGame *game)
{
    boot_thr_at(game, SH_CAFE_EXPLORE_X, SH_CAFE_EXPLORE_Z, 2);
    if (!try_place_at(game, SH_CAFE_EXPLORE_X, SH_CAFE_EXPLORE_Z)) {
        try_place_at(game, SH_CAFE_SPAWN_X, SH_CAFE_SPAWN_Z);
    }
    game->player_yaw = 0.0f;
    load_npcs_cafe();
    sh_cafe_begin(&game->cafe, &game->save);
}

/* Dev skip: cafe explore (no Cybil cutscene). */
static void start_skip_cafe_explore(ShGame *game)
{
    boot_thr_at(game, SH_CAFE_EXPLORE_X, SH_CAFE_EXPLORE_Z, 2);
    game->player_yaw = 0.0f;
    load_npcs_cafe();
    sh_cafe_begin_explore(&game->cafe, &game->save);
}

/* Dev skip: Air Screamer fight with kit ready. */
static void start_skip_bird_fight(ShGame *game)
{
    boot_thr_at(game, SH_CAFE_EXPLORE_X, SH_CAFE_EXPLORE_Z, 2);
    game->player_yaw = 0.0f;
    load_npcs_cafe();
    sh_cafe_begin_bird_fight(&game->cafe, &game->save);
}

static void skip_town_save_kit(ShSavegame *save)
{
    sh_save_item_add(save, SH_ITEM_POCKET_KNIFE, 1);
    sh_save_item_add(save, SH_ITEM_FLASHLIGHT, 1);
    sh_save_item_add(save, SH_ITEM_HANDGUN, 1);
    sh_save_item_add(save, SH_ITEM_HANDGUN_BULLETS, 15);
    sh_save_item_add(save, SH_ITEM_RADIO, 1);
    sh_save_item_add(save, SH_ITEM_HEALTH_DRINK, 2);
    save->equipped_weapon = SH_ITEM_HANDGUN;
    save->has_map = true;
    sh_save_event_set(save, SH_EV_KNIFE);
    sh_save_event_set(save, SH_EV_FLASHLIGHT);
    sh_save_event_set(save, SH_EV_MAP);
    sh_save_event_set(save, SH_EV_RADIO);
    sh_save_event_set(save, SH_EV_KNIFE_AND_MAP);
    sh_save_event_set(save, SH_EV_BIRD_DIED);
    sh_save_event_set(save, SH_EV_CAFE_DONE);
    sh_save_event_set(save, SH_EV_EXIT_CAFE);
    sh_save_flashlight_set(save, true);
    sh_save_radio_set(save, true);
}

/* Dev skip: post-cafe street (MAP2_S00) with kit + ExitCafe cutscene. */
static void start_skip_post_cafe(ShGame *game)
{
    boot_thr_at(game, SH_TOWN_SPAWN_X, SH_TOWN_SPAWN_Z, 5);
    game->player_yaw = SH_TOWN_SPAWN_YAW;
    skip_town_save_kit(&game->save);
    load_npcs_town();
    sh_town_begin(&game->town, &game->save);
}

/* Dev skip: behind the sketchbook gate (map2_s00 event 51 LoadRoom -> MAP_POINTS[8]). */
static void start_skip_sketch_gate(ShGame *game)
{
    float sx = SH_TOWN_SPAWN_X;
    float sz = SH_TOWN_SPAWN_Z;
    float syaw = SH_TOWN_SPAWN_YAW;
    sh_town_map_point_spawn(8, &sx, &sz, &syaw);
    boot_thr_at(game, sx, sz, 5);
    game->player_pos[0] = sx;
    game->player_pos[2] = sz;
    game->player_yaw = syaw;
    game->cam_pitch = 0.12f;
    skip_town_save_kit(&game->save);
    game->save.map_idx = SH_MAP_IDX_MAP2_S00;
    load_npcs_town();
    sh_town_begin_explore_at(&game->town, &game->save, sx, sz, syaw);
    update_chase_camera(game);
    {
        const int cx = (int)floorf(sx / SH_CHUNK_METERS);
        const int cz = (int)floorf(-sz / SH_CHUNK_METERS);
        window_job_start(game, cx, cz);
    }
    game->phase = SH_PHASE_LOADING;
}

/* Debug checkpoint: sketchbook read (MapEvent_CherylsSketchbook flags), optional doghouse done. */
/* Debug checkpoint: carnet lu, devant la niche Levin St (note au sol). */
static void start_skip_doghouse_note(ShGame *game)
{
    const float sx = -154.56f, sz = -1.78f, syaw = 0.0f; /* map point 14, TouchFacing */
    boot_thr_at(game, sx, sz, 5);
    game->player_pos[0] = sx;
    game->player_pos[2] = sz;
    game->player_yaw = syaw;
    game->cam_pitch = 0.12f;
    skip_town_save_kit(&game->save);
    game->save.map_idx = SH_MAP_IDX_MAP2_S00;
    sh_save_event_set(&game->save, 875);
    sh_save_event_set(&game->save, 147);
    sh_save_event_set(&game->save, 930);
    sh_save_item_add(&game->save, SH_ITEM_NOTE_SCHOOL, 1);
    load_npcs_town();
    sh_town_begin_explore_at(&game->town, &game->save, sx, sz, syaw);
    update_chase_camera(game);
    window_job_start(game, (int)floorf(sx / SH_CHUNK_METERS), (int)floorf(-sz / SH_CHUNK_METERS));
    game->phase = SH_PHASE_LOADING;
}

static void start_skip_after_sketch(ShGame *game, bool dog_done)
{
    const float sx = -154.6f, sz = -4.0f, syaw = 3.14159265f; /* facing the Levin St doghouse (map point 12) */
    boot_thr_at(game, sx, sz, 5);
    game->player_pos[0] = sx;
    game->player_pos[2] = sz;
    game->player_yaw = syaw;
    game->cam_pitch = 0.12f;
    skip_town_save_kit(&game->save);
    game->save.map_idx = SH_MAP_IDX_MAP2_S00;
    sh_save_event_set(&game->save, 875); /* EV_MAPMARK_11 */
    sh_save_event_set(&game->save, 147); /* EV_SKETCH_DONE */
    sh_save_event_set(&game->save, 930); /* EV_SCHOOL_CIRCLE */
    sh_save_item_add(&game->save, SH_ITEM_NOTE_SCHOOL, 1);
    if (dog_done) {
        sh_save_event_set(&game->save, 148); /* EventFlag_M2S00_DogHouseNoteFound */
        sh_save_event_set(&game->save, 149); /* EventFlag_M2S00_PickupDogHouseKey */
        sh_save_item_add(&game->save, SH_ITEM_HOUSE_KEY, 1);
    }
    load_npcs_town();
    sh_town_begin_explore_at(&game->town, &game->save, sx, sz, syaw);
    update_chase_camera(game);
    window_job_start(game, (int)floorf(sx / SH_CHUNK_METERS), (int)floorf(-sz / SH_CHUNK_METERS));
    game->phase = SH_PHASE_LOADING;
}

static void start_debug_checkpoint(ShGame *game, int index)
{
    ShDebugArrival arrival;
    boot_thr_at(game, SH_TOWN_SPAWN_X, SH_TOWN_SPAWN_Z, 5);
    if (!sh_debug_checkpoint_apply(index, &game->save, &arrival)) {
        return;
    }
    load_npcs_town();
    sh_town_begin_explore_at(&game->town, &game->save, arrival.x, arrival.z, arrival.yaw);
    game->town.overlay_sys_state = SH_SYS_LOAD_OVERLAY;
    game->town.overlay_map_idx = arrival.map;
    game->town.overlay_x = arrival.x;
    game->town.overlay_z = arrival.z;
    game->town.overlay_yaw = arrival.yaw;
    town_overlay_transition(game);
    if (arrival.map == SH_MAP_IDX_MAP2_S00 && sh_save_event_get(&game->save, 159)) {
        /* map2_s00.c MapEvent_DoorOfEclipseEnter case 4. */
        sh_env_set(&game->town.environment, 6, 3);
    }
    game->save.map_room_idx = sh_school_map_is_school(arrival.map) ?
        sh_school_map_room_idx(arrival.map, arrival.x, arrival.z) : 0;
}

static bool restore_savegame(ShGame *game, const ShAutosave *snapshot)
{
    const uint8_t map = (uint8_t)snapshot->save.map_idx;
    if (!snapshot->valid || (map != SH_MAP_IDX_MAP0_S01 && map != SH_MAP_IDX_MAP0_S02 &&
        map != SH_MAP_IDX_MAP2_S00 && !sh_school_map_is_school(map))) { return false; }
    const ShAutosave restored = *snapshot;
    const float x = SH_Q12_TO_FLOAT(restored.position[0]);
    const float y = -SH_Q12_TO_FLOAT(restored.position[1]);
    const float z = -SH_Q12_TO_FLOAT(restored.position[2]);
    const float yaw = -(float)restored.yaw * (6.28318530717958647692f / 4096.0f);
    boot_thr_at(game, x, z, 5);
    game->save = restored.save;
    if (map == SH_MAP_IDX_MAP0_S01) {
        load_npcs_cafe();
        sh_cafe_begin_explore(&game->cafe, &game->save);
        game->cafe.got_text = NULL;
        game->cafe.got_time = 0.0f;
    } else {
        load_npcs_town();
        sh_town_begin_explore_at(&game->town, &game->save, x, z, yaw);
        game->save = restored.save;
        game->town.overlay_sys_state = SH_SYS_LOAD_OVERLAY;
        game->town.overlay_map_idx = map;
        game->town.overlay_x = x;
        game->town.overlay_z = z;
        game->town.overlay_yaw = yaw;
        town_overlay_transition(game);
        if (map == SH_MAP_IDX_MAP2_S00 && sh_save_event_get(&restored.save, 159)) {
            sh_env_set(&game->town.environment, 6, 3);
        }
    }
    game->save = restored.save;
    game->player_pos[0] = x;
    game->player_pos[1] = y;
    game->player_pos[2] = z;
    game->player_yaw = yaw;
    update_chase_camera(game);
    sh_save_autosave_set(&game->save, restored.position[0], restored.position[1], restored.position[2], restored.yaw);
    game->phase = SH_PHASE_LOADING;
    return true;
}

static void open_save_menu(ShGame *game, bool writing)
{
    ShAutosave snapshot = {0};
    if (writing) {
        snapshot.save = game->save;
        snapshot.position[0] = SH_Q12(game->player_pos[0]);
        snapshot.position[1] = SH_Q12(-game->player_pos[1]);
        snapshot.position[2] = SH_Q12(-game->player_pos[2]);
        snapshot.yaw = (int16_t)SH_Q12(-game->player_yaw / 6.28318530717958647692f);
        snapshot.valid = true;
    }
    game->save_return_phase = game->phase;
    sh_save_menu_begin(&game->save_menu, writing, writing ? &snapshot : NULL, "sdmc:");
    sh_snd_stop(SH_SFX_RADIO_INTERFERENCE);
    game->radio.noise_on = false;
    game->phase = SH_PHASE_SAVE;
}

static void open_movie(ShGame *game, unsigned movie)
{
    char video[80], audio[80];
    snprintf(video, sizeof(video), "romfs:/game/movie/%03u.shv", movie);
    snprintf(audio, sizeof(audio), "romfs:/game/movie/%03u.xac", movie);
    game->alley.movie_requested = false;
    if (!sh_movie_open(&game->movie, video) || !sh_sound_movie_begin(audio)) {
        sh_movie_close(&game->movie);
        sh_assets_log("movie %u load failed", movie);
        game->load_error = "Les ressources ne sont plus lisibles.\nFerme le jeu puis relance-le.\nDiagnostic SD : sh3ds_load.log";
        game->phase = SH_PHASE_LOADING;
        return;
    }
    game->movie_start_ms = osGetTime();
    game->phase = SH_PHASE_MOVIE;
}

/* Alley finished → hand off to cafe cutscene (stay black through the load). */
static void alley_to_cafe(ShGame *game)
{
    const int spawn_cx = (int)floorf(SH_CAFE_SPAWN_X / SH_CHUNK_METERS);
    const int spawn_cz = (int)floorf(-SH_CAFE_SPAWN_Z / SH_CHUNK_METERS);
    int thr = find_area_index(game, "THR");
    if (thr < 0) {
        thr = game->area_index;
    }
    /* Kill alley XA/BGM so cafe crossfade(9) cannot soft-lock on black. */
    sh_snd_call(19);
    sh_snd_call(22);
    sh_snd_stop_all();
    sh_snd_reset();
    sh_bgm_crossfade_reset();
    sh_alley_reset(&game->alley);
    load_area_ex(game, thr, spawn_cx, spawn_cz, 2);
    if (!try_place_at(game, SH_CAFE_SPAWN_X, SH_CAFE_SPAWN_Z) &&
        !try_place_at(game, SH_CAFE_EXPLORE_X, SH_CAFE_EXPLORE_Z)) {
        for (int i = 0; i < game->chunk_count; i++) {
            if (game->chunks[i].x == spawn_cx && game->chunks[i].z == spawn_cz) {
                place_player_in_chunk(game, &game->chunks[i]);
                break;
            }
        }
    }
    game->player_yaw = 0.0f;
    game->cam_pitch = 0.08f;
    load_npcs_cafe();
    sh_cafe_begin(&game->cafe, &game->save); /* starts fade=1 → wake from black */
    update_chase_camera(game);
    window_job_start(game, spawn_cx, spawn_cz);
    game->phase = SH_PHASE_LOADING;
}

/* Background TIMs are 4-bit 256x256 with up to 16 palettes, about 33 KB. */
#define SH_BG_TIM_MAX_BYTES (64u * 1024u)

/* Map window work allowed per frame; the rest of the 16.7 ms goes to the game and draw calls. */
#define SH_WINDOW_BUDGET_TICKS (SYSCLOCK_ARM11 / 1000u * 5u)
#define SH_WINDOW_EMIT_SLICE 8000 /* triangles */

static bool window_storage_init(ShGame *game)
{
    ShArena *stream = sh_memory_stream_arena();
    size_t capacity = 0;

    if (!sh_gfx_map_back_storage(&capacity) || capacity == 0) {
        return false;
    }
    game->job_verts = sh_arena_alloc(stream, sizeof(ShMapVertex) * capacity, 16);
    game->job_tri_slots = sh_arena_alloc(stream, capacity / 3, 4);
    game->job_tim_file = sh_arena_alloc(stream, SH_BG_TIM_MAX_BYTES, 16);
    game->job_capacity = capacity;
    return game->job_verts && game->job_tri_slots && game->job_tim_file;
}

static unsigned sh_game_world_key(const ShGame *game)
{
    if (!game) {
        return 0;
    }
    const unsigned cafe = sh_cafe_world_key(&game->cafe, &game->save);
    const unsigned town = sh_town_world_key(&game->town, &game->save);
    return cafe ^ (town << 1);
}

/* Starts rebuilding the 3x3 chunks around (cx, cz), nearest first. */
static void window_job_start(ShGame *game, int cx, int cz)
{
    ShWindowJob *job = &game->job;

    job->stage = SH_WINDOW_IDLE;
    if (!game->job_verts) {
        return;
    }
    job->cx = cx;
    job->cz = cz;
    job->chunk_count = 0;
    job->next = 0;
    job->lost = 0;
    for (int ring = 0; ring <= 1; ring++) {
        for (int i = 0; i < game->chunk_count && job->chunk_count < 9; i++) {
            const int dx = abs(game->chunks[i].x - cx);
            const int dz = abs(game->chunks[i].z - cz);
            if ((dx > dz ? dx : dz) == ring) {
                job->chunks[job->chunk_count++] = i;
            }
        }
    }

    sh_maptex_reset(&job->textures);
    sh_map_builder_init(&job->builder, game->job_verts, game->job_tri_slots, game->job_capacity, sh_maptex_resolve,
                        &job->textures);
    job->world_key = sh_game_world_key(game);
    job->stage = SH_WINDOW_BUILD;
}

/* BG_ITEM.PLM: common pickups (DRINK_NE, AIDKIT_N…), g_WorldGfxWork.itemLmHdr. */
#define SH_BG_ITEM_MAX (4u * 1024u)
static uint8_t s_bg_item[SH_BG_ITEM_MAX];
static size_t s_bg_item_size;

/* WorldObjects_Add: a named LM model from whichever window chunk owns it, else the item library. */
static void window_add_world_object(ShGame *game, const char *name, const float pos_ps1[3], const float rot_deg[3])
{
    ShWindowJob *job = &game->job;
    for (int i = 0; i < job->chunk_count; i++) {
        const ShMapChunk *chunk = &game->chunks[job->chunks[i]];
        if (sh_map_add_named_model(&job->builder, chunk->data, chunk->size, game->glb, game->glb_size, name, pos_ps1,
                                   rot_deg)) {
            return;
        }
    }
    if (s_bg_item_size > 0 && job->chunk_count > 0) {
        const ShMapChunk *chunk = &game->chunks[job->chunks[0]];
        sh_map_add_named_model(&job->builder, chunk->data, chunk->size, s_bg_item, s_bg_item_size, name, pos_ps1,
                               rot_deg);
    }
}

/* Map_WorldObjectsUpdate + CutsceneObjects_Add (map0_s01_events.c). */
static void window_add_cafe_objects(ShGame *game)
{
    static const float cutscene_pos[3] = {0.0f, 0.0f, 280.0f}; /* g_CutscenePosition */
    static const float zero_rot[3] = {0.0f, 0.0f, 0.0f};
    const ShSavegame *save = &game->save;

    if (game->cafe.stage == SH_CAFE_OFF) {
        return;
    }

    switch (sh_cafe_world_state(&game->cafe, save)) {
    case 1:
        window_add_world_object(game, "OUT1_HID", cutscene_pos, zero_rot);
        window_add_world_object(game, "OUT_BLD_", cutscene_pos, zero_rot);
        break;
    case 2:
        window_add_world_object(game, "IN1_HIDE", cutscene_pos, zero_rot);
        window_add_world_object(game, "BRK_BLD1", cutscene_pos, zero_rot);
        window_add_world_object(game, "BRK_GLS1", cutscene_pos, zero_rot);
        window_add_world_object(game, "BRK_GLS2", cutscene_pos, zero_rot);
        break;
    default:
        window_add_world_object(game, "IN1_HIDE", cutscene_pos, zero_rot);
        window_add_world_object(game, "IN_BLD1_", cutscene_pos, zero_rot);
        break;
    }

    /* WorldObject_Init(...) poses: PS1 meters, degrees. */
    if (!sh_save_event_get(save, SH_EV_MAP)) {
        const float p[3] = {5.24f, -1.0f, 267.21f}, r[3] = {0.0f, 57.0f, 0.0f};
        window_add_world_object(game, "MAP_HIDE", p, r);
    }
    if (!sh_save_event_get(save, SH_EV_FLASHLIGHT)) {
        const float p[3] = {5.18f, -1.0f, 267.28f}, r[3] = {-3.0f, 111.0f, 0.0f};
        window_add_world_object(game, "FLASH_HI", p, r);
    }
    if (!sh_save_event_get(save, SH_EV_RADIO)) {
        const float p[3] = {2.31f, -0.6f, 273.95f}, r[3] = {0.0f, 147.0f, 0.0f};
        window_add_world_object(game, "RADIO_HI", p, r);
    }
    if (!sh_save_event_get(save, SH_EV_KNIFE)) {
        const float p[3] = {5.09f, -1.01f, 274.12f}, r[3] = {0.0f, 32.0f, 0.0f};
        window_add_world_object(game, "KNIFE_HI", p, r);
    }
    /* D_800DE12C / D_800DE140 (map0_s01 overlay data). */
    if (!sh_save_event_get(save, SH_EV_HEALTH_DRINK0)) {
        const float p[3] = {5.1f, -1.0f, 273.7f}, r[3] = {0.0f, 51.9f, 0.0f};
        window_add_world_object(game, "DRINK_NE", p, r);
    }
    if (!sh_save_event_get(save, SH_EV_HEALTH_DRINK1)) {
        const float p[3] = {4.95f, -1.0f, 267.5f}, r[3] = {0.0f, 69.0f, 0.0f};
        window_add_world_object(game, "DRINK_NE", p, r);
    }
}

/* Map_WorldObjectsUpdate map2_s00.c — hide props once collected / examined. */
static void window_add_town_objects(ShGame *game)
{
    const ShSavegame *save = &game->save;

    if (!sh_town_active(&game->town) || save->map_idx != SH_MAP_IDX_MAP2_S00) {
        return;
    }
    if (!sh_save_event_get(save, 161)) { /* EventFlag_M2S00_PickupSteelPipe */
        const float p[3] = {-187.591f, 0.093f, -300.271f}, r[3] = {4.4f, 36.8f, 2.9f};
        window_add_world_object(game, "IRONPIPE", p, r);
    }
    if (!sh_save_event_get(save, 147)) { /* sketchbook */
        const float p[3] = {-186.68f, 0.483f, -300.829f}, r[3] = {4.1f, -74.05f, -9.5f};
        window_add_world_object(game, "SKB2_NEA", p, r);
    }
    if (!sh_save_event_get(save, 148)) {
        const float p1[3] = {-154.56f, 0.0f, -1.78f}, r1[3] = {0.0f, -51.7f, 0.0f};
        const float p2[3] = {-154.6f, 0.0f, -2.22f}, r2[3] = {0.0f, 74.3f, 0.0f};
        window_add_world_object(game, "PPR1_NEA", p1, r1);
        window_add_world_object(game, "PPR2_NEA", p2, r2);
    }
    for (int i = 0; i < MAP_WORLD_OBJECT_COUNT_MAP2_S00; i++) {
        const ShMapWorldObject *wo = &s_map_world_objects_map2_s00[i];
        if (sh_save_event_get(save, wo->event_flag)) {
            continue;
        }
        const float p[3] = {wo->pos_x, wo->pos_y, wo->pos_z};
        const float r[3] = {wo->rot_x, wo->rot_y, wo->rot_z};
        window_add_world_object(game, wo->model, p, r);
    }
}

static void window_job_sort_pairs(ShWindowJob *job)
{
    const ShMapTexSet *set = &job->textures;

    for (int i = 0; i < set->count; i++) {
        const int idx = i;
        int j = i - 1;
        while (j >= 0 && memcmp(set->pairs[job->pair_order[j]].name, set->pairs[idx].name, 8) > 0) {
            job->pair_order[j + 1] = job->pair_order[j];
            j--;
        }
        job->pair_order[j + 1] = idx;
    }
}

static void window_add_school_objects(ShGame *game)
{
    if (!sh_school_map_is_school(game->save.map_idx) ||
        !sh_town_active(&game->town)) {
        return;
    }
    ShWindowJob *job = &game->job;
    int count;
    const ShSchoolProp *props = sh_school_map_props(game->save.map_idx, &count);
    for (int i = 0; i < count && job->object_count < SH_MAP_MAX_OBJECTS; i++) {
        const size_t first = job->builder.count;
        window_add_world_object(game, props[i].model, props[i].pos, props[i].rotation);
        if (job->builder.count > first) {
            ShWindowObject *object = &job->objects[job->object_count++];
            object->first = first;
            object->count = job->builder.count - first;
            object->mask = props[i].mask;
        }
    }
}

static int window_job_unplaced(const ShMapTexSet *set)
{
    int unplaced = 0;
    for (int i = 0; i < set->count; i++) {
        unplaced += set->pairs[i].page < 0 && set->pairs[i].max_u >= set->pairs[i].min_u;
    }
    return unplaced;
}

/* Packs into spare atlas pages when possible; otherwise reuses the drawn pages in-place. */
static void window_job_pack(ShGame *game)
{
    ShWindowJob *job = &game->job;
    ShMapTexSet *set = &job->textures;
    int free_pages = 0;
    const int pool = sh_gfx_atlas_pages();
    const int pack_limit = pool < SH_MAPTEX_MAX_PAGES ? pool : SH_MAPTEX_MAX_PAGES;

    sh_maptex_measure(set, game->job_verts, game->job_tri_slots, job->builder.count);

    for (int p = 0; p < pool && free_pages < pack_limit; p++) {
        if (!sh_gfx_atlas_page_busy(p)) {
            job->page_map[free_pages++] = p;
        }
    }
    job->pages = sh_maptex_pack(set, free_pages);
    job->lost = window_job_unplaced(set);
    job->inplace = false;

    if (job->lost > 0 || free_pages == 0) {
        /* Keep showing the old view until this job finishes; overwrite its pages only
         * after begin_frame (GPU idle), then present before the next draw. */
        for (int p = 0; p < pack_limit; p++) {
            job->page_map[p] = p;
        }
        job->pages = sh_maptex_pack(set, pack_limit);
        job->lost = window_job_unplaced(set);
        job->inplace = true;
    }

    window_job_sort_pairs(job);
    memset(job->tim_name, 0, sizeof(job->tim_name));
    job->tim_ok = false;
    job->next = 0;
    for (int p = 0; p < job->pages; p++) {
        sh_gfx_atlas_clear_page(job->page_map[p]);
    }
    job->stage = SH_WINDOW_TEXTURES;
}

static void window_job_upload_pair(ShGame *game, ShMapTexPair *pair)
{
    ShWindowJob *job = &game->job;

    if (pair->page < 0) {
        return;
    }
    if (memcmp(job->tim_name, pair->name, 8) != 0) {
        char name[9] = {0};
        char path[sizeof(SH_BG_DIR) + 16];
        size_t size = 0;
        memcpy(job->tim_name, pair->name, 8);
        memcpy(name, pair->name, 8);
        snprintf(path, sizeof(path), "%s/%s.TIM", SH_BG_DIR, name);
        job->tim_ok = read_file_into(path, game->job_tim_file, SH_BG_TIM_MAX_BYTES, &size) &&
                      sh_tim_parse(game->job_tim_file, size, &job->tim);
    }
    if (!job->tim_ok) {
        pair->page = -1;
        job->lost++;
        return;
    }
    sh_gfx_atlas_blit(job->page_map[pair->page], pair->x, pair->y, &job->tim, pair->min_u, pair->min_v,
                      pair->max_u - pair->min_u + 1, pair->max_v - pair->min_v + 1,
                      sh_tim_clut_offset(&job->tim, pair->clut_delta));
}

static void window_job_finish(ShGame *game)
{
    ShWindowJob *job = &game->job;
    ShMapRange ranges[SH_MAP_MAX_RANGES];
    int range_count = job->layout.range_count;
    size_t vertices = job->static_vert_count;
    size_t capacity;
    ShMapVertex *back = sh_gfx_map_back_storage(&capacity);

    for (int r = 0; r < job->layout.range_count; r++) {
        ranges[r] = job->layout.ranges[r];
    }
    for (int i = 0; i < job->object_count; i++) {
        const ShWindowObject *object = &job->objects[i];
        const int count = sh_maptex_finalize(&job->textures, job->pages, game->job_verts + object->first,
                                             game->job_tri_slots + object->first / 3, object->count,
                                             back + vertices, ranges + range_count);
        for (int r = range_count; r < range_count + count; r++) {
            ranges[r].first += vertices;
            ranges[r].visibility_mask = object->mask;
        }
        vertices += object->count;
        range_count += count;
    }
    for (int r = 0; r < range_count; r++) {
        if (ranges[r].page >= 0) {
            ranges[r].page = job->page_map[ranges[r].page];
        }
    }
    sh_gfx_map_present(vertices, ranges, range_count);

    game->window_pairs = job->textures.count;
    game->window_pages = job->pages;
    game->window_textures_lost = job->lost;
    game->window_x = job->cx;
    game->window_z = job->cz;
    game->window_world_key = job->world_key;
    game->window_valid = true;
    game->window_triangles = job->builder.count / 3;
    game->window_overflow = job->builder.overflow;
    job->stage = SH_WINDOW_IDLE;
}

/* Runs one bounded unit of work; returns false once the job is idle. */
static bool window_job_step(ShGame *game)
{
    ShWindowJob *job = &game->job;

    switch (job->stage) {
    case SH_WINDOW_BUILD:
        if (job->next < job->chunk_count) {
            const ShMapChunk *chunk = &game->chunks[job->chunks[job->next++]];
            sh_map_add_chunk(&job->builder, chunk->data, chunk->size, game->glb, game->glb_size);
        } else {
            window_add_cafe_objects(game);
            window_add_town_objects(game);
            job->static_vert_count = job->builder.count;
            job->object_count = 0;
            window_add_school_objects(game);
            job->stage = SH_WINDOW_PACK;
        }
        return true;
    case SH_WINDOW_PACK:
        window_job_pack(game);
        return true;
    case SH_WINDOW_TEXTURES:
        if (job->next < job->textures.count) {
            window_job_upload_pair(game, &job->textures.pairs[job->pair_order[job->next++]]);
        } else {
            for (int p = 0; p < job->pages; p++) {
                sh_gfx_atlas_flush_page(job->page_map[p]);
            }
            sh_maptex_layout(&job->textures, job->pages, game->job_tri_slots, job->static_vert_count, &job->layout);
            job->next = 0;
            job->stage = SH_WINDOW_EMIT;
        }
        return true;
    case SH_WINDOW_EMIT: {
        const size_t tri_count = job->static_vert_count / 3;
        size_t capacity = 0;
        ShMapVertex *back = sh_gfx_map_back_storage(&capacity);
        if (!back) {
            job->stage = SH_WINDOW_IDLE;
            return false;
        }
        const size_t first = (size_t)job->next;
        const size_t slice = tri_count - first < SH_WINDOW_EMIT_SLICE ? tri_count - first : SH_WINDOW_EMIT_SLICE;
        sh_maptex_emit(&job->textures, &job->layout, game->job_verts, game->job_tri_slots, first, slice, back);
        job->next += (int)slice;
        if ((size_t)job->next >= tri_count) {
            window_job_finish(game);
            return false;
        }
        return true;
    }
    case SH_WINDOW_IDLE:
    default:
        return false;
    }
}

/* Advances the job within the frame budget, or to completion when nothing is drawn yet. */
static void window_job_run(ShGame *game, bool blocking)
{
    const uint64_t start = svcGetSystemTick();

    while (window_job_step(game)) {
        const ShWindowJob *job = &game->job;
        /* Overwriting the drawn atlas, or writing the only VBO, must finish before draw. */
        if (job->inplace && job->stage != SH_WINDOW_BUILD && job->stage != SH_WINDOW_PACK) {
            blocking = true;
        }
        if (!sh_gfx_map_double_buffered() && job->stage == SH_WINDOW_EMIT) {
            blocking = true;
        }
        if (!blocking && svcGetSystemTick() - start >= SH_WINDOW_BUDGET_TICKS) {
            break;
        }
    }
}

static void update_player(ShGame *game, const ShInputState *input, float dt)
{
    float cafe_radio = -1.0f;
    float fwd = 0.0f;
    bool want_run = false;
    const bool alley_active = sh_alley_active(&game->alley);
    const bool town_active = sh_town_active(&game->town);
    const bool cafe_active = game->cafe.stage != SH_CAFE_OFF;
    bool cafe_lock = false;
    bool map_open = sh_cafe_map_open(&game->cafe) || sh_town_map_open(&game->town);

    if (game->alley.room_ready) {
        const int cx = (int)floorf(game->alley.room_x / SH_CHUNK_METERS);
        const int cz = (int)floorf(-game->alley.room_z / SH_CHUNK_METERS);
        load_area_ex(game, game->area_index, cx, cz, 5);
        game->player_pos[0] = game->alley.room_x;
        game->player_pos[1] = 0.0f;
        game->player_pos[2] = game->alley.room_z;
        game->player_yaw = game->alley.room_yaw;
        game->alley.room_ready = false;
        game->alley.stage = SH_ALLEY_ROAM;
        game->alley.need_ground_snap = true;
        game->alley.fade_target = 0.0f;
        game->alley.fade_speed = 3.0f;
        window_job_start(game, cx, cz);
        game->phase = SH_PHASE_LOADING;
        return;
    }

    if (sh_alley_go_to_cafe(&game->alley)) {
        alley_to_cafe(game);
        return;
    }
    if (sh_cafe_leave_town(&game->cafe)) {
        leave_cafe_to_town(game);
        return;
    }
    if (sh_town_leave_overlay(&game->town)) {
        town_overlay_transition(game);
        return;
    }
    if (sh_cafe_game_over(&game->cafe) ||
        (town_active && game->save.health <= 0 && game->phase == SH_PHASE_MAP)) {
        /* Town death: reuse game-over screen (cafe path already fades). */
        if (town_active && game->save.health <= 0 && !sh_cafe_game_over(&game->cafe)) {
            sh_snd_stop_all();
            sh_inv_close(&game->inventory);
            sh_town_reset(&game->town);
            sh_cafe_reset(&game->cafe);
            sh_alley_reset(&game->alley);
            game->phase = SH_PHASE_GAME_OVER;
            game->last_tick_ms = osGetTime();
            return;
        }
        if (sh_cafe_game_over(&game->cafe)) {
            sh_snd_stop_all();
            sh_inv_close(&game->inventory);
            sh_cafe_reset(&game->cafe);
            sh_alley_reset(&game->alley);
            sh_town_reset(&game->town);
            game->phase = SH_PHASE_GAME_OVER;
            game->last_tick_ms = osGetTime();
            return;
        }
    }

    /* Status screen — SELECT (PS1 Item). Pauses gameplay / cafe scripts. */
    if (game->inventory.open) {
        sh_inv_update(&game->inventory, &game->save, input, dt);
        if (game->inventory.request_use_item) {
            const uint8_t item = game->inventory.request_use_item;
            game->inventory.request_use_item = 0;
            sh_town_item_use(&game->town, &game->save, game->player_pos[0], game->player_pos[2],
                             game->player_yaw, item);
        }
        if (game->inventory.request_map) {
            game->inventory.request_map = false;
            if (game->save.has_map || sh_save_event_get(&game->save, SH_EV_MAP)) {
                if (town_active) {
                    game->town.map_open = true;
                } else {
                    game->cafe.map_open = true;
                }
            }
        }
        /* Inventory_Update returns to gameplay on the next frame. */
        return;
    } else if (alley_active) {
        if (!game->alley.torch_active) {
            game->alley.torch_player.model.keyframe = game->harry_anim_kf;
            game->alley.torch_player.extra_model.keyframe = game->harry_anim_kf;
        }
        cafe_lock =
            sh_alley_update(&game->alley, &game->save, &game->player_pos[0], &game->player_pos[1], &game->player_pos[2],
                            &game->player_yaw, input, dt);
        if (game->alley.movie_requested) {
            open_movie(game, 16);
            return;
        }
        if (sh_alley_needs_grey_assets(&game->alley)) {
            load_npcs_alley_grey();
            sh_alley_clear_grey_assets_flag(&game->alley);
        }
        if (sh_alley_take_ground_snap(&game->alley)) {
            if (!try_place_at(game, game->player_pos[0], game->player_pos[2])) {
                /* Probe near the warp if the exact point has no floor. */
                const float ox[] = {0.0f, 1.5f, -1.5f, 3.0f, -3.0f};
                const float oz[] = {0.0f, 1.5f, -1.5f, 3.0f, -3.0f};
                bool ok = false;
                for (size_t i = 0; i < sizeof(ox) / sizeof(ox[0]) && !ok; i++) {
                    for (size_t j = 0; j < sizeof(oz) / sizeof(oz[0]) && !ok; j++) {
                        ok = try_place_at(game, game->player_pos[0] + ox[i], game->player_pos[2] + oz[j]);
                    }
                }
            }
        }
        map_open = false;
    } else if (town_active) {
        sh_school_seed_harry_keyframe(&game->town, game->harry_anim_kf);
        sh_school_seed_harry_position(&game->town, game->player_pos[0], game->player_pos[1], game->player_pos[2]);
        if (game->town.stage == SH_TOWN_SCRIPT && game->town.script == SH_TOWN_SCR_SCHOOL &&
            game->town.school.overlay == SH_MAP_IDX_MAP1_S02 && game->town.school.callback == 14) {
            ShCollHit ground;
            if (sh_coll_ground_near(&game->collision, game->player_pos[0], game->player_pos[2], game->player_pos[1], &ground)) {
                game->town.school.phone.ground_type = ground.ground_type;
            }
        }
        sh_school_phone_assets(s_plm_phone_size > 0 && s_phone_tim_ready,
            (game->save.equipped_weapon != SH_ITEM_HANDGUN || s_plm_handgun_size > 0) &&
            (game->save.equipped_weapon != SH_ITEM_POCKET_KNIFE || s_plm_knife_size > 0));
        if (game->save.map_idx == SH_MAP_IDX_MAP1_S03 && s_harry_hand_bone_valid) {
            const int32_t position[3] = {SH_Q12(game->player_pos[0]), -SH_Q12(game->player_pos[1]), -SH_Q12(game->player_pos[2])};
            const int yaw = SH_Q12(-game->player_yaw / (2.0f * 3.14159265358979323846f));
            ShCharaBone hand;
            sh_drain_valve_world_bone(&s_harry_hand_bone, position, yaw, &hand);
            sh_school_seed_harry_hand(&game->town, &hand);
        } else {
            sh_school_seed_harry_hand(&game->town, NULL);
        }
        cafe_lock =
            sh_town_update(&game->town, &game->save, &game->player_pos[0], &game->player_pos[1], &game->player_pos[2],
                           &game->player_yaw, input, dt);
        sh_town_snap_enemy_heights(&game->town, &game->collision);
        if (sh_town_take_ground_snap(&game->town)) {
            if (!try_place_at(game, game->player_pos[0], game->player_pos[2])) {
                /* Nearest ring first so a failed probe can't land across a wall. */
                const float x0 = game->player_pos[0];
                const float z0 = game->player_pos[2];
                for (float r = 0.25f; r <= 3.0f; r += 0.25f) {
                    for (int k = 0; k < 8; k++) {
                        const float a = (float)k * 0.785398f;
                        if (try_place_at(game, x0 + r * cosf(a), z0 + r * sinf(a))) {
                            goto town_snap_done;
                        }
                    }
                }
            }
        town_snap_done:;
        }
        map_open = sh_town_map_open(&game->town);
    } else {
        /* Always tick cafe scripts (map peek / bird) even while the paper map is up. */
        cafe_lock =
            sh_cafe_update(&game->cafe, &game->save, &game->player_pos[0], &game->player_pos[1], &game->player_pos[2],
                           &game->player_yaw, &cafe_radio, input, dt);
        map_open = sh_cafe_map_open(&game->cafe);
        if (map_open) {
            if (sh_input_down(input, SH_BTN_MAP) || sh_input_down(input, SH_BTN_CANCEL)) {
                sh_cafe_toggle_map(&game->cafe, &game->save);
                map_open = sh_cafe_map_open(&game->cafe);
            }
            /* Paper map freezes tank controls like the original Item map view. */
            cafe_lock = true;
        }
    }

    if (game->cafe.save_requested || game->town.save_requested) {
        game->save.location_id = game->cafe.save_requested ? game->cafe.save_location : game->town.save_location;
        game->cafe.save_requested = false;
        game->town.save_requested = false;
        open_save_menu(game, true);
        return;
    }

    const bool aiming = !alley_active && (sh_cafe_aiming(&game->cafe) || sh_town_aiming(&game->town));

    /* SysState_Gameplay_Update gives map events priority over the status menu. */
    const bool free_map = map_open &&
        ((town_active && game->town.stage == SH_TOWN_EXPLORE &&
          !game->town.pickup_active && !game->town.prompt_open) ||
         (cafe_active && !game->cafe.prompt_open && !game->cafe.pickup_active &&
          (game->cafe.stage == SH_CAFE_EXPLORE || game->cafe.stage == SH_CAFE_DONE ||
           game->cafe.stage == SH_CAFE_BIRD_FIGHT)));
    if (!game->inventory.open && (!cafe_lock || free_map)) {
        if (sh_input_down(input, SH_BTN_INVENTORY)) {
            if (map_open) {
                game->cafe.map_open = false;
                game->town.map_open = false;
            }
            sh_inv_open(&game->inventory, &game->save);
            if (town_active) {
                sh_town_item_triggers(&game->town, &game->save, game->player_pos[0], game->player_pos[2],
                                      game->player_yaw, game->inventory.available_use_items,
                                      (int)sizeof(game->inventory.available_use_items));
            }
            /* Inventory_Update / Game_RadioSoundStop. */
            sh_snd_stop(SH_SFX_RADIO_INTERFERENCE);
            game->radio.noise_on = false;
            return;
        }
    }

    if (!cafe_lock && !game->inventory.open && !sh_cafe_map_open(&game->cafe)) {
        if (sh_input_down(input, SH_BTN_MAP)) {
            if (game->save.has_map || sh_save_event_get(&game->save, SH_EV_MAP)) {
                sh_cafe_toggle_map(&game->cafe, &game->save);
            }
        }
        if (sh_input_down(input, SH_BTN_LIGHT)) {
            sh_save_flashlight_toggle(&game->save);
        }
        fwd = input->circle_y / 156.0f;
        float turn = input->circle_x / 156.0f;
        if (fabsf(fwd) < 0.12f) {
            fwd = 0.0f;
        }
        if (fabsf(turn) < 0.12f) {
            turn = 0.0f;
        }
        /* Player_Controller: digital directions take over when the stick is neutral. */
        if (fwd == 0.0f && turn == 0.0f && (alley_active || cafe_active || town_active)) {
            fwd = (sh_input_held(input, KEY_DUP) ? 1.0f : 0.0f) -
                  (sh_input_held(input, KEY_DDOWN) ? 1.0f : 0.0f);
            turn = (sh_input_held(input, KEY_DRIGHT) ? 1.0f : 0.0f) -
                   (sh_input_held(input, KEY_DLEFT) ? 1.0f : 0.0f);
        }
        if (aiming) {
            fwd = 0.0f;
            turn *= 0.8f;
        }
        /* Type A tank: stick Y = forward along Harry, stick X = turn.
         * Camera cuts must NOT remap stick directions (PS1 Type A). */
        game->player_yaw -= turn * SH_TURN_SPEED * 1.4f * dt;

        /* Debug noclip: START+L+R */
        if (development_enabled() && sh_input_held(input, KEY_START) && sh_input_held(input, KEY_L) && sh_input_held(input, KEY_R) &&
            (sh_input_down(input, KEY_L) || sh_input_down(input, KEY_R))) {
            game->noclip = !game->noclip;
        }

        /* Type A: Run = Square→L, Aim = R2→R (mutually exclusive). */
        want_run = !game->noclip && !aiming && sh_input_held(input, SH_BTN_RUN);
        const float step = (want_run ? SH_RUN_SPEED : SH_WALK_SPEED) * dt;
        const float s = sinf(game->player_yaw);
        const float c = cosf(game->player_yaw);
        const float dx = -s * fwd * step;
        const float dz = -c * fwd * step;

        if (!game->noclip && !sh_alley_skip_collision(&game->alley)) {
            sh_coll_move(&game->collision, &game->player_pos[0], &game->player_pos[1], &game->player_pos[2], dx, dz,
                         SH_COLL_PLAYER_RADIUS, 0.0f);
        } else {
            game->player_pos[0] += dx;
            game->player_pos[2] += dz;
            if (game->noclip) {
                if (sh_input_held(input, KEY_R)) {
                    game->player_pos[1] += SH_LIFT_SPEED * dt;
                }
                if (sh_input_held(input, KEY_L)) {
                    game->player_pos[1] -= SH_LIFT_SPEED * dt;
                }
            }
        }
    }

    {
        const bool has_radio = sh_save_item_count(&game->save, SH_ITEM_RADIO) > 0;
        const bool radio_on = sh_save_radio_on(&game->save);
        float danger = 0.0f;
        if (game->cafe.stage == SH_CAFE_BIRD_INTRO || game->cafe.stage == SH_CAFE_BIRD_FIGHT) {
            danger = 1.0f;
        }
        bool noise_needed = false;
        if (sh_town_active(&game->town) && !game->inventory.open && !map_open) {
            /* Game_NpcUpdate radio noise: exterior Q4 distance / 2, clamped to 255. */
            float best = 255.0f;
            bool any = false;
            for (int i = 0; i < game->town.dog_count; i++) {
                const ShTownDog *d = &game->town.dogs[i];
                if (!d->alive || d->hp <= 0.0f) {
                    continue;
                }
                const float dx = d->pos[0] - game->player_pos[0], dz = d->pos[2] - game->player_pos[2];
                /* Game_NpcUpdate initializes detection distance to SQUARE(32). */
                if (dx * dx + dz * dz >= 32.0f * 32.0f) {
                    continue;
                }
                const float att = sqrtf(dx * dx + dz * dz) * 8.0f;
                best = att < best ? att : best;
                any = true;
            }
            danger = 1.0f - best / 255.0f;
            if (any && has_radio && radio_on) {
                noise_needed = true;
                if (!game->radio.noise_on) {
                    sh_snd_call(SH_SFX_RADIO_INTERFERENCE);
                    game->radio.noise_on = true;
                }
                sh_sfx_update(SH_SFX_RADIO_INTERFERENCE, 0, (uint8_t)best, 0);
            }
        }
        if (!noise_needed && game->radio.noise_on) {
            sh_snd_stop(SH_SFX_RADIO_INTERFERENCE);
            game->radio.noise_on = false;
        }
        game->radio_time += dt;
        if (cafe_radio >= 0.0f && radio_on) {
            game->radio.intensity = cafe_radio;
        } else {
            sh_radio_update(&game->radio, has_radio, radio_on, danger, dt);
        }
        /* Player_LogicUpdate does not run in the inventory or paper map states. */
        if (!game->inventory.open && !map_open) {
            sh_save_tick(&game->save, dt, 0.0f, false);
        }
    }

    update_harry_anim(game, cafe_lock ? 0.0f : fwd, want_run, dt);
    update_npc_anims(game);

    /* Cafe 5to2 floor is flat (y = 0): stair probes near the door frame must not lift Harry. */
    if (cafe_active && !game->noclip) {
        ShCollHit hit;
        float y = 0.0f;
        if (sh_coll_ground(&game->collision, game->player_pos[0], game->player_pos[2], &hit) &&
            fabsf(hit.height) < 0.25f) {
            y = hit.height;
        }
        game->player_pos[1] = y;
    }

    if (!(cafe_active && game->cafe.cam_active) && !(alley_active && game->alley.cam_active) &&
        !(town_active && game->town.cam_active)) {
        update_chase_camera(game);
    }
    game->camera.fov_deg = 0.0f;
    {
        ShCamera cut;
        const bool alley_camera = sh_alley_gameplay_camera(&game->alley, game->player_pos,
                                                          game->player_yaw, dt, &cut);
        if (sh_alley_camera(&game->alley, &cut) || alley_camera) {
            game->camera = cut;
        } else if (sh_town_camera(&game->town, &cut)) {
            game->camera = cut;
        } else if (sh_cafe_camera(&game->cafe, &cut)) {
            game->camera = cut;
        }
    }
}

static void update_map(ShGame *game, const ShInputState *input, float dt)
{
    sh_coll_trigger_flags_set(&game->collision,
        sh_school_collision_flags(&game->town, &game->save, game->player_pos[0], game->player_pos[2]));
    const bool cafe_active = game->cafe.stage != SH_CAFE_OFF;
    const bool alley_active = sh_alley_active(&game->alley);
    const bool town_active = sh_town_active(&game->town);

    /* Debug area hopping disabled during alley / cafe / town scripts. */
    if (development_enabled() && !cafe_active && !alley_active && !town_active) {
        if (sh_input_down(input, KEY_DRIGHT)) {
            load_area(game, game->area_index + 1);
        } else if (sh_input_down(input, KEY_DLEFT)) {
            load_area(game, game->area_index - 1);
        } else if (sh_input_down(input, KEY_DUP) || sh_input_down(input, KEY_DDOWN)) {
            if (game->chunk_count > 0) {
                int current = 0;
                for (int i = 0; i < game->chunk_count; i++) {
                    if (game->chunks[i].x == game->window_x && game->chunks[i].z == game->window_z) {
                        current = i;
                        break;
                    }
                }
                const int step = sh_input_down(input, KEY_DUP) ? 1 : -1;
                const int next = (current + step + game->chunk_count) % game->chunk_count;
                place_player_in_chunk(game, &game->chunks[next]);
            }
        }
    }

    if (!game->area_loaded) {
        return;
    }

    update_player(game, input, dt);
    if (game->phase != SH_PHASE_MAP) { return; }
    if (game->save.map_idx == SH_MAP_IDX_MAP1_S03 && sh_school_roof_water(&game->town)) {
        if (!game->water_ready) { game->water_ready = load_water_atlas(); }
        if (!game->water_ready) {
            game->load_error = "L'effet d'eau n'a pas pu etre charge.\nDiagnostic SD : sh3ds_load.log";
            game->phase = SH_PHASE_LOADING;
            return;
        }
        ShEnvLight light;
        const bool have_light = environment_light_from_harry(game, &light);
        const int yaw = SH_Q12(-game->camera.yaw / (2.0f * 3.14159265358979323846f));
        sh_school_roof_water_update(&game->town, yaw, &game->town.environment.current, have_light ? &light : NULL, dt);
    } else if (game->water_ready && game->save.map_idx != SH_MAP_IDX_MAP1_S03) {
        sh_gfx_set_water_atlas(NULL);
        game->water_ready = false;
    }

    const int cx = (int)floorf(game->player_pos[0] / SH_CHUNK_METERS);
    const int cz = (int)floorf(-game->player_pos[2] / SH_CHUNK_METERS);
    ShWindowJob *job = &game->job;
    const unsigned world_key = sh_game_world_key(game);
    const bool shown = game->window_valid && cx == game->window_x && cz == game->window_z &&
                       world_key == game->window_world_key;
    const bool queued = job->stage != SH_WINDOW_IDLE && cx == job->cx && cz == job->cz && world_key == job->world_key;
    if (!shown && !queued) {
        window_job_start(game, cx, cz);
    }
    /* The old window keeps being drawn meanwhile; only block when there is nothing to show. */
    window_job_run(game, !game->window_valid);
}

/* Listener, weapon VAB and Bgm_Update for the frame (after scripts set bgmStatusFlags). */
static void update_sound(ShGame *game, float dt)
{
    sh_snd_set_listener(&game->camera, game->player_pos);
    sh_snd_weapon(game->save.equipped_weapon);
    if (game->inventory.open) {
        sh_bgm_status_set(SH_BGM_STATUS_DUCK); /* Bgm_MenuUpdate */
    }
    if (sh_town_active(&game->town)) {
        sh_town_bgm_update(&game->town, &game->save, game->save.health <= 0, dt);
    } else {
        sh_cafe_bgm_update(&game->cafe, &game->save, game->save.health <= 0, dt);
    }
}

/* ---- Shell ------------------------------------------------------------- */

bool sh_game_init(ShGame *game)
{
    if (!game) {
        return false;
    }

    memset(game, 0, sizeof(*game));
    game->phase = SH_PHASE_BOOT;
    game->last_tick_ms = osGetTime();
    game->cam_pitch = 0.15f;

    sh_arena_reset(sh_memory_level_arena());
    sh_arena_reset(sh_memory_stream_arena());
    window_storage_init(game);
    sh_save_init(&game->save);
    sh_radio_init(&game->radio);
    sh_alley_reset(&game->alley);
    sh_cafe_reset(&game->cafe);
    sh_town_reset(&game->town);
    sh_inv_init(&game->inventory);
    game->title_sel = 0;
    scan_textures(game);
    scan_areas(game);
    sh_assets_log("scan textures=%d areas=%d", game->texture_count, game->area_count);
    load_harry(game);
    load_npcs();
    load_pickup_items();
    s_bg_item_size = 0;
    read_file_into(SH_BG_DIR "/BG_ITEM.PLM", s_bg_item, sizeof(s_bg_item), &s_bg_item_size);
    sh_arena_reset(sh_memory_level_arena());
    return true;
}

void sh_game_shutdown(ShGame *game)
{
    if (!game) {
        return;
    }
    if (game->movie.file) {
        sh_sound_movie_end();
        sh_movie_close(&game->movie);
    }
    sh_arena_reset(sh_memory_level_arena());
    sh_arena_reset(sh_memory_frame_arena());
    game->phase = SH_PHASE_EXIT;
}

static void return_to_title(ShGame *game)
{
    sh_school_roof_water_reset();
    if (game->water_ready) { sh_gfx_set_water_atlas(NULL); game->water_ready = false; }
    sh_alley_reset(&game->alley);
    sh_cafe_reset(&game->cafe);
    sh_town_reset(&game->town);
    sh_inv_close(&game->inventory);
    sh_snd_stop_all();
    sh_snd_reset();
    sh_bgm_crossfade_reset();
    sh_radio_init(&game->radio);
    game->job.stage = SH_WINDOW_IDLE;
    game->loading_ambient = false;
    game->noclip = false;
    game->phase = SH_PHASE_TITLE;
}

void sh_game_update(ShGame *game, const ShInputState *input)
{
    if (!game || !input) {
        return;
    }

    const uint64_t now = osGetTime();
    float dt = (float)(now - game->last_tick_ms) / 1000.0f;
    game->last_tick_ms = now;
    if (dt > 0.1f) {
        dt = 0.1f;
    }
    if (dt > 0.0f) {
        game->fps += (1.0f / dt - game->fps) * 0.1f;
    }

    sh_arena_reset(sh_memory_frame_arena());
    game->frame++;
    sh_bgm_frame_begin();

    if (development_enabled() && (game->phase == SH_PHASE_MAP || game->phase == SH_PHASE_LOADING ||
         game->phase == SH_PHASE_GAME_OVER) &&
        sh_input_held(input, KEY_START) && sh_input_held(input, KEY_SELECT) &&
        sh_input_down(input, KEY_START | KEY_SELECT)) {
        return_to_title(game);
        return;
    }

    /* SELECT quits from menus; in map mode it uses a health item instead. */
    if (sh_input_down(input, KEY_SELECT) && game->phase != SH_PHASE_MAP && game->phase != SH_PHASE_SAVE &&
        game->phase != SH_PHASE_MOVIE) {
        game->wants_exit = true;
        game->phase = SH_PHASE_EXIT;
        return;
    }

    switch (game->phase) {
    case SH_PHASE_BOOT:
        if (game->frame > 45 || sh_input_down(input, KEY_A) || sh_input_down(input, KEY_START)) {
            game->phase = SH_PHASE_TITLE;
        }
        break;
    case SH_PHASE_TITLE: {
        const int title_count = development_enabled() ? 10 + sh_debug_checkpoint_count() : 2;
        if (sh_input_down(input, KEY_DUP)) {
            game->title_sel = (game->title_sel + title_count - 1) % title_count;
        } else if (sh_input_down(input, KEY_DDOWN)) {
            game->title_sel = (game->title_sel + 1) % title_count;
        } else if (sh_input_down(input, KEY_A) || sh_input_down(input, KEY_START)) {
            if (game->title_sel == title_count - 1) {
                open_save_menu(game, false);
                break;
            }
            switch (game->title_sel) {
            case 0:
                start_new_game_full(game);
                break;
            case 1:
                start_skip_cafe(game);
                break;
            case 2:
                start_skip_cafe_explore(game);
                break;
            case 3:
                start_skip_bird_fight(game);
                break;
            case 4:
                start_skip_post_cafe(game);
                break;
            case 5:
                start_skip_sketch_gate(game);
                break;
            case 6:
                start_skip_after_sketch(game, false);
                break;
            case 7:
                start_skip_after_sketch(game, true);
                break;
            case 8:
                start_skip_doghouse_note(game);
                break;
            default:
                start_debug_checkpoint(game, game->title_sel - 9);
                break;
            }
        } else if (development_enabled() && sh_input_down(input, KEY_Y)) {
            game->phase = SH_PHASE_VIEWER;
            load_texture(game, game->texture_index);
        }
        break;
    }
    case SH_PHASE_LOADING:
        if (!game->area_loaded && !game->load_error) {
            game->load_error = "La zone n'a pas pu etre chargee.\nRelance le fichier .3dsx complet.\nDiagnostic SD : sh3ds_load.log";
        }
        if (game->load_error) {
            if (sh_input_down(input, KEY_A | KEY_B | KEY_START)) {
                return_to_title(game);
            }
            break;
        }
        /* Spread window build across frames so the loading screen can redraw. */
        if (game->job.stage != SH_WINDOW_IDLE) {
            window_job_run(game, false);
        }
        if (game->loading_ambient) {
            if (sh_sound_streaming() != 0) {
                break;
            }
            game->loading_ambient = false;
        }
        if (game->window_valid) {
            game->phase = SH_PHASE_MAP;
        } else if (game->job.stage == SH_WINDOW_IDLE) {
            game->load_error = "La construction de la zone a echoue.\nRetourne au menu pour reessayer.";
        }
        break;
    case SH_PHASE_MOVIE: {
        const uint32_t elapsed = (uint32_t)(now - game->movie_start_ms);
        if (sh_movie_step(&game->movie, elapsed, sh_input_down(input, SH_BTN_SKIP))) {
            sh_sound_movie_end();
            sh_movie_close(&game->movie);
            if (game->movie.failed) {
                sh_assets_log("movie frame read failed");
                game->load_error = "Les ressources ne sont plus lisibles.\nFerme le jeu puis relance-le.\nDiagnostic SD : sh3ds_load.log";
                game->phase = SH_PHASE_LOADING;
            } else {
                game->alley.movie_finished = true;
                game->phase = SH_PHASE_MAP;
            }
        }
        break;
    }
    case SH_PHASE_SAVE: {
        sh_bgm_status_set(SH_BGM_STATUS_PAUSE);
        const ShSaveMenuAction action = sh_save_menu_update(&game->save_menu, input, dt);
        if (action == SH_SAVE_MENU_LOADED) {
            if (!restore_savegame(game, &game->save_menu.snapshot)) {
                game->save_menu.loaded = false;
                game->save_menu.result = SH_SAVEFILE_INVALID;
                game->save_menu.step = SH_SAVE_MENU_STATUS;
            }
        } else if (action == SH_SAVE_MENU_CLOSED) {
            if (game->save_menu.writing && sh_save_event_get(&game->save_menu.snapshot.save, 186)) {
                sh_save_event_set(&game->save, 186);
            }
            game->cafe.controls_locked = false;
            game->town.controls_locked = false;
            game->phase = game->save_return_phase;
        }
        break;
    }
    case SH_PHASE_VIEWER:
        if (sh_input_down(input, KEY_START) || sh_input_down(input, KEY_B)) {
            game->phase = SH_PHASE_TITLE;
        } else if (sh_input_down(input, KEY_DRIGHT | KEY_R)) {
            load_texture(game, game->texture_index + 1);
        } else if (sh_input_down(input, KEY_DLEFT | KEY_L)) {
            load_texture(game, game->texture_index - 1);
        } else if (sh_input_down(input, KEY_DDOWN)) {
            load_texture(game, game->texture_index + 10);
        } else if (sh_input_down(input, KEY_DUP)) {
            load_texture(game, game->texture_index - 10);
        }
        break;
    case SH_PHASE_MAP:
        if (sh_input_down(input, KEY_START) && game->cafe.stage == SH_CAFE_OFF &&
            !sh_alley_active(&game->alley) && !sh_town_active(&game->town)) {
            return_to_title(game);
        } else {
            update_map(game, input, dt);
            if (game->save.map_idx == SH_MAP_IDX_MAP1_S03 || game->save.map_idx == SH_MAP_IDX_MAP1_S02) {
                const uint32_t moving = sh_school_roof_moving_mask(&game->town, &game->save, game->player_pos[0], game->player_pos[2]);
                if (moving && !sh_gfx_prepare_moving_object(moving)) {
                    game->load_error = "Le modele anime n'a pas pu etre prepare.\nDiagnostic SD : sh3ds_load.log";
                    game->phase = SH_PHASE_LOADING;
                }
            }
            if (game->town.school.silver_active) {
                int count;
                const ShSchoolProp *props = sh_school_map_props(game->save.map_idx, &count);
                if (count <= 6 || !sh_gfx_prepare_moving_object(props[6].mask)) {
                    game->load_error = "Le modele anime n'a pas pu etre prepare.\nDiagnostic SD : sh3ds_load.log";
                    game->phase = SH_PHASE_LOADING;
                }
            }
            update_sound(game, dt);
        }
        break;
    case SH_PHASE_GAME_OVER:
        if (sh_input_down(input, KEY_A) || sh_input_down(input, KEY_START) || sh_input_down(input, KEY_B)) {
            game->phase = SH_PHASE_TITLE;
        }
        break;
    case SH_PHASE_EXIT:
    default:
        break;
    }
}

static void render_viewer(const ShGame *game)
{
    char counter[24];
    char info[128];

    if (game->texture_count == 0) {
        sh_gfx_draw_viewer("Aucune texture",
                           "romfs:/game/tim est vide.\nLance scripts/prepare_assets.ps1\npuis recompile.",
                           "SELECT: quitter");
        return;
    }

    snprintf(counter, sizeof(counter), "%d / %d", game->texture_index + 1, game->texture_count);
    if (game->texture_loaded) {
        snprintf(info, sizeof(info), "Texture %s\n%dx%d, %d bits\nmemoire: %u Ko",
                 counter, game->texture.width, game->texture.height, game->texture.bpp,
                 (unsigned)(sh_arena_used(sh_memory_level_arena()) / 1024u));
    } else {
        snprintf(info, sizeof(info), "Texture %s\nformat non pris en charge", counter);
    }

    sh_gfx_draw_viewer(game->texture_names[game->texture_index], info,
                       "Gauche/Droite: suivante  Haut/Bas: +-10\nSTART: maps 3D   SELECT: quitter");
}

static void render_map(const ShGame *game)
{
    char title[48];
    char info[320];
    char hint_buf[96];
    char inv_top[192];
    const char *cafe_dlg = sh_cafe_dialogue(&game->cafe);
    const char *cafe_hint = sh_cafe_hint(&game->cafe);
    const char *alley_dlg = sh_alley_dialogue(&game->alley);
    const char *alley_hint = sh_alley_hint(&game->alley);
    const char *town_dlg = sh_town_dialogue(&game->town);
    const char *town_hint = sh_town_hint(&game->town);
    const bool alley_active = sh_alley_active(&game->alley);
    const bool town_active = sh_town_active(&game->town);
    const bool cafe_active = game->cafe.stage != SH_CAFE_OFF;
    const bool inv_open = game->inventory.open;

    if (inv_open) {
        sh_inv_format_hint(&game->inventory, hint_buf, sizeof(hint_buf));
    } else if (alley_active && alley_hint[0]) {
        snprintf(hint_buf, sizeof(hint_buf), "%s", alley_hint);
    } else if (town_active && town_hint[0]) {
        snprintf(hint_buf, sizeof(hint_buf), "%s", town_hint);
    } else if (cafe_active && cafe_hint[0]) {
        snprintf(hint_buf, sizeof(hint_buf), "%s", cafe_hint);
    } else if (game->noclip) {
        snprintf(hint_buf, sizeof(hint_buf), "NOCLIP  Stick: fly  L/R: height\nSTART+L+R: collisions");
    } else {
        snprintf(hint_buf, sizeof(hint_buf),
                 "L:run R:aim A:action/attack\nY:map SELECT:status X:light");
    }

    ShPlayerPose player = {0};
    player.visible = game->harry_ready && game->area_loaded && !inv_open;
    if (sh_school_map_is_school(game->save.map_idx) && game->town.school.player_hidden) { player.visible = false; }
    player.pos[0] = game->player_pos[0];
    player.pos[1] = game->player_pos[1];
    player.pos[2] = game->player_pos[2];
    player.yaw = game->player_yaw;

    ShActorPose actors[SH_GFX_MAX_ACTORS];
    int actor_count = 0;
    refresh_pickup_item(game, inv_open ? NULL : &game->camera);
    if (!inv_open) {
        if (alley_active) {
            for (int slot = 0; slot < SH_GFX_MAX_ACTORS; slot++) {
                if (sh_alley_actor_pose(&game->alley, slot, &actors[actor_count])) {
                    actor_count++;
                }
            }
        } else if (town_active) {
            for (int slot = 0; slot < SH_GFX_MAX_ACTORS; slot++) {
                if (sh_town_actor_pose(&game->town, slot, &actors[actor_count])) {
                    actor_count++;
                }
            }
        } else {
            for (int slot = 0; slot < SH_GFX_MAX_ACTORS; slot++) {
                if (sh_cafe_actor_pose(&game->cafe, slot, &actors[actor_count])) {
                    actor_count++;
                }
            }
        }
    }

    ShMapHud hud = {0};
    if (town_active) {
        hud.smoke = &game->town.school.smoke;
        hud.piano_cursor = game->town.school.piano_result.draw_cursor;
        hud.piano_cursor_x = game->town.school.piano_result.cursor_x;
        hud.piano_cursor_y = game->town.school.piano_result.cursor_y;
        hud.retail_borders = (game->town.border.state & SH_BORDER_ACTIVE) != 0;
        hud.border_shade = game->town.border.shade;
    }
    if (town_active && sh_school_map_is_school(game->save.map_idx)) {
        hud.world_object_mask = sh_school_prop_mask(&game->town, &game->save,
                                                    game->player_pos[0], game->player_pos[2]);
        if (game->save.map_idx == SH_MAP_IDX_MAP1_S02) {
            int count;
            const ShSchoolProp *props = sh_school_map_props(game->save.map_idx, &count);
            if (count > 0) {
                hud.moving_y_mask = props[0].mask;
                hud.moving_y_rotation = sh_school_bathroom_rotation(&game->town, &game->save);
                memcpy(hud.moving_y_pivot, props[0].pos, sizeof(hud.moving_y_pivot));
                hud.moving_object_mask = sh_school_roof_moving_mask(&game->town, &game->save, game->player_pos[0], game->player_pos[2]);
                if (count >= 6) {
                    hud.moving_rotations = game->town.school.boss_doors.rotation;
                    for (int door = 0; door < 2; door++) {
                        hud.moving_locker_masks[door] = props[door + 4].mask;
                        memcpy(hud.moving_locker_pivots[door], props[door + 4].pos, sizeof(props[door + 4].pos));
                    }
                }
                if (count >= 7) {
                    hud.moving_vertical_mask = props[6].mask;
                    hud.moving_vertical_q8 = game->town.school.boss_box_y >> 4;
                }
            }
        }
        if (game->save.map_idx == SH_MAP_IDX_MAP1_S03 && game->town.school.valve_active) {
            int count;
            const ShSchoolProp *props = sh_school_map_props(game->save.map_idx, &count);
            if (count > 0) {
                hud.moving_object_mask = props[0].mask;
                hud.moving_valve = &game->town.school.valve;
                hud.moving_valve_mask = props[0].mask;
                memcpy(hud.moving_valve_pivot, props[0].pos, sizeof(hud.moving_valve_pivot));
            }
        }
        if (game->save.map_idx == SH_MAP_IDX_MAP1_S03 && game->town.school.locker_render_active) {
            int count;
            const ShSchoolProp *props = sh_school_map_props(game->save.map_idx, &count);
            if (count >= 5) {
                hud.moving_rotations = game->town.school.locker.rotation;
                for (int door = 0; door < 3; door++) {
                    hud.moving_locker_masks[door] = props[door + 2].mask;
                    memcpy(hud.moving_locker_pivots[door], props[door + 2].pos, sizeof(props[door + 2].pos));
                }
            }
        }
        if (game->save.map_idx == SH_MAP_IDX_MAP1_S03) {
            hud.moving_object_mask = sh_school_roof_moving_mask(&game->town, &game->save, game->player_pos[0], game->player_pos[2]);
        }
        if (game->town.school.silver_active) {
            int count;
            const ShSchoolProp *props = sh_school_map_props(game->save.map_idx, &count);
            if (count > 6) {
                hud.moving_object_mask = props[6].mask;
                hud.moving_silver = &game->town.school.silver;
            }
        }
    }
    if (town_active && game->town.environment.active) {
        hud.environment = &game->town.environment.current;
        if (environment_light_from_harry(game, &s_environment_light)) {
            hud.environment_light = &s_environment_light;
        }
    }
    if (alley_active && game->alley.environment.active) {
        hud.environment = &game->alley.environment.current;
        hud.retail_borders = (game->alley.border.state & SH_BORDER_ACTIVE) != 0;
        hud.border_shade = game->alley.border.shade;
    }
    hud.hint = hint_buf;
    if (game->save.map_idx == SH_MAP_IDX_MAP1_S03 && game->water_ready) {
        const ShRoofWater *water = sh_school_roof_water(&game->town);
        if (water) {
            hud.water = &water->pool;
            hud.water_config = &water->config;
            hud.water_visibility = &water->visibility;
            hud.water_spray = water->spray;
            hud.water_spray_count = water->ball ? 3 : 2;
            hud.water_surface = water->surface;
            hud.water_surface_count = water->surface_count;
        }
    }
    if (!game->area_loaded) {
        hud.title = "Echec du chargement";
        hud.info = game->load_error ? game->load_error : "Relance le fichier .3dsx complet.";
        sh_gfx_draw_map(NULL, NULL, NULL, 0, &hud);
        return;
    }

    const ShSavegame *save = &game->save;
    if (inv_open) {
        snprintf(title, sizeof(title), "STATUS");
        sh_inv_format_top(&game->inventory, save, inv_top, sizeof(inv_top));
        sh_inv_format_bottom(&game->inventory, save, info, sizeof(info));
        hud.subtitle = inv_top;
        hud.letterbox = true;
        hud.fade = 0.35f;
    } else {
        snprintf(title, sizeof(title),
                 alley_active || town_active ? "Old Silent Hill"
                 : cafe_active               ? "Cafe 5to2 - Silent Hill"
                                             : "Old Silent Hill");
        const char *weapon = save->equipped_weapon == SH_ITEM_HANDGUN        ? "Handgun"
                             : save->equipped_weapon == SH_ITEM_POCKET_KNIFE ? "Kitchen knife"
                                                                              : "--";
        const char *dlg = alley_active ? alley_dlg : town_active ? town_dlg : cafe_dlg;
        if (dlg && dlg[0]) {
            if ((!alley_active && game->cafe.stage == SH_CAFE_BIRD_FIGHT) ||
                (town_active && save->equipped_weapon == SH_ITEM_HANDGUN)) {
                snprintf(info, sizeof(info), "%s\n\nHP %.0f   %s   ammo %d", dlg, sh_save_health_f(save), weapon,
                         sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS));
            } else {
                snprintf(info, sizeof(info), "%s\n\nHP %.0f   %s", dlg, sh_save_health_f(save), weapon);
            }
        } else {
            snprintf(info, sizeof(info), "HP %.0f   %s%s\nFlashlight %s   Radio %s", sh_save_health_f(save), weapon,
                     save->equipped_weapon == SH_ITEM_HANDGUN
                         ? (sh_save_item_count(save, SH_ITEM_HANDGUN_BULLETS) > 0 ? "" : " (empty)")
                         : "",
                     sh_save_flashlight_on(save) ? "On" : "Off", sh_save_radio_on(save) ? "On" : "Off");
        }
        {
            /* Debug position, same order as the autotest "warp X Z YAW" command. */
            const size_t len = strlen(info);
            snprintf(info + len, sizeof(info) - len, "\n\nPOS %.1f %.1f %.0f  y%.2f", game->player_pos[0],
                     game->player_pos[2], game->player_yaw * 57.2958f, game->player_pos[1]);
        }
        if (alley_active) {
            hud.subtitle = sh_alley_subtitle(&game->alley);
            hud.letterbox = sh_alley_letterbox(&game->alley);
            hud.fade = sh_alley_fade(&game->alley);
        } else if (town_active) {
            hud.subtitle = sh_town_subtitle(&game->town);
            if (game->town.school.choice_active) {
                hud.choice = &game->town.school.choice;
                for (unsigned row = 0; row < 3; row++) {
                    hud.choice_options[row] = game->town.school.choice_options[row];
                }
            }
            hud.letterbox = sh_town_letterbox(&game->town);
            hud.fade = sh_town_fade(&game->town);
        } else {
            hud.subtitle = sh_cafe_subtitle(&game->cafe);
            hud.letterbox = sh_cafe_letterbox(&game->cafe);
            hud.fade = sh_cafe_fade(&game->cafe);
        }
        {
            const float gf_cafe = sh_cafe_gun_flash(&game->cafe);
            const float gf_town = sh_town_gun_flash(&game->town);
            const float gf = gf_cafe > gf_town ? gf_cafe : gf_town;
            hud.gun_flash = gf > 0.0f ? (gf / 0.18f > 1.0f ? 1.0f : gf / 0.18f) : 0.0f;
        }
    }

    hud.title = title;
    hud.info = info;
    const char *bg_tim = inv_open ? NULL : sh_town_bg_tim(&game->town);
    if (bg_tim) {
        if (strcmp(s_sketch_tim_name, bg_tim) != 0) {
            char path[64];
            size_t sz = 0;
            snprintf(path, sizeof path, SH_TEXTURE_DIR "/%s", bg_tim);
            s_sketch_tim_ok = read_file_into(path, s_sketch_tim_file, sizeof(s_sketch_tim_file), &sz) &&
                              sh_tim_parse(s_sketch_tim_file, sz, &s_sketch_tim_parsed);
            snprintf(s_sketch_tim_name, sizeof s_sketch_tim_name, "%s", bg_tim);
        }
        if (s_sketch_tim_ok) {
            const ShTim *image = &s_sketch_tim_parsed;
            float alpha = 0.0f;
            const char *next = sh_town_bg_next_tim(&game->town, &alpha);
            if (next) {
                if (strcmp(s_event_next_name, next) != 0) {
                    char path[64];
                    size_t size = 0;
                    snprintf(path, sizeof path, SH_TEXTURE_DIR "/%s", next);
                    s_event_next_ok = read_file_into(path, s_event_next_file, sizeof(s_event_next_file), &size) &&
                                      sh_tim_parse(s_event_next_file, size, &s_event_next_tim);
                    snprintf(s_event_next_name, sizeof s_event_next_name, "%s", next);
                }
                const unsigned buffer = game->frame & 1u;
                if (s_event_next_ok && sh_event_image_transition(&s_event_blend_tim[buffer],
                    s_event_blend_pixels[buffer], sizeof(s_event_blend_pixels[buffer]), image, &s_event_next_tim, alpha)) {
                    image = &s_event_blend_tim[buffer];
                }
            }
            if (game->town.script == SH_TOWN_SCR_SCHOOL && game->town.school.brightness != 128) {
                const unsigned buffer = game->frame & 1u;
                if (sh_event_image_shade(&s_event_blend_tim[buffer], s_event_blend_pixels[buffer],
                    sizeof(s_event_blend_pixels[buffer]), image, game->town.school.brightness)) {
                    image = &s_event_blend_tim[buffer];
                }
            }
            sh_gfx_set_image(image);
            hud.overlay = image;
        }
        hud.letterbox = false;
    } else if (!inv_open && (sh_cafe_map_open(&game->cafe) || sh_town_map_open(&game->town))) {
        if (s_map_tim_ok) {
            sh_gfx_set_image_crop(&s_map_tim_parsed, 0, SH_PAPER_MAP_CROP_Y, SH_PAPER_MAP_VIEW_W,
                                  SH_PAPER_MAP_VIEW_H);
            hud.overlay = &s_map_tim_parsed;
            hud.fade = 0.0f; /* don't black out the paper map */
        } else {
            hud.subtitle = "Residential area map\n(TIM manquant — relance prepare_assets)";
        }
        hud.letterbox = false; /* full TIM, no bars covering it */
        sh_town_map_hud(&game->town, &game->save, game->player_pos[0], game->player_pos[2], game->player_yaw, &hud,
                        s_map_sprites, SH_MAP_MAX_SPRITES);
        if (s_map_tim_ok) {
            hud.subtitle = NULL;
        }
        if (sh_town_map_open(&game->town) &&
            (game->town.stage == SH_TOWN_EXIT_CS || game->town.stage == SH_TOWN_SCRIPT)) {
            hud.hint = sh_town_hint(&game->town);
            if (game->town.fade > 0.01f) {
                hud.fade = game->town.fade;
            }
            if (game->town.stage == SH_TOWN_SCRIPT) {
                hud.subtitle = sh_town_subtitle(&game->town);
                hud.letterbox = sh_town_letterbox(&game->town);
            }
        } else if (sh_town_map_open(&game->town)) {
            hud.hint = s_map_tim_ok ? "Y / B : close map" : "Y / B : fermer (map TIM absente)";
        }
    }
    if (!development_enabled()) {
        hud.title = inv_open ? title : "";
        hud.info = inv_open ? info : alley_active ? alley_dlg : town_active ? town_dlg : cafe_dlg;
        hud.hint = inv_open ? hint_buf : "";
    }
    sh_gfx_draw_map(&game->camera, &player, actors, actor_count, &hud);
}

void sh_game_render(const ShGame *game)
{
    char line3[96];

    if (!game) {
        return;
    }

    switch (game->phase) {
    case SH_PHASE_BOOT:
        snprintf(line3, sizeof(line3), "%d textures · %d zones", game->texture_count, game->area_count);
        sh_gfx_draw_boot_screen("SILENT HILL", "Welcome to Silent Hill...", line3);
        break;
    case SH_PHASE_TITLE: {
        if (!development_enabled()) {
            char menu[96];
            snprintf(menu, sizeof(menu), "%c %s\n%c %s", game->title_sel == 0 ? '>' : ' ',
                sh_save_title_label(2), game->title_sel == 1 ? '>' : ' ', sh_save_title_label(0));
            sh_gfx_draw_title_screen("SILENT HILL", "", "", menu);
            break;
        }
        static const char *const opts[] = {
            "New Game (intro rue + Cheryl)",
            "Skip → Cafe (Cybil)",
            "Skip → Cafe explore",
            "Skip → Combat Air Screamer",
            "Skip → Rue post-cafe",
            "Skip → Derriere la grille (carnet)",
            "Skip → Niche Levin St (apres carnet)",
            "Skip → Niche faite (note + cle)",
            "Skip → Note niche Levin St",
        };
        char menu[600];
        size_t len = 0;
        const int base_count = (int)(sizeof(opts) / sizeof(opts[0]));
        const int count = base_count + sh_debug_checkpoint_count() + 1;
        const int first = game->title_sel >= 8 ? game->title_sel - 7 : 0;
        for (int i = first; i < count && i < first + 9; i++) {
            const char *name = i == count - 1 ? sh_save_title_label(0) :
                i < base_count ? opts[i] : sh_debug_checkpoint_name(i - base_count);
            len += (size_t)snprintf(menu + len, sizeof(menu) - len, "%s%c %s", i > first ? "\n" : "",
                                    game->title_sel == i ? '>' : ' ', name);
        }
        snprintf(line3, sizeof(line3), "%d/%d - %s", game->title_sel + 1, count,
                 game->title_sel >= base_count ? "Objets et progression restaures" : "Y : navigateur TIM");
        sh_gfx_draw_title_screen("SILENT HILL", "Haut/Bas : choix   A/START : lancer", line3, menu);
        break;
    }
    case SH_PHASE_LOADING:
        if (game->load_error) {
            sh_gfx_draw_title_screen("SILENT HILL", "Echec du chargement", "A / B / START : menu", game->load_error);
        } else {
            sh_gfx_draw_title_screen("SILENT HILL", "Chargement de la zone...",
                                     game->area_loaded ? "Construction de la map..." : "Lecture des chunks...",
                                     "Merci de patienter.");
        }
        break;
    case SH_PHASE_MAP:
        render_map(game);
        break;
    case SH_PHASE_MOVIE:
        sh_gfx_draw_movie(&game->movie.image, game->movie.revision);
        break;
    case SH_PHASE_SAVE: {
        char title[48], body[768];
        sh_save_menu_format(&game->save_menu, title, sizeof(title), body, sizeof(body));
        sh_gfx_draw_title_screen("SILENT HILL", title, "", body);
        break;
    }
    case SH_PHASE_GAME_OVER:
        sh_gfx_draw_title_screen("GAME OVER", "Harry is dead.",
                                 "A / START / B : title",
                                 "Try again from New Game.");
        break;
    case SH_PHASE_VIEWER:
    default:
        render_viewer(game);
        break;
    }
}
