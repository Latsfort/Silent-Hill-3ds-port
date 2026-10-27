#include "sh3ds/gfx.h"
#include "sh3ds/vc.h"
#include "sh3ds/choice.h"
#include "sh3ds/chara.h"
#include "sh3ds/config.h"
#include "sh3ds/item_model.h"
#include "sh3ds/smoke_mesh.h"
#include "sh3ds/water_mesh.h"
#include "sh3ds/water_texture.h"
#include "sh3ds/drain.h"
#include "sh3ds/locker_world.h"
#include "sh3ds/border.h"
#include "sh3ds/piano_render_data.h"
#include "sh3ds/silver.h"

#include <citro2d.h>
#include <citro3d.h>
#include <3ds.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "map_shbin.h"

/* Largest TIM in the game is 1024 wide (FONT16) and 480 tall (TITLE). */
#define SH_TEX_W 1024
#define SH_TEX_H 512

/* A 3x3 chunk window peaks around 56k triangles in the town. */
#define SH_MAP_MAX_VERTS (64000 * 3)
/* Linear memory left free after the atlas pages, for Citro3D/Citro2D and the system. */
#define SH_LINEAR_RESERVE (2u * 1024u * 1024u)
/* Keep single draw calls well under the PICA vertex counter limits. */
#define SH_MAP_DRAW_BATCH (30000 * 3)

#define SH_MAP_NEAR 0.1f
#define SH_MAP_FAR 90.0f
#define SH_FOG_RGB 0x8C8C90 /* 0xBBGGRR */
#define SH_FOG_RGBA 0x908C8CFF

#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static C3D_RenderTarget *s_top;
static C3D_RenderTarget *s_bottom;
static C2D_TextBuf s_text_buf;
static ShColor s_clear = {0.05f, 0.05f, 0.07f, 1.0f};
static bool s_ready;

static C3D_Tex s_tex;
static Tex3DS_SubTexture s_subtex;
static bool s_tex_ready;
static bool s_has_image;
static const ShTim *s_pending;
static const ShTim *s_uploaded;
static int s_img_crop_x;
static int s_img_crop_y;
static int s_img_crop_w;
static int s_img_crop_h;

static DVLB_s *s_map_dvlb;
static shaderProgram_s s_map_program;
static int s_uloc_projection;
static int s_uloc_model_view;
static int s_uloc_world_model;
static int s_uloc_env_tint;
static int s_uloc_env_params;
static int s_uloc_env_light_pos;
static int s_uloc_env_light_positive;
static int s_uloc_env_light_negative;
static C3D_FogLut s_fog_lut;
static C3D_FogLut s_environment_fog_lut;
static const ShEnvState *s_draw_environment;
static int32_t s_environment_fog_distance = -1;
static bool s_environment_fog_valid;
static ShMapVertex *s_map_vbo[2];
static ShMapVertex *s_moving_vbo;
static size_t s_moving_capacity;
static size_t s_moving_draw_count;
size_t sh_gfx_moving_vertex_count(void) { return s_moving_draw_count; }
static int s_map_front;
static size_t s_map_count;
static ShMapRange s_map_ranges[SH_MAP_MAX_RANGES];
static int s_map_range_count;
static C3D_Tex s_atlas[SH_GFX_ATLAS_POOL];
static int s_atlas_count;
static C3D_Tex s_smoke_tex;
static bool s_smoke_tex_ok;
static bool s_smoke_ready;
static C3D_Tex s_piano_cursor_tex;
static bool s_piano_cursor_ready;
static ShMapVertex *s_smoke_vbo;
static size_t s_smoke_draw_count;
static C3D_Tex s_water_tex;
static C3D_Tex s_water_ribbon_tex;
static bool s_water_ribbon_tex_ok;
static C3D_Tex s_water_surface_tex;
static bool s_water_surface_tex_ok;
static ShMapVertex *s_water_surface_vbo;
static bool s_water_tex_ok, s_water_ready;
static ShMapVertex *s_water_vbo;
static size_t s_water_draw_count;

size_t sh_gfx_smoke_vertex_count(void) { return s_smoke_draw_count; }
size_t sh_gfx_water_vertex_count(void) { return s_water_draw_count; }

#define SH_PLAYER_MAX_VERTS (900 * 3)
static ShMapVertex *s_player_vbo;
static size_t s_player_count;
static C3D_Tex s_harry_tex;
static bool s_harry_tex_ok;
static bool s_harry_tex_ready; /* true after a successful TIM upload */

/* NPC / cutscene actors (Air Screamer, Cybil…): own VBO + 512x256 atlas each. */
typedef struct ShActorGfx {
    ShMapVertex *vbo;
    size_t count;
    C3D_Tex tex;
    bool tex_ok;
    bool tex_ready;
} ShActorGfx;
static ShActorGfx s_actors[SH_GFX_MAX_ACTORS];

static ShMapVertex *s_item_vbo;
static size_t s_item_count;
static C3D_Tex s_item_tex;
static bool s_item_tex_ok;
static bool s_item_tex_ready;

static void atlas_init(void)
{
    const size_t page_bytes = (size_t)SH_ATLAS_SIZE * SH_ATLAS_SIZE * 2u;

    while (s_atlas_count < SH_GFX_ATLAS_POOL && linearSpaceFree() >= page_bytes + SH_LINEAR_RESERVE) {
        C3D_Tex *tex = &s_atlas[s_atlas_count];
        if (!C3D_TexInit(tex, SH_ATLAS_SIZE, SH_ATLAS_SIZE, GPU_RGBA5551)) {
            break;
        }
        C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        /* Transparent clear: unused atlas texels must not show as white glitch quads. */
        memset(tex->data, 0, page_bytes);
        C3D_TexFlush(tex);
        s_atlas_count++;
    }
}

static bool map_pipeline_init(void)
{
    s_map_dvlb = DVLB_ParseFile((u32 *)map_shbin, map_shbin_size);
    if (!s_map_dvlb) {
        return false;
    }
    shaderProgramInit(&s_map_program);
    shaderProgramSetVsh(&s_map_program, &s_map_dvlb->DVLE[0]);
    s_uloc_projection = shaderInstanceGetUniformLocation(s_map_program.vertexShader, "projection");
    s_uloc_model_view = shaderInstanceGetUniformLocation(s_map_program.vertexShader, "modelView");
    s_uloc_world_model = shaderInstanceGetUniformLocation(s_map_program.vertexShader, "worldModel");
    s_uloc_env_tint = shaderInstanceGetUniformLocation(s_map_program.vertexShader, "envTint");
    s_uloc_env_params = shaderInstanceGetUniformLocation(s_map_program.vertexShader, "envParams");
    s_uloc_env_light_pos = shaderInstanceGetUniformLocation(s_map_program.vertexShader, "envLightPos");
    s_uloc_env_light_positive = shaderInstanceGetUniformLocation(s_map_program.vertexShader, "envLightPositive");
    s_uloc_env_light_negative = shaderInstanceGetUniformLocation(s_map_program.vertexShader, "envLightNegative");

    /* Silent Hill's fog: almost clear up close, opaque past ~50 m. */
    FogLut_Exp(&s_fog_lut, 0.032f, 1.7f, SH_MAP_NEAR, SH_MAP_FAR);

    /* Small and required: take it before the map buffers drain linear memory. */
    s_player_vbo = linearAlloc(sizeof(ShMapVertex) * SH_PLAYER_MAX_VERTS);
    s_player_count = 0;
    s_harry_tex_ok = C3D_TexInit(&s_harry_tex, SH_CHARA_TEX_W, SH_CHARA_TEX_H, GPU_RGBA5551);
    if (s_harry_tex_ok) {
        C3D_TexSetFilter(&s_harry_tex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&s_harry_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        memset(s_harry_tex.data, 0, (size_t)SH_CHARA_TEX_W * SH_CHARA_TEX_H * 2u);
        C3D_TexFlush(&s_harry_tex);
    }
    for (int a = 0; a < SH_GFX_MAX_ACTORS; a++) {
        ShActorGfx *actor = &s_actors[a];
        memset(actor, 0, sizeof(*actor));
        actor->vbo = linearAlloc(sizeof(ShMapVertex) * SH_GFX_ACTOR_MAX_VERTS);
        actor->tex_ok = C3D_TexInit(&actor->tex, SH_NPC_TEX_W, SH_NPC_TEX_H, GPU_RGBA5551);
        if (actor->tex_ok) {
            C3D_TexSetFilter(&actor->tex, GPU_NEAREST, GPU_NEAREST);
            C3D_TexSetWrap(&actor->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
            memset(actor->tex.data, 0, (size_t)SH_NPC_TEX_W * SH_NPC_TEX_H * 2u);
            C3D_TexFlush(&actor->tex);
        }
    }
    s_item_vbo = linearAlloc(sizeof(ShMapVertex) * SH_ITEM_MAX_VERTS);
    s_item_count = 0;
    s_item_tex_ok = C3D_TexInit(&s_item_tex, SH_ITEM_TEX_SIZE, SH_ITEM_TEX_SIZE, GPU_RGBA5551);
    if (s_item_tex_ok) {
        C3D_TexSetFilter(&s_item_tex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&s_item_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    }

    /* Prefer atlas pages over a second VBO: without spare pages the streamer would
     * have to overwrite the drawn textures and could not keep the previous view. */
    s_map_vbo[0] = linearAlloc(sizeof(ShMapVertex) * SH_MAP_MAX_VERTS);
    if (!s_map_vbo[0]) {
        return false;
    }
    atlas_init();
    s_map_vbo[1] = linearAlloc(sizeof(ShMapVertex) * SH_MAP_MAX_VERTS);
    if (!s_map_vbo[1]) {
        /* One buffer: rebuilds must finish before the draw of that same frame. */
        s_map_vbo[1] = s_map_vbo[0];
    }
    return s_atlas_count > 0;
}

bool sh_gfx_init(void)
{
    if (s_ready) {
        return true;
    }

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    /* Own top target so the 3D map gets a depth buffer; Citro2D draws on it too. */
    s_top = C3D_RenderTargetCreate(SH3DS_TOP_HEIGHT, SH3DS_TOP_WIDTH, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (s_top) {
        C3D_RenderTargetSetOutput(s_top, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
    }
    s_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    s_text_buf = C2D_TextBufNew(4096);

    if (!s_top || !s_bottom || !s_text_buf) {
        return false;
    }

    s_tex_ready = C3D_TexInit(&s_tex, SH_TEX_W, SH_TEX_H, GPU_RGBA8);
    if (!s_tex_ready) {
        return false;
    }
    C3D_TexSetFilter(&s_tex, GPU_LINEAR, GPU_NEAREST);
    C3D_TexSetWrap(&s_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    if (!map_pipeline_init()) {
        return false;
    }

    s_ready = true;
    return true;
}

void sh_gfx_shutdown(void)
{
    if (!s_ready) {
        return;
    }

    for (int i = 0; i < s_atlas_count; i++) {
        C3D_TexDelete(&s_atlas[i]);
    }
    s_atlas_count = 0;
    if (s_map_vbo[1] && s_map_vbo[1] != s_map_vbo[0]) {
        linearFree(s_map_vbo[1]);
    }
    if (s_map_vbo[0]) {
        linearFree(s_map_vbo[0]);
    }
    s_map_vbo[0] = s_map_vbo[1] = NULL;
    if (s_moving_vbo) { linearFree(s_moving_vbo); }
    s_moving_vbo = NULL;
    s_moving_capacity = 0;
    if (s_player_vbo) {
        linearFree(s_player_vbo);
        s_player_vbo = NULL;
    }
    for (int a = 0; a < SH_GFX_MAX_ACTORS; a++) {
        if (s_actors[a].vbo) {
            linearFree(s_actors[a].vbo);
        }
        if (s_actors[a].tex_ok) {
            C3D_TexDelete(&s_actors[a].tex);
        }
        memset(&s_actors[a], 0, sizeof(s_actors[a]));
    }
    if (s_item_vbo) {
        linearFree(s_item_vbo);
        s_item_vbo = NULL;
    }
    s_item_count = 0;
    if (s_item_tex_ok) {
        C3D_TexDelete(&s_item_tex);
        s_item_tex_ok = false;
    }
    s_item_tex_ready = false;
    if (s_smoke_vbo) {
        linearFree(s_smoke_vbo);
        s_smoke_vbo = NULL;
    }
    if (s_smoke_tex_ok) {
        C3D_TexDelete(&s_smoke_tex);
        s_smoke_tex_ok = false;
    }
    s_smoke_ready = false;
    if (s_water_vbo) {
        linearFree(s_water_vbo);
        s_water_vbo = NULL;
    }
    if (s_water_tex_ok) {
        C3D_TexDelete(&s_water_tex);
        s_water_tex_ok = false;
    }
    s_water_ready = false;
    if (s_water_surface_vbo) { linearFree(s_water_surface_vbo); s_water_surface_vbo = NULL; }
    if (s_water_surface_tex_ok) { C3D_TexDelete(&s_water_surface_tex); s_water_surface_tex_ok = false; }
    if (s_water_ribbon_tex_ok) {
        C3D_TexDelete(&s_water_ribbon_tex);
        s_water_ribbon_tex_ok = false;
    }
    if (s_piano_cursor_ready) {
        C3D_TexDelete(&s_piano_cursor_tex);
        s_piano_cursor_ready = false;
    }
    s_player_count = 0;
    if (s_harry_tex_ok) {
        C3D_TexDelete(&s_harry_tex);
        s_harry_tex_ok = false;
    }
    s_harry_tex_ready = false;
    s_map_count = 0;
    s_map_range_count = 0;
    shaderProgramFree(&s_map_program);
    if (s_map_dvlb) {
        DVLB_Free(s_map_dvlb);
        s_map_dvlb = NULL;
    }
    if (s_tex_ready) {
        C3D_TexDelete(&s_tex);
        s_tex_ready = false;
    }
    if (s_text_buf) {
        C2D_TextBufDelete(s_text_buf);
        s_text_buf = NULL;
    }

    C3D_RenderTargetDelete(s_top);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    s_top = NULL;
    s_bottom = NULL;
    s_has_image = false;
    s_pending = NULL;
    s_ready = false;
}

void sh_gfx_set_clear_color(ShColor color)
{
    s_clear = color;
}

void sh_gfx_set_image(const ShTim *tim)
{
    sh_gfx_set_image_crop(tim, 0, 0, 0, 0);
}

void sh_gfx_set_image_crop(const ShTim *tim, int sx, int sy, int sw, int sh)
{
    /* Same image already resident: re-uploading 1024x512 every frame is too slow on Old 3DS. */
    if (tim && s_has_image && tim == s_uploaded && sx == s_img_crop_x && sy == s_img_crop_y && sw == s_img_crop_w &&
        sh == s_img_crop_h) {
        return;
    }
    s_pending = tim;
    s_img_crop_x = sx;
    s_img_crop_y = sy;
    s_img_crop_w = sw;
    s_img_crop_h = sh;
}

static void upload_pending_image(void)
{
    const ShTim *tim = s_pending;
    s_pending = NULL;
    if (!tim || !s_tex_ready) {
        return;
    }

    int src_x = 0;
    int src_y = 0;
    int w = tim->width;
    int h = tim->height;
    if (s_img_crop_w > 0 && s_img_crop_h > 0) {
        src_x = s_img_crop_x;
        src_y = s_img_crop_y;
        w = s_img_crop_w;
        h = s_img_crop_h;
    }
    if (src_x < 0) {
        src_x = 0;
    }
    if (src_y < 0) {
        src_y = 0;
    }
    if (src_x + w > tim->width) {
        w = tim->width - src_x;
    }
    if (src_y + h > tim->height) {
        h = tim->height - src_y;
    }
    if (w > SH_TEX_W) {
        w = SH_TEX_W;
    }
    if (h > SH_TEX_H) {
        h = SH_TEX_H;
    }
    uint32_t *dst = (uint32_t *)s_tex.data;

    memset(dst, 0, (size_t)SH_TEX_W * SH_TEX_H * 4u);

    /* PICA200 textures are stored as 8x8 Morton-ordered tiles. */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const uint32_t tile = (uint32_t)(((y >> 3) * (SH_TEX_W >> 3) + (x >> 3)) << 6);
            const uint32_t morton = (uint32_t)((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
                                               ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3));
            dst[tile + morton] = sh_tim_pixel_rgba(tim, src_x + x, src_y + y);
        }
    }

    C3D_TexFlush(&s_tex);

    s_subtex.width = (u16)w;
    s_subtex.height = (u16)h;
    s_subtex.left = 0.0f;
    s_subtex.top = 1.0f;
    s_subtex.right = (float)w / SH_TEX_W;
    s_subtex.bottom = 1.0f - (float)h / SH_TEX_H;
    s_has_image = true;
    s_uploaded = tim;
}

static C3D_Tex s_marks_tex;
static bool s_marks_ready;

bool sh_gfx_set_piano_cursor(const ShTim *tim)
{
    if (!tim || tim->width < SH_PIANO_CURSOR_U + SH_PIANO_CURSOR_WIDTH ||
        tim->height < SH_PIANO_CURSOR_V + SH_PIANO_CURSOR_HEIGHT) {
        return false;
    }
    if (!s_piano_cursor_ready) {
        if (!C3D_TexInit(&s_piano_cursor_tex, SH_PIANO_CURSOR_WIDTH, SH_PIANO_CURSOR_HEIGHT, GPU_RGBA8)) {
            return false;
        }
        C3D_TexSetFilter(&s_piano_cursor_tex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&s_piano_cursor_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        s_piano_cursor_ready = true;
    }
    uint32_t *pixels = (uint32_t *)s_piano_cursor_tex.data;
    for (int y = 0; y < SH_PIANO_CURSOR_HEIGHT; y++) {
        for (int x = 0; x < SH_PIANO_CURSOR_WIDTH; x++) {
            const unsigned tile = ((y >> 3) * (SH_PIANO_CURSOR_WIDTH >> 3) + (x >> 3)) << 6;
            const unsigned morton = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
                                    ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
            pixels[tile + morton] = sh_tim_pixel_rgba_clut(tim, SH_PIANO_CURSOR_U + x,
                                                          SH_PIANO_CURSOR_V + y, SH_PIANO_CURSOR_CLUT_OFFSET);
        }
    }
    C3D_TexFlush(&s_piano_cursor_tex);
    return true;
}

/* Gfx_CursorDraw: opaque FT4, neutral tint 128, PS1 screen origin at (160,120). */
static void draw_piano_cursor(int x, int y)
{
    if (!s_piano_cursor_ready) { return; }
    const Tex3DS_SubTexture sub = {
        .width = SH_PIANO_CURSOR_WIDTH, .height = SH_PIANO_CURSOR_HEIGHT,
        .left = 0.0f, .top = 1.0f, .right = 1.0f, .bottom = 0.0f
    };
    const C2D_Image image = {&s_piano_cursor_tex, &sub};
    C2D_DrawImageAt(image, 200.0f + x - SH_PIANO_CURSOR_HALF_X,
                    120.0f + y - SH_PIANO_CURSOR_HALF_Y, 0.93f, NULL,
                    (2.0f * SH_PIANO_CURSOR_HALF_X) / SH_PIANO_CURSOR_WIDTH,
                    (2.0f * SH_PIANO_CURSOR_HALF_Y) / SH_PIANO_CURSOR_HEIGHT);
}

bool sh_gfx_set_water_atlas(const ShTim *tim)
{
    s_water_ready = false;
    if (!tim) {
        if (s_water_vbo) { linearFree(s_water_vbo); s_water_vbo = NULL; }
        if (s_water_tex_ok) { C3D_TexDelete(&s_water_tex); s_water_tex_ok = false; }
        if (s_water_ribbon_tex_ok) { C3D_TexDelete(&s_water_ribbon_tex); s_water_ribbon_tex_ok = false; }
        if (s_water_surface_vbo) { linearFree(s_water_surface_vbo); s_water_surface_vbo = NULL; }
        if (s_water_surface_tex_ok) { C3D_TexDelete(&s_water_surface_tex); s_water_surface_tex_ok = false; }
        return false;
    }
    if (!s_water_vbo) {
        s_water_vbo = linearAlloc(sizeof(ShMapVertex) * SH_ROOF_WATER_POOL_COUNT * 6);
    }
    if (!s_water_tex_ok) {
        s_water_tex_ok = C3D_TexInit(&s_water_tex, SH_WATER_SPRAY_ATLAS_WIDTH, SH_WATER_SPRAY_ATLAS_HEIGHT, GPU_RGBA8);
        if (s_water_tex_ok) {
            C3D_TexSetFilter(&s_water_tex, GPU_NEAREST, GPU_NEAREST);
            C3D_TexSetWrap(&s_water_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        }
    }
    if (!s_water_ribbon_tex_ok) {
        s_water_ribbon_tex_ok = C3D_TexInit(&s_water_ribbon_tex, SH_WATER_RIBBON_ATLAS_WIDTH, SH_WATER_RIBBON_ATLAS_HEIGHT, GPU_RGBA8);
        if (s_water_ribbon_tex_ok) {
            C3D_TexSetFilter(&s_water_ribbon_tex, GPU_NEAREST, GPU_NEAREST);
            C3D_TexSetWrap(&s_water_ribbon_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        }
    }
    if (!s_water_surface_vbo) {
        s_water_surface_vbo = linearAlloc(sizeof(ShMapVertex) * SH_WATER_SURFACE_MAX_QUADS * 6);
    }
    if (!s_water_surface_tex_ok) {
        s_water_surface_tex_ok = C3D_TexInit(&s_water_surface_tex, SH_WATER_SURFACE_TEXTURE_W, SH_WATER_SURFACE_TEXTURE_H, GPU_RGBA8);
        if (s_water_surface_tex_ok) {
            C3D_TexSetFilter(&s_water_surface_tex, GPU_NEAREST, GPU_NEAREST);
            C3D_TexSetWrap(&s_water_surface_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        }
    }
    if (!s_water_vbo || !s_water_tex_ok || !s_water_ribbon_tex_ok || !s_water_surface_vbo || !s_water_surface_tex_ok ||
        !sh_water_surface_decode(tim, s_water_surface_tex.data, SH_WATER_SURFACE_TEXTURE_W * SH_WATER_SURFACE_TEXTURE_H) ||
        !sh_water_ribbon_decode_atlas(tim, s_water_ribbon_tex.data, SH_WATER_RIBBON_ATLAS_WIDTH * SH_WATER_RIBBON_ATLAS_HEIGHT) ||
        !sh_water_spray_decode_atlas(tim, s_water_tex.data,
            SH_WATER_SPRAY_ATLAS_WIDTH * SH_WATER_SPRAY_ATLAS_HEIGHT)) {
        sh_gfx_set_water_atlas(NULL);
        return false;
    }
    C3D_TexFlush(&s_water_tex);
    C3D_TexFlush(&s_water_ribbon_tex);
    C3D_TexFlush(&s_water_surface_tex);
    s_water_ready = true;
    return true;
}

bool sh_gfx_set_smoke_atlas(const ShTim *tim)
{
    s_smoke_ready = false;
    if (!tim) { return false; }
    if (!s_smoke_vbo) {
        s_smoke_vbo = linearAlloc(sizeof(ShMapVertex) * SH_SMOKE_CAPACITY * 6);
    }
    if (!s_smoke_tex_ok) {
        s_smoke_tex_ok = C3D_TexInit(&s_smoke_tex, SH_SMOKE_ATLAS_WIDTH, SH_SMOKE_ATLAS_HEIGHT, GPU_RGBA8);
        if (s_smoke_tex_ok) {
            C3D_TexSetFilter(&s_smoke_tex, GPU_NEAREST, GPU_NEAREST);
            C3D_TexSetWrap(&s_smoke_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        }
    }
    if (!s_smoke_vbo || !s_smoke_tex_ok || !sh_smoke_decode_atlas(tim, s_smoke_tex.data,
                                                                SH_SMOKE_ATLAS_WIDTH * SH_SMOKE_ATLAS_HEIGHT)) {
        return false;
    }
    C3D_TexFlush(&s_smoke_tex);
    s_smoke_ready = true;
    return true;
}

void sh_gfx_set_map_marks_atlas(const ShTim *tim)
{
    if (!tim) {
        return;
    }
    const int tw = 128, th = 256;
    if (!s_marks_ready) {
        if (!C3D_TexInit(&s_marks_tex, tw, th, GPU_RGBA8)) {
            return;
        }
        C3D_TexSetFilter(&s_marks_tex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&s_marks_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        s_marks_ready = true;
    }
    uint32_t *dst = (uint32_t *)s_marks_tex.data;
    memset(dst, 0, (size_t)tw * th * 4u);
    const int w = tim->width < tw ? tim->width : tw;
    const int h = tim->height < th ? tim->height : th;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const uint32_t tile = (uint32_t)(((y >> 3) * (tw >> 3) + (x >> 3)) << 6);
            const uint32_t morton = (uint32_t)((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) |
                                               ((x & 4) << 2) | ((y & 4) << 3));
            dst[tile + morton] = sh_tim_pixel_rgba(tim, x, y);
        }
    }
    C3D_TexFlush(&s_marks_tex);
}

/* PS1 320x240 screen → top LCD (4:3 window centred in 400 px). */
#define PS1_TO_TOP_X(x) (40.0f + (x))

static void draw_map_sprites(const ShMapSprite *sp, int count, float depth)
{
    if (!s_marks_ready || !sp) {
        return;
    }
    /* PaperMap marks: tpage 0x47 = semi-transparency mode 2 (background minus sprite). */
    C2D_Flush();
    C3D_AlphaBlend(GPU_BLEND_REVERSE_SUBTRACT, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_ZERO, GPU_ONE);
    for (int i = 0; i < count; i++) {
        const ShMapSprite *s = &sp[i];
        if (s->w == 0 || s->h == 0 || s->sw <= 0.0f || s->sh <= 0.0f) {
            continue;
        }
        Tex3DS_SubTexture sub = {
            .width = s->w,
            .height = s->h,
            .left = (float)s->u / 128.0f,
            .top = 1.0f - (float)s->v / 256.0f,
            .right = (float)(s->u + s->w) / 128.0f,
            .bottom = 1.0f - (float)(s->v + s->h) / 256.0f,
        };
        const C2D_Image img = {&s_marks_tex, &sub};
        C2D_ImageTint tint;
        const float k = s->shade < 0.0f ? 0.0f : (s->shade > 1.0f ? 1.0f : s->shade);
        C2D_PlainImageTint(&tint, C2D_Color32f(1.0f, 1.0f, 1.0f, k), 0.0f);
        C2D_DrawImageAt(img, PS1_TO_TOP_X(s->x), s->y, depth, &tint, s->sw / (float)s->w, s->sh / (float)s->h);
    }
    C2D_Flush();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA);
}

/* PaperMap_ExpandingBoxesDraw: 5 LINE_F3 outlines, blue 128 - i*64/5. */
/* func_80067914 — green/yellow player wedge on the paper map (320x240 space). */
static void draw_map_player(float x, float y, float yaw_ps1, float depth)
{
    const float a = yaw_ps1;
    const float sin_a = sinf(a);
    const float cos_a = cosf(a);
    const float x0 = x + sin_a * 6.0f;
    const float y0 = y + cos_a * -6.0f;
    const float x1 = x + cos_a * 4.0f - sin_a * 6.0f;
    const float y1 = y + sin_a * 4.0f + cos_a * 6.0f;
    const float x2 = x + cos_a * -4.0f - sin_a * 6.0f;
    const float y2 = y + sin_a * -4.0f + cos_a * 6.0f;
    const u32 edge = C2D_Color32(16, 16, 16, 255);
    const u32 tip = C2D_Color32(0, 255, 0, 255);
    const u32 base = C2D_Color32(255, 220, 0, 255);
    const float sx0 = PS1_TO_TOP_X(x0), sx1 = PS1_TO_TOP_X(x1), sx2 = PS1_TO_TOP_X(x2);
    C2D_DrawTriangle(sx0, y0, tip, sx1, y1, base, sx2, y2, base, depth);
    C2D_DrawLine(sx0, y0, edge, sx1, y1, edge, 1.5f, depth + 0.001f);
    C2D_DrawLine(sx1, y1, edge, sx2, y2, edge, 1.5f, depth + 0.001f);
    C2D_DrawLine(sx2, y2, edge, sx0, y0, edge, 1.5f, depth + 0.001f);
}

static void draw_map_boxes(const float (*boxes)[4], int count, float depth)
{
    for (int i = 0; i < count; i++) {
        const float x0 = PS1_TO_TOP_X(boxes[i][0]), y0 = boxes[i][1];
        const float x1 = PS1_TO_TOP_X(boxes[i][2]), y1 = boxes[i][3];
        const u32 c = C2D_Color32(0, 0, (u8)(128 - (i * 64) / 5), 255);
        C2D_DrawLine(x0, y0, c, x1, y0, c, 1.0f, depth);
        C2D_DrawLine(x1, y0, c, x1, y1, c, 1.0f, depth);
        C2D_DrawLine(x1, y1, c, x0, y1, c, 1.0f, depth);
        C2D_DrawLine(x0, y1, c, x0, y0, c, 1.0f, depth);
    }
}

/* PaperMap_DrawScaled: show the 320*scale x 240*scale window at scroll, stretched to 320x240. */
static void draw_paper_map(float scale, float scroll_x, float scroll_y, float depth)
{
    if (!s_has_image) {
        return;
    }
    if (scale < 0.05f || scale > 1.0f) {
        scale = 1.0f;
    }
    const float vw = 320.0f * scale, vh = 240.0f * scale;
    const float u0 = scroll_x, v0 = scroll_y;
    Tex3DS_SubTexture sub = {
        .width = (u16)vw,
        .height = (u16)vh,
        .left = u0 / SH_TEX_W,
        .top = 1.0f - v0 / SH_TEX_H,
        .right = (u0 + vw) / SH_TEX_W,
        .bottom = 1.0f - (v0 + vh) / SH_TEX_H,
    };
    const C2D_Image img = {&s_tex, &sub};
    C2D_DrawImageAt(img, PS1_TO_TOP_X(0.0f), 0.0f, depth, NULL, 320.0f / vw, 240.0f / vh);
}

void sh_gfx_begin_frame(void)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    upload_pending_image();
}

void sh_gfx_end_frame(void)
{
    C3D_FrameEnd(0);
}

/* PS1 Screen_BackgroundImgDraw / PaperMap_DrawScaled — 320x240 map window, 4:3 on the top LCD. */
static void draw_tim_image_centered(float depth)
{
    if (!s_has_image) {
        return;
    }
    const int w = s_subtex.width;
    const int h = s_subtex.height;
    float scale_x;
    float scale_y;
    if ((h == 240 || h == 480) && w >= 256 && w <= 640) {
        scale_y = (float)SH3DS_TOP_HEIGHT / (float)h;
        scale_x = (SH3DS_TOP_HEIGHT * 4.0f / 3.0f) / (float)w;
    } else {
        const float sx = (float)SH3DS_TOP_WIDTH / (float)w;
        const float sy = (float)SH3DS_TOP_HEIGHT / (float)h;
        scale_x = scale_y = sx < sy ? sx : sy;
    }
    const float draw_w = (float)w * scale_x;
    const float draw_h = (float)h * scale_y;
    const C2D_Image image = {&s_tex, &s_subtex};
    C2D_DrawImageAt(image, (SH3DS_TOP_WIDTH - draw_w) * 0.5f, (SH3DS_TOP_HEIGHT - draw_h) * 0.5f, depth, NULL,
                    scale_x, scale_y);
}

static void draw_text_line(float x, float y, float scale, const char *text, u32 color)
{
    if (!text || !text[0]) {
        return;
    }

    C2D_Text c2d_text;
    C2D_TextParse(&c2d_text, s_text_buf, text);
    C2D_TextOptimize(&c2d_text);
    C2D_DrawText(&c2d_text, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

void sh_gfx_draw_boot_screen(const char *line1, const char *line2, const char *line3)
{
    sh_gfx_draw_title_screen(line1, line2, line3, "Chargement...");
}

void sh_gfx_draw_title_screen(const char *line1, const char *line2, const char *line3, const char *bottom)
{
    const u32 clear = C2D_Color32(12, 12, 14, 255);
    const u32 fog = C2D_Color32(36, 36, 42, 255);
    const u32 band = C2D_Color32(22, 22, 26, 255);
    const u32 title = C2D_Color32(210, 210, 218, 255);
    const u32 muted = C2D_Color32(140, 140, 150, 255);

    C2D_TextBufClear(s_text_buf);

    C2D_TargetClear(s_top, clear);
    C2D_SceneBegin(s_top);
    C2D_DrawRectSolid(0.0f, 0.0f, 0.3f, (float)SH3DS_TOP_WIDTH, (float)SH3DS_TOP_HEIGHT, clear);
    C2D_DrawRectSolid(0.0f, 70.0f, 0.4f, (float)SH3DS_TOP_WIDTH, 100.0f, fog);
    C2D_DrawRectSolid(0.0f, 108.0f, 0.45f, (float)SH3DS_TOP_WIDTH, 24.0f, band);

    draw_text_line(28.0f, 40.0f, 0.85f, line1 ? line1 : "SILENT HILL", title);
    draw_text_line(28.0f, 112.0f, 0.48f, line2 ? line2 : "", muted);
    draw_text_line(28.0f, 150.0f, 0.42f, line3 ? line3 : "", muted);

    C2D_TargetClear(s_bottom, clear);
    C2D_SceneBegin(s_bottom);
    draw_text_line(16.0f, 20.0f, 0.5f, bottom ? bottom : "", title);
    int lines = 1;
    for (const char *p = bottom; p && *p; p++) {
        lines += *p == '\n';
    }
    const float footer_y = lines > 1 ? 20.0f + (lines + 1) * 16.0f : 56.0f;
    draw_text_line(16.0f, footer_y, 0.4f, "Port Old 3DS — silent-hill-decomp", muted);
}

void sh_gfx_draw_viewer(const char *title, const char *info, const char *hint)
{
    const u32 black = C2D_Color32(0, 0, 0, 255);
    const u32 clear = C2D_Color32f(s_clear.r, s_clear.g, s_clear.b, s_clear.a);
    const u32 text = C2D_Color32(220, 220, 230, 255);
    const u32 muted = C2D_Color32(160, 160, 170, 255);

    C2D_TextBufClear(s_text_buf);

    C2D_TargetClear(s_top, black);
    C2D_SceneBegin(s_top);

    if (s_has_image) {
        draw_tim_image_centered(0.5f);
    }

    C2D_TargetClear(s_bottom, clear);
    C2D_SceneBegin(s_bottom);
    draw_text_line(16.0f, 16.0f, 0.6f, title, text);
    draw_text_line(16.0f, 56.0f, 0.45f, info, muted);
    draw_text_line(16.0f, 200.0f, 0.4f, hint, muted);
}

void sh_gfx_draw_movie(const ShTim *frame, uint32_t revision)
{
    static uint32_t last_revision = UINT32_MAX;
    if (revision != last_revision || s_uploaded != frame) {
        s_pending = frame;
        s_img_crop_x = s_img_crop_y = s_img_crop_w = s_img_crop_h = 0;
        upload_pending_image();
        last_revision = revision;
    }
    const u32 black = C2D_Color32(0, 0, 0, 255);
    C2D_TargetClear(s_top, black);
    C2D_SceneBegin(s_top);
    if (s_has_image) {
        const C2D_Image image = {&s_tex, &s_subtex};
        C2D_DrawImageAt(image, (SH3DS_TOP_WIDTH - frame->width) * 0.5f,
            (SH3DS_TOP_HEIGHT - frame->height) * 0.5f, 0.5f, NULL, 1.0f, 1.0f);
    }
    C2D_TargetClear(s_bottom, black);
    C2D_SceneBegin(s_bottom);
}

ShMapVertex *sh_gfx_map_back_storage(size_t *capacity)
{
    ShMapVertex *back = s_map_vbo[s_map_front ^ 1];
    if (capacity) {
        *capacity = back ? SH_MAP_MAX_VERTS : 0;
    }
    return back;
}

bool sh_gfx_map_double_buffered(void)
{
    return s_map_vbo[0] && s_map_vbo[1] && s_map_vbo[0] != s_map_vbo[1];
}

void sh_gfx_map_present(size_t vertex_count, const ShMapRange *ranges, int range_count)
{
    const int back = s_map_front ^ 1;
    if (!s_map_vbo[back]) {
        return;
    }

    s_map_count = vertex_count < SH_MAP_MAX_VERTS ? vertex_count : SH_MAP_MAX_VERTS;
    s_map_range_count = 0;
    for (int i = 0; i < range_count && i < SH_MAP_MAX_RANGES; i++) {
        if (ranges[i].first + ranges[i].count > s_map_count || ranges[i].page >= s_atlas_count) {
            continue;
        }
        s_map_ranges[s_map_range_count++] = ranges[i];
    }
    if (s_map_count > 0) {
        GSPGPU_FlushDataCache(s_map_vbo[back], sizeof(ShMapVertex) * s_map_count);
    }
    s_map_front = back;
}

void sh_gfx_map_clear(void)
{
    s_map_count = 0;
    s_map_range_count = 0;
}

int sh_gfx_atlas_pages(void)
{
    return s_atlas_count;
}

bool sh_gfx_atlas_page_busy(int page)
{
    for (int i = 0; i < s_map_range_count; i++) {
        if (s_map_ranges[i].page == page) {
            return true;
        }
    }
    return false;
}

static uint16_t rgba_to_5551(uint32_t c)
{
    return (uint16_t)(((c >> 27) & 0x1F) << 11 | ((c >> 19) & 0x1F) << 6 | ((c >> 11) & 0x1F) << 1 |
                      ((c & 0xFF) ? 1 : 0));
}

void sh_gfx_atlas_clear_page(int page)
{
    if (page < 0 || page >= s_atlas_count) {
        return;
    }
    const size_t page_bytes = (size_t)SH_ATLAS_SIZE * SH_ATLAS_SIZE * 2u;
    memset(s_atlas[page].data, 0, page_bytes);
    C3D_TexFlush(&s_atlas[page]);
}

void sh_gfx_atlas_blit(int page, int dx, int dy, const ShTim *tim, int sx, int sy, int w, int h, int clut_offset)
{
    if (page < 0 || page >= s_atlas_count || !tim || sx < 0 || sy < 0 || dx < 0 || dy < 0) {
        return;
    }
    if (sx + w > tim->width) {
        w = tim->width - sx;
    }
    if (sy + h > tim->height) {
        h = tim->height - sy;
    }
    if (dx + w > SH_ATLAS_SIZE) {
        w = SH_ATLAS_SIZE - dx;
    }
    if (dy + h > SH_ATLAS_SIZE) {
        h = SH_ATLAS_SIZE - dy;
    }

    uint16_t *dst = (uint16_t *)s_atlas[page].data;
    uint16_t palette[256];
    const bool indexed = tim->bpp == 4 || tim->bpp == 8;
    if (indexed) {
        const int colors = tim->bpp == 4 ? 16 : 256;
        for (int i = 0; i < colors; i++) {
            palette[i] = rgba_to_5551(sh_tim_palette_rgba(tim, clut_offset, (unsigned)i));
        }
    }

    for (int y = 0; y < h; y++) {
        const int ty = dy + y;
        const uint8_t *row = tim->pixels + (size_t)(sy + y) * tim->stride;
        const uint32_t tile_row = (uint32_t)((ty >> 3) * (SH_ATLAS_SIZE >> 3));
        const uint32_t morton_y = (uint32_t)(((ty & 1) << 1) | ((ty & 2) << 2) | ((ty & 4) << 3));

        for (int x = 0; x < w; x++) {
            const int tx = dx + x;
            const int px = sx + x;
            uint16_t texel;
            if (tim->bpp == 4) {
                texel = palette[(row[px >> 1] >> ((px & 1) * 4)) & 0xF];
            } else if (indexed) {
                texel = palette[row[px]];
            } else {
                texel = rgba_to_5551(sh_tim_pixel_rgba(tim, px, sy + y));
            }
            const uint32_t tile = (tile_row + (uint32_t)(tx >> 3)) << 6;
            const uint32_t morton = morton_y | (uint32_t)((tx & 1) | ((tx & 2) << 1) | ((tx & 4) << 2));
            dst[tile + morton] = texel;
        }
    }
}

void sh_gfx_atlas_flush_page(int page)
{
    if (page >= 0 && page < s_atlas_count) {
        C3D_TexFlush(&s_atlas[page]);
    }
}

static void emit_marker_tri(ShMapVertex *v, float x0, float y0, float z0, float x1, float y1, float z1,
                            float x2, float y2, float z2, uint8_t r, uint8_t g, uint8_t b)
{
    const float pts[3][3] = {{x0, y0, z0}, {x1, y1, z1}, {x2, y2, z2}};
    for (int i = 0; i < 3; i++) {
        v[i].pos[0] = pts[i][0];
        v[i].pos[1] = pts[i][1];
        v[i].pos[2] = pts[i][2];
        v[i].uv[0] = 0.0f;
        v[i].uv[1] = 0.0f;
        v[i].color[0] = r;
        v[i].color[1] = g;
        v[i].color[2] = b;
        v[i].color[3] = 255;
    }
}

/* Bright capsule so the player is always visible even if HERO.ILM fails. */
bool sh_gfx_set_player_marker(void)
{
    if (!s_player_vbo) {
        s_player_count = 0;
        return false;
    }

    ShMapVertex *v = s_player_vbo;
    size_t n = 0;
    /* Body box ~0.5 x 1.5 x 0.35 m, feet at y=0. */
    const float hx = 0.22f, hz = 0.14f, top = 1.45f, mid = 0.75f;
    const uint8_t r = 255, g = 90, b = 40;

    /* front */
    emit_marker_tri(v + n, -hx, 0, hz, hx, 0, hz, hx, mid, hz, r, g, b); n += 3;
    emit_marker_tri(v + n, -hx, 0, hz, hx, mid, hz, -hx, mid, hz, r, g, b); n += 3;
    emit_marker_tri(v + n, -hx, mid, hz, hx, mid, hz, hx, top, hz, r, g, b); n += 3;
    emit_marker_tri(v + n, -hx, mid, hz, hx, top, hz, -hx, top, hz, r, g, b); n += 3;
    /* back */
    emit_marker_tri(v + n, hx, 0, -hz, -hx, 0, -hz, -hx, mid, -hz, r, g, b); n += 3;
    emit_marker_tri(v + n, hx, 0, -hz, -hx, mid, -hz, hx, mid, -hz, r, g, b); n += 3;
    emit_marker_tri(v + n, hx, mid, -hz, -hx, mid, -hz, -hx, top, -hz, r, g, b); n += 3;
    emit_marker_tri(v + n, hx, mid, -hz, -hx, top, -hz, hx, top, -hz, r, g, b); n += 3;
    /* head */
    emit_marker_tri(v + n, -0.12f, top, 0.12f, 0.12f, top, 0.12f, 0.12f, top + 0.28f, 0.12f, r, g, b); n += 3;
    emit_marker_tri(v + n, -0.12f, top, 0.12f, 0.12f, top + 0.28f, 0.12f, -0.12f, top + 0.28f, 0.12f, r, g, b); n += 3;
    emit_marker_tri(v + n, 0.12f, top, -0.12f, -0.12f, top, -0.12f, -0.12f, top + 0.28f, -0.12f, r, g, b); n += 3;
    emit_marker_tri(v + n, 0.12f, top, -0.12f, -0.12f, top + 0.28f, -0.12f, 0.12f, top + 0.28f, -0.12f, r, g, b); n += 3;

    GSPGPU_FlushDataCache(s_player_vbo, n * sizeof(ShMapVertex));
    s_player_count = n;
    return true;
}

bool sh_gfx_set_player_mesh(const ShMapVertex *verts, size_t count)
{
    if (!s_player_vbo || !verts || count == 0) {
        s_player_count = 0;
        return false;
    }
    if (count > SH_PLAYER_MAX_VERTS) {
        count = SH_PLAYER_MAX_VERTS;
    }
    memcpy(s_player_vbo, verts, count * sizeof(ShMapVertex));
    GSPGPU_FlushDataCache(s_player_vbo, count * sizeof(ShMapVertex));
    s_player_count = count;
    return true;
}

/* Blit one CLUT variant of `tim` into atlas sheet index `atlas_slot`. */
static void upload_sheet_slot(C3D_Tex *tex, int tex_w, int tex_h, int sheet_w, int sheet_h, int per_row,
                              int atlas_slot, const ShTim *tim, unsigned clut_delta)
{
    if (!tex || !tim || atlas_slot < 0) {
        return;
    }
    uint16_t *dst = (uint16_t *)tex->data;
    const int copy_w = tim->width < sheet_w ? tim->width : sheet_w;
    const int copy_h = tim->height < sheet_h ? tim->height : sheet_h;
    const int clut = sh_tim_clut_offset(tim, clut_delta);
    const int dx = (atlas_slot % per_row) * sheet_w;
    const int dy = (atlas_slot / per_row) * sheet_h;
    if (dy + copy_h > tex_h || dx + copy_w > tex_w) {
        return;
    }
    uint16_t palette[256];
    const bool indexed = tim->bpp == 4 || tim->bpp == 8;
    if (indexed) {
        const int colors = tim->bpp == 4 ? 16 : 256;
        for (int i = 0; i < colors; i++) {
            palette[i] = rgba_to_5551(sh_tim_palette_rgba(tim, clut, (unsigned)i));
        }
    }
    for (int y = 0; y < copy_h; y++) {
        const int ty = dy + y;
        const uint8_t *row = tim->pixels + (size_t)y * tim->stride;
        const uint32_t tile_row = (uint32_t)((ty >> 3) * (tex_w >> 3));
        const uint32_t morton_y = (uint32_t)(((ty & 1) << 1) | ((ty & 2) << 2) | ((ty & 4) << 3));
        for (int x = 0; x < copy_w; x++) {
            const int tx = dx + x;
            uint16_t texel;
            if (tim->bpp == 4) {
                texel = palette[(row[x >> 1] >> ((x & 1) * 4)) & 0xF];
            } else if (indexed) {
                texel = palette[row[x]];
            } else {
                texel = rgba_to_5551(sh_tim_pixel_rgba(tim, x, y));
            }
            const uint32_t tile = (tile_row + (uint32_t)(tx >> 3)) << 6;
            const uint32_t morton = morton_y | (uint32_t)((tx & 1) | ((tx & 2) << 1) | ((tx & 4) << 2));
            dst[tile + morton] = texel;
        }
    }
}

/* Blit each CLUT variant of `tim` into consecutive sheet slots of a tiled RGBA5551 texture. */
static void upload_sheets(C3D_Tex *tex, int tex_w, int tex_h, int sheet_w, int sheet_h, int per_row,
                          const ShTim *tim, const unsigned *clut_deltas, int count)
{
    memset(tex->data, 0, (size_t)tex_w * (size_t)tex_h * 2u);
    for (int slot = 0; slot < count; slot++) {
        upload_sheet_slot(tex, tex_w, tex_h, sheet_w, sheet_h, per_row, slot, tim, clut_deltas[slot]);
    }
    C3D_TexFlush(tex);
}

bool sh_gfx_set_player_tim(const ShTim *tim, const unsigned *clut_deltas, int count)
{
    if (!s_harry_tex_ok || !tim || !clut_deltas || count <= 0) {
        return false;
    }
    if (count > SH_CHARA_CLUT_SLOTS) {
        count = SH_CHARA_CLUT_SLOTS;
    }
    upload_sheets(&s_harry_tex, SH_CHARA_TEX_W, SH_CHARA_TEX_H, SH_CHARA_SHEET_W, SH_CHARA_SHEET_H,
                  SH_CHARA_SHEETS_PER_ROW, tim, clut_deltas, count);
    s_harry_tex_ready = true;
    return true;
}

bool sh_gfx_set_player_clut_atlas(const ShTim *hero, const ShTim *held, const ShCharaClutMap *map)
{
    if (!s_harry_tex_ok || !hero || !map || map->count <= 0) {
        return false;
    }
    const int count = map->count > SH_CHARA_CLUT_SLOTS ? SH_CHARA_CLUT_SLOTS : map->count;
    memset(s_harry_tex.data, 0, (size_t)SH_CHARA_TEX_W * (size_t)SH_CHARA_TEX_H * 2u);
    for (int slot = 0; slot < count; slot++) {
        const ShTim *tim = map->tim_src[slot] ? held : hero;
        if (!tim) {
            tim = hero;
        }
        upload_sheet_slot(&s_harry_tex, SH_CHARA_TEX_W, SH_CHARA_TEX_H, SH_CHARA_SHEET_W, SH_CHARA_SHEET_H,
                          SH_CHARA_SHEETS_PER_ROW, slot, tim, map->delta[slot]);
    }
    C3D_TexFlush(&s_harry_tex);
    s_harry_tex_ready = true;
    return true;
}

bool sh_gfx_actor_set_mesh(int slot, const ShMapVertex *verts, size_t count)
{
    if (slot < 0 || slot >= SH_GFX_MAX_ACTORS) {
        return false;
    }
    ShActorGfx *actor = &s_actors[slot];
    if (!actor->vbo || !verts || count == 0) {
        actor->count = 0;
        return false;
    }
    if (count > SH_GFX_ACTOR_MAX_VERTS) {
        count = SH_GFX_ACTOR_MAX_VERTS;
    }
    memcpy(actor->vbo, verts, count * sizeof(ShMapVertex));
    GSPGPU_FlushDataCache(actor->vbo, count * sizeof(ShMapVertex));
    actor->count = count;
    return true;
}

bool sh_gfx_actor_set_tim(int slot, const ShTim *tim, const unsigned *clut_deltas, int count)
{
    if (slot < 0 || slot >= SH_GFX_MAX_ACTORS) {
        return false;
    }
    ShActorGfx *actor = &s_actors[slot];
    if (!actor->tex_ok || !tim || !clut_deltas || count <= 0) {
        return false;
    }
    if (count > SH_NPC_CLUT_SLOTS) {
        count = SH_NPC_CLUT_SLOTS;
    }
    upload_sheets(&actor->tex, SH_NPC_TEX_W, SH_NPC_TEX_H, SH_NPC_SHEET_W, SH_NPC_SHEET_H, SH_NPC_SHEETS_PER_ROW,
                  tim, clut_deltas, count);
    actor->tex_ready = true;
    return true;
}

bool sh_gfx_item_set_tim(const ShTim *tim, unsigned clut_delta)
{
    if (!s_item_tex_ok || !tim) {
        return false;
    }
    upload_sheets(&s_item_tex, SH_ITEM_TEX_SIZE, SH_ITEM_TEX_SIZE, SH_ITEM_TEX_SIZE, SH_ITEM_TEX_SIZE, 1, tim,
                  &clut_delta, 1);
    s_item_tex_ready = true;
    return true;
}

bool sh_gfx_item_set_mesh(const ShMapVertex *verts, size_t count)
{
    if (!s_item_vbo || !verts || count < 3) {
        s_item_count = 0;
        return false;
    }
    if (count > SH_ITEM_MAX_VERTS) {
        count = SH_ITEM_MAX_VERTS;
    }
    memcpy(s_item_vbo, verts, count * sizeof(ShMapVertex));
    GSPGPU_FlushDataCache(s_item_vbo, count * sizeof(ShMapVertex));
    s_item_count = count;
    return true;
}

/* WorldEnv_WorldLightingParamSet / func_80055D78. The light envelope is in PS1 Q8 distance units. */
static void environment_uniforms(const ShEnvState *state, const ShEnvLight *light)
{
    if (!state) {
        C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_params, 0.0f, 128.0f, 128.0f, 1.0f / 128.0f);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_tint, 1.0f, 1.0f, 1.0f, 1.0f);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_light_pos, 0.0f, 0.0f, 0.0f, 0.0f);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_light_positive, 0.0f, 0.0f, 0.0f, 0.0f);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_light_negative, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }
    const ShEnvPreset *preset = &state->preset;
    C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_params, 1.0f, (float)(preset->world_light >> 5),
                 (float)preset->flags[1], 1.0f / 128.0f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_tint, (float)(preset->world_tint[0] >> 5) / 128.0f,
                 (float)(preset->world_tint[1] >> 5) / 128.0f, (float)(preset->world_tint[2] >> 5) / 128.0f, 1.0f);
    float positive[3] = {0};
    float negative[3] = {0};
    float base = 0.0f;
    if (light && preset->flags[2] != 0) {
        C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_light_pos, SH_Q12_TO_FLOAT(light->position[0]),
                     -SH_Q12_TO_FLOAT(light->position[1]), -SH_Q12_TO_FLOAT(light->position[2]), 0.0f);
        for (int axis = 0; axis < 3; axis++) {
            const int side = axis == 0 ? 0 : 1;
            positive[axis] = (float)light->plane[axis][side][2] / 16.0f;
            negative[axis] = (float)light->plane[axis][side ^ 1][2] / 16.0f;
        }
        base = (float)light->plane[0][0][1];
    } else {
        C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_light_pos, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_light_positive, positive[0], positive[1], positive[2], base);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, s_uloc_env_light_negative, negative[0], negative[1], negative[2], base);
}

/* WorldEnv_FogDistanceSet / func_800559A8, sampled in the 3DS projection's depth coordinate. */
static void environment_fog(const ShEnvState *state)
{
    if (!state) {
        C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
        C3D_FogColor(SH_FOG_RGB);
        C3D_FogLutBind(&s_fog_lut);
        return;
    }
    const ShEnvPreset *preset = &state->preset;
    C3D_FogColor((u32)preset->fog_color[0] | ((u32)preset->fog_color[1] << 8) | ((u32)preset->fog_color[2] << 16));
    if (preset->fog_mode == 0) {
        C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
        return;
    }
    if (!s_environment_fog_valid || s_environment_fog_distance != preset->fog_distance) {
        ShEnvFog ramp;
        if (!sh_env_fog_build(&ramp, preset->fog_distance)) {
            C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
            return;
        }
        float data[256];
        float previous = 0.0f;
        /* FogLut_FromArray stores 128 samples followed by their 128 forward differences. */
        for (int index = 0; index <= 128; index++) {
            const float z = FogLut_CalcZ((float)index / 128.0f, SH_MAP_NEAR, SH_MAP_FAR);
            const float density = 1.0f - SH_Q12_TO_FLOAT(sh_env_fog_alpha(&ramp, SH_Q12(z)));
            if (index < 128) {
                data[index] = density;
            }
            if (index > 0) {
                data[index + 127] = density - previous;
            }
            previous = density;
        }
        FogLut_FromArray(&s_environment_fog_lut, data);
        s_environment_fog_distance = preset->fog_distance;
        s_environment_fog_valid = true;
    }
    C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
    C3D_FogLutBind(&s_environment_fog_lut);
}

static void draw_item_overlay(void)
{
    if (!s_item_vbo || s_item_count < 3 || !s_item_tex_ready) {
        return;
    }
    C3D_Mtx identity;
    Mtx_Identity(&identity);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_uloc_model_view, &identity);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_uloc_world_model, &identity);
    environment_uniforms(NULL, NULL);

    C3D_BufInfo *buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, s_item_vbo, sizeof(ShMapVertex), 3, 0x210);

    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexBind(0, &s_item_tex);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    C3D_TexEnvScale(env, C3D_RGB, GPU_TEVSCALE_2);

    /* Own ordering table on PS1: drawn over the world, sorted back to front on the CPU. */
    C3D_DepthTest(true, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(true, GPU_GREATER, 0x80);
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    C3D_DrawArrays(GPU_TRIANGLES, 0, (int)s_item_count);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
}

void sh_gfx_actor_clear(int slot)
{
    if (slot >= 0 && slot < SH_GFX_MAX_ACTORS) {
        s_actors[slot].count = 0;
    }
}

/* Draws one character VBO with its own model matrix; `tex` NULL = vertex colors only. */
static void draw_character(const C3D_Mtx *view, const ShMapVertex *vbo, size_t count, const C3D_Tex *tex,
                           const float pos[3], float yaw)
{
    C3D_Mtx model, model_view;
    Mtx_Identity(&model);
    Mtx_Translate(&model, pos[0], pos[1], pos[2], true);
    Mtx_RotateY(&model, yaw, true);
    Mtx_Multiply(&model_view, view, &model);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_uloc_model_view, &model_view);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_uloc_world_model, &model);

    /* C3D_GetBufInfo marks the buffer state dirty; reusing the old pointer would
     * silently keep drawing from the map VBO. */
    C3D_BufInfo *buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, vbo, sizeof(ShMapVertex), 3, 0x210);

    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    if (tex) {
        C3D_TexBind(0, (C3D_Tex *)tex);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    } else {
        C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    }
    /* Drop TIM color-0 cutouts (hair edges, feathers…). */
    C3D_AlphaTest(true, GPU_GREATER, 0x80);
    if (s_draw_environment) {
        environment_fog(s_draw_environment);
    } else {
        C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    }
    C3D_DrawArrays(GPU_TRIANGLES, 0, (int)count);
    environment_fog(s_draw_environment);
}

static void draw_additive_particles(ShMapVertex *vertices, size_t count, C3D_Tex *texture, const C3D_Mtx *view, bool source_shades)
{
    if (!count) { return; }
    GSPGPU_FlushDataCache(vertices, count * sizeof(ShMapVertex));
    C3D_Mtx identity;
    Mtx_Identity(&identity);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_uloc_model_view, view);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_uloc_world_model, &identity);
    environment_uniforms(NULL, NULL);
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_COLOR);
    C3D_AlphaTest(true, GPU_GREATER, 0);
    /* BLD and WATER: additive pages, all nonzero texels have STP. */
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ZERO, GPU_ONE);
    C3D_BufInfo *buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, vertices, sizeof(ShMapVertex), 3, 0x210);
    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexBind(0, texture);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    if (source_shades) { C3D_TexEnvScale(env, C3D_RGB, GPU_TEVSCALE_2); }
    C3D_DrawArrays(GPU_TRIANGLES, 0, (int)count);
    if (source_shades) { C3D_TexEnvScale(env, C3D_RGB, GPU_TEVSCALE_1); }
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
}

static void draw_smoke(const ShSmoke *smoke, const ShCamera *camera, const C3D_Mtx *view)
{
    if (!s_smoke_ready || !smoke) { return; }
    s_smoke_draw_count = sh_smoke_build_mesh(smoke, camera, s_smoke_vbo, SH_SMOKE_CAPACITY * 6);
    draw_additive_particles(s_smoke_vbo, s_smoke_draw_count, &s_smoke_tex, view, false);
}

static void draw_water(const ShMapHud *hud, const ShCamera *camera, const C3D_Mtx *view)
{
    if (!s_water_ready || !hud) { return; }
    const size_t surface = sh_water_surface_build_mesh(hud->water_surface, hud->water_surface_count, camera,
        s_water_surface_vbo, SH_WATER_SURFACE_MAX_QUADS * 6);
    draw_additive_particles(s_water_surface_vbo, surface, &s_water_surface_tex, view, true);
    const size_t ribbons = sh_water_ribbon_build_mesh_visible(hud->water, hud->water_config, camera,
        hud->environment, hud->environment_light, s_water_vbo, SH_ROOF_WATER_POOL_COUNT * 6, hud->water_visibility);
    draw_additive_particles(s_water_vbo, ribbons, &s_water_ribbon_tex, view, false);
    s_water_draw_count = sh_water_spray_build_mesh_visible(hud->water, hud->water_spray, hud->water_spray_count,
        camera, hud->environment, hud->environment_light, s_water_vbo + ribbons, SH_ROOF_WATER_POOL_COUNT * 6 - ribbons, hud->water_visibility);
    draw_additive_particles(s_water_vbo + ribbons, s_water_draw_count, &s_water_tex, view, false);
    s_water_draw_count += ribbons + surface;
}

bool sh_gfx_prepare_moving_object(uint32_t mask)
{
    size_t count = 0;
    for (int range = 0; range < s_map_range_count; range++) {
        if (s_map_ranges[range].visibility_mask & mask) { count += s_map_ranges[range].count; }
    }
    if (!count) { return false; }
    if (count > s_moving_capacity) {
        ShMapVertex *vertices = linearAlloc(count * sizeof(*vertices));
        if (!vertices) { return false; }
        if (s_moving_vbo) { linearFree(s_moving_vbo); }
        s_moving_vbo = vertices;
        s_moving_capacity = count;
    }
    return true;
}

static void draw_map_geometry(const ShCamera *camera, const ShPlayerPose *player, const ShActorPose *actors,
                              int actor_count, const ShEnvState *environment, const ShEnvLight *light,
                              uint32_t world_object_mask, const ShSmoke *smoke,
                              uint32_t moving_object_mask, const ShSilverMotion *moving_silver, const ShMapHud *hud)
{
    C2D_Flush();

    C3D_BindProgram(&s_map_program);

    C3D_AttrInfo *attr = C3D_GetAttrInfo();
    AttrInfo_Init(attr);
    AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attr, 2, GPU_UNSIGNED_BYTE, 4);

    for (int i = 1; i < 6; i++) {
        C3D_TexEnvInit(C3D_GetTexEnv(i));
    }

    /* Gfx_MeshDraw: gte_nclip rejects back-facing map polygons. */
    C3D_CullFace(GPU_CULL_FRONT_CCW);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    /* PS1 color 0x0000 is a cutout (fences, foliage, signs). */
    C3D_AlphaTest(true, GPU_GREATER, 0x80);

    s_draw_environment = environment;
    environment_uniforms(environment, light);
    environment_fog(environment);

    C3D_Mtx projection;
    C3D_Mtx view;
    const float fov = camera->fov_deg > 10.0f && camera->fov_deg < 140.0f ? camera->fov_deg : SH_VC_FOV_DEG;
    Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(fov), C3D_AspectRatioTop, SH_MAP_NEAR, SH_MAP_FAR, false);
    Mtx_Identity(&view);
    Mtx_RotateX(&view, -camera->pitch, true);
    Mtx_RotateY(&view, -camera->yaw, true);
    Mtx_Translate(&view, -camera->pos[0], -camera->pos[1], -camera->pos[2], true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_uloc_projection, &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_uloc_model_view, &view);
    C3D_Mtx identity;
    Mtx_Identity(&identity);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_uloc_world_model, &identity);

    C3D_BufInfo *buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, s_map_vbo[s_map_front], sizeof(ShMapVertex), 3, 0x210);

    bool moving = false;
    size_t moving_cursor = 0;
    for (int r = 0; r < s_map_range_count; r++) {
        const ShMapRange *range = &s_map_ranges[r];
        if (range->visibility_mask && !(range->visibility_mask & world_object_mask)) {
            continue;
        }
        const bool range_moving = (range->visibility_mask & moving_object_mask) != 0;
        size_t range_first = range->first;
        if (range_moving) {
            if (!s_moving_vbo || range->count > s_moving_capacity - moving_cursor) { continue; }
            if (moving_silver) {
                sh_silver_transform_vertices(moving_silver, s_map_vbo[s_map_front] + range->first,
                                              s_moving_vbo + moving_cursor, range->count);
            } else if (hud && hud->moving_valve && (range->visibility_mask & hud->moving_valve_mask)) {
                sh_drain_valve_transform_vertices(hud->moving_valve->rotation, hud->moving_valve_pivot,
                    s_map_vbo[s_map_front] + range->first, s_moving_vbo + moving_cursor, range->count);
            } else if (hud && (range->visibility_mask & hud->moving_y_mask)) {
                sh_locker_transform_vertices(hud->moving_y_rotation, hud->moving_y_pivot,
                    s_map_vbo[s_map_front] + range->first, s_moving_vbo + moving_cursor, range->count);
            } else if (hud && (range->visibility_mask & hud->moving_vertical_mask)) {
                sh_world_vertical_vertices(hud->moving_vertical_q8, s_map_vbo[s_map_front] + range->first,
                    s_moving_vbo + moving_cursor, range->count);
            } else if (hud && hud->moving_rotations) {
                int door;
                for (door = 0; door < 3; door++) { if (range->visibility_mask & hud->moving_locker_masks[door]) { break; } }
                if (door == 3) { continue; }
                sh_locker_transform_vertices(hud->moving_rotations[door], hud->moving_locker_pivots[door],
                    s_map_vbo[s_map_front] + range->first, s_moving_vbo + moving_cursor, range->count);
            } else { continue; }
            GSPGPU_FlushDataCache(s_moving_vbo + moving_cursor, range->count * sizeof(ShMapVertex));
            range_first = moving_cursor;
            moving_cursor += range->count;
        }
        if (range_moving != moving) {
            moving = range_moving;
            C3D_BufInfo *current = C3D_GetBufInfo();
            BufInfo_Init(current);
            BufInfo_Add(current, moving ? s_moving_vbo : s_map_vbo[s_map_front], sizeof(ShMapVertex), 3, 0x210);
        }
        C3D_TexEnv *env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        if (range->page >= 0) {
            C3D_TexBind(0, &s_atlas[range->page]);
            C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
            C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
        } else {
            C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
            C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
        }

        const size_t end = range_first + range->count;
        if (range_moving) { s_moving_draw_count += range->count; }
        for (size_t first = range_first; first < end; first += SH_MAP_DRAW_BATCH) {
            const size_t left = end - first;
            C3D_DrawArrays(GPU_TRIANGLES, (int)first, (int)(left < SH_MAP_DRAW_BATCH ? left : SH_MAP_DRAW_BATCH));
        }
    }

    C3D_CullFace(GPU_CULL_NONE);
    if (player && player->visible && s_player_vbo && s_player_count >= 3) {
        draw_character(&view, s_player_vbo, s_player_count, s_harry_tex_ready ? &s_harry_tex : NULL, player->pos,
                       player->yaw);
    }

    for (int i = 0; i < actor_count && actors; i++) {
        const ShActorPose *a = &actors[i];
        if (!a->visible || a->slot < 0 || a->slot >= SH_GFX_MAX_ACTORS) {
            continue;
        }
        const ShActorGfx *g = &s_actors[a->slot];
        if (!g->vbo || g->count < 3) {
            continue;
        }
        draw_character(&view, g->vbo, g->count, g->tex_ready ? &g->tex : NULL, a->pos, a->yaw);
    }

    draw_smoke(smoke, camera, &view);
    draw_water(hud, camera, &view);
    draw_item_overlay();

    /* Hand the GPU back to Citro2D in the state it expects. */
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    C2D_Prepare();
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
}

static void draw_retail_borders(int32_t shade)
{
    uint8_t outer, inner;
    sh_border_colors(shade, &outer, &inner);
    const u32 outer_color = C2D_Color32(outer, outer, outer, 255);
    const u32 inner_color = C2D_Color32(inner, inner, inner, 255);
    C2D_Flush();
    /* cutscene_border.c D_800A8E98: PS1 subtractive semi-transparency. */
    C3D_AlphaBlend(GPU_BLEND_REVERSE_SUBTRACT, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ZERO, GPU_ONE);
    int count;
    const ShBorderRect *rectangles = sh_border_rects(&count);
    for (int i = 0; i < count; i++) {
        const ShBorderRect *rect = &rectangles[i];
        const float y = rect->h < 0 ? rect->y + rect->h : rect->y;
        const u32 top = rect->h < 0 ? inner_color : outer_color;
        const u32 bottom = rect->h < 0 ? outer_color : inner_color;
        C2D_DrawRectangle(PS1_TO_TOP_X(rect->x + 160), y + 120, 0.9f, rect->w, rect->h < 0 ? -rect->h : rect->h,
                           top, top, bottom, bottom);
    }
    C2D_Flush();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
}

void sh_gfx_draw_map(const ShCamera *camera, const ShPlayerPose *player, const ShActorPose *actors, int actor_count,
                     const ShMapHud *hud)
{
    s_smoke_draw_count = 0;
    s_water_draw_count = 0;
    s_moving_draw_count = 0;
    const u32 clear = C2D_Color32f(s_clear.r, s_clear.g, s_clear.b, s_clear.a);
    const u32 text = C2D_Color32(220, 220, 230, 255);
    const u32 muted = C2D_Color32(160, 160, 170, 255);
    const u32 dialog = C2D_Color32(230, 210, 180, 255);
    const u32 black = C2D_Color32(0, 0, 0, 255);
    const u32 shadow = C2D_Color32(0, 0, 0, 200);
    const u32 subtitle = C2D_Color32(236, 232, 220, 255);

    C2D_TextBufClear(s_text_buf);

    u32 fog_clear = SH_FOG_RGBA;
    if (hud && hud->environment) {
        const uint8_t *color = hud->environment->preset.fog_color;
        fog_clear = ((u32)color[0] << 24) | ((u32)color[1] << 16) | ((u32)color[2] << 8) | 255u;
    }
    C3D_RenderTargetClear(s_top, C3D_CLEAR_ALL, fog_clear, 0);
    C3D_FrameDrawOn(s_top);
    if (camera && s_map_count > 0) {
        draw_map_geometry(camera, player, actors, actor_count, hud ? hud->environment : NULL,
                          hud ? hud->environment_light : NULL, hud ? hud->world_object_mask : 0, hud ? hud->smoke : NULL,
                          hud ? hud->moving_object_mask : 0, hud ? hud->moving_silver : NULL, hud);
    }

    /* Top-screen overlay: paper map, cutscene borders, subtitles, fade. */
    if (hud && (hud->overlay || hud->piano_cursor || hud->letterbox || hud->retail_borders || hud->choice || (hud->subtitle && hud->subtitle[0]) || hud->fade > 0.0f ||
                hud->gun_flash > 0.0f)) {
        C2D_SceneBegin(s_top);
        if (hud->overlay && s_has_image) {
            C2D_DrawRectSolid(0.0f, 0.0f, 0.88f, (float)SH3DS_TOP_WIDTH, (float)SH3DS_TOP_HEIGHT, black);
            draw_paper_map(hud->overlay_scale > 0.0f ? hud->overlay_scale : 1.0f, hud->overlay_scroll_x,
                           hud->overlay_scroll_y, 0.9f);
            draw_map_sprites(hud->map_sprites, hud->map_sprite_count, 0.91f);
            if (hud->map_player_valid) {
                draw_map_player(hud->map_player_x, hud->map_player_y, hud->map_player_yaw, 0.915f);
            }
            draw_map_boxes((const float (*)[4])hud->map_boxes, hud->map_box_count, 0.92f);
        }
        if (hud->piano_cursor) { draw_piano_cursor(hud->piano_cursor_x, hud->piano_cursor_y); }
        if (hud->retail_borders) {
            draw_retail_borders(hud->border_shade);
        } else if (hud->letterbox) {
            C2D_DrawRectSolid(0.0f, 0.0f, 0.9f, (float)SH3DS_TOP_WIDTH, 28.0f, black);
            C2D_DrawRectSolid(0.0f, (float)SH3DS_TOP_HEIGHT - 28.0f, 0.9f, (float)SH3DS_TOP_WIDTH, 28.0f, black);
        }
        if (hud->subtitle && hud->subtitle[0]) {
            C2D_Text t;
            C2D_TextParse(&t, s_text_buf, hud->subtitle);
            C2D_TextOptimize(&t);
            float w = 0.0f, h = 0.0f;
            C2D_TextGetDimensions(&t, 0.5f, 0.5f, &w, &h);
            const float x = ((float)SH3DS_TOP_WIDTH - w) * 0.5f;
            const float y = (float)SH3DS_TOP_HEIGHT - 30.0f - h - (hud->letterbox ? 4.0f : 10.0f);
            C2D_DrawText(&t, C2D_WithColor, x + 1.0f, y + 1.0f, 0.95f, 0.5f, 0.5f, shadow);
            C2D_DrawText(&t, C2D_WithColor, x, y, 0.96f, 0.5f, 0.5f, subtitle);
        }
        if (hud->choice) {
            for (unsigned row = 0; row < hud->choice->count && row < 3; row++) {
                if (!hud->choice_options[row]) { continue; }
                int x, y;
                uint8_t rgb[3];
                sh_map_choice_row(hud->choice, row, &x, &y, rgb);
                C2D_Text text;
                C2D_TextParse(&text, s_text_buf, hud->choice_options[row]);
                C2D_TextOptimize(&text);
                C2D_DrawText(&text, C2D_WithColor, PS1_TO_TOP_X((float)x),
                    y, 0.96f, 0.5f, 0.5f, C2D_Color32(rgb[0], rgb[1], rgb[2], 255));
            }
        }
        if (hud->fade > 0.0f) {
            const u32 fade = C2D_Color32(0, 0, 0, (u8)(hud->fade > 1.0f ? 255 : hud->fade * 255.0f));
            C2D_DrawRectSolid(0.0f, 0.0f, 0.98f, (float)SH3DS_TOP_WIDTH, (float)SH3DS_TOP_HEIGHT, fade);
        }
        if (hud->gun_flash > 0.0f) {
            const float a = hud->gun_flash > 1.0f ? 1.0f : hud->gun_flash;
            const u32 wash = C2D_Color32(255, 236, 180, (u8)(a * 70.0f));
            C2D_DrawRectSolid(0.0f, 0.0f, 0.97f, (float)SH3DS_TOP_WIDTH, (float)SH3DS_TOP_HEIGHT, wash);
        }
    }

    C2D_TargetClear(s_bottom, clear);
    C2D_SceneBegin(s_bottom);
    if (hud) {
        draw_text_line(16.0f, 16.0f, 0.6f, hud->title, text);
        draw_text_line(16.0f, 48.0f, 0.42f, hud->info, dialog);
        draw_text_line(16.0f, 190.0f, 0.4f, hud->hint, muted);
    }
}
