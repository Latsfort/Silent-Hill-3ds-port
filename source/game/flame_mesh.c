#include "sh3ds/flame_mesh.h"
#include "sh3ds/vc.h"
#include <math.h>
#include <string.h>

typedef struct FlameProjection {
    const ShCamera *camera;
    const ShCharaBone *hand;
    float right[3], up[3], forward[3], projection;
} FlameProjection;

static void flame_view(FlameProjection *view, const ShCamera *camera)
{
    const float sy = sinf(camera->yaw), cy = cosf(camera->yaw);
    const float sp = sinf(camera->pitch), cp = cosf(camera->pitch);
    view->camera = camera;
    view->right[0] = cy; view->right[1] = 0; view->right[2] = -sy;
    view->up[0] = sy * sp; view->up[1] = cp; view->up[2] = cy * sp;
    view->forward[0] = -sy * cp; view->forward[1] = sp; view->forward[2] = -cy * cp;
    const float fov = camera->fov_deg > 0 ? camera->fov_deg : SH_VC_FOV_DEG;
    view->projection = 120.0f / tanf(fov * 0.008726646259971648f);
}

static int32_t flame_project(void *user, bool hand, const ShFlamePoint *point, ShFlameScreen *screen)
{
    const FlameProjection *view = user;
    float x = point->vx, y = point->vy, depth = point->vz;
    if (hand) {
        const int32_t local[3] = {point->vx, point->vy, point->vz};
        float world[3];
        for (int row = 0; row < 3; row++) {
            int64_t sum = 0;
            for (int column = 0; column < 3; column++) {
                sum += (int64_t)view->hand->rotation[row][column] * local[column];
            }
            const int32_t position = (int32_t)(sum >> 12) + view->hand->translation[row];
            world[row] = (row == 0 ? position : -position) / 256.0f - view->camera->pos[row];
        }
        x = y = depth = 0;
        for (int axis = 0; axis < 3; axis++) {
            x += world[axis] * view->right[axis];
            y -= world[axis] * view->up[axis];
            depth += world[axis] * view->forward[axis];
        }
        x *= 256; y *= 256; depth *= 256;
    }
    if (depth <= 0) { screen->vx = screen->vy = 0; return 0; }
    screen->vx = (int16_t)floorf(x * view->projection / depth);
    screen->vy = (int16_t)floorf(y * view->projection / depth);
    return (int32_t)depth >> 2;
}

bool sh_flame_decode(const ShTim *tim, uint32_t *out, size_t pixels)
{
    if (!tim || !out || !tim->pixels || !tim->clut || tim->clut_colors < 16 || tim->clut_rows < 1 ||
        tim->bpp != 4 || tim->width != SH_FLAME_TEXTURE_W || tim->height != SH_FLAME_TEXTURE_H ||
        tim->stride < SH_FLAME_TEXTURE_W / 2 ||
        pixels < SH_FLAME_TEXTURE_W * SH_FLAME_TEXTURE_H) { return false; }
    for (unsigned y = 0; y < SH_FLAME_TEXTURE_H; y++) {
        for (unsigned x = 0; x < SH_FLAME_TEXTURE_W; x++) {
            const unsigned morton = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) |
                ((x & 4) << 2) | ((y & 4) << 3);
            const unsigned tiled = (((y >> 3) * (SH_FLAME_TEXTURE_W >> 3) + (x >> 3)) << 6) + morton;
            const uint8_t packed = tim->pixels[y * tim->stride + x / 2];
            const unsigned index = (packed >> ((x & 1) * 4)) & 15;
            const uint16_t ps1 = tim->clut[index * 2] | (uint16_t)tim->clut[index * 2 + 1] << 8;
            const uint32_t color = sh_tim_pixel_rgba(tim, x, y);
            out[tiled] = (color & 0xffffff00u) | (ps1 == 0 ? 0 : ps1 & 0x8000 ? 128 : 255);
        }
    }
    return true;
}

void sh_flame_update(ShFlame *flame, const ShCamera *camera, const ShCharaBone *world_hand, float dt)
{
    if (!flame || !camera || !world_hand) { return; }
    FlameProjection view;
    flame_view(&view, camera);
    view.hand = world_hand;
    if (!flame->started) { flame->phase = 1; flame->started = true; }
    else { flame->phase += (double)dt * 30.0; }
    bool advanced = false;
    while (flame->phase >= 1.0) {
        sh_flame_frame(flame, dt > 0, flame_project, &view);
        flame->phase -= 1.0;
        advanced = true;
    }
    if (!advanced) {
        /* Reproject the same source sprite between 30 Hz ticks without consuming RNG or changing its counter. */
        ShFlame display = *flame;
        display.counter = flame->draw_counter;
        sh_flame_frame(&display, false, flame_project, &view);
        flame->sprite = display.sprite;
        flame->visible = display.visible;
    }
}

size_t sh_flame_build_mesh(const ShFlame *flame, const ShCamera *camera, ShMapVertex *out, size_t capacity)
{
    if (!flame || !flame->visible || !camera || !out || capacity < 6) { return 0; }
    FlameProjection view;
    flame_view(&view, camera);
    const ShFlameQuad *q = &flame->sprite;
    const int16_t x[4] = {q->x0, q->x1, q->x2, q->x3};
    const int16_t y[4] = {q->y0, q->y1, q->y2, q->y3};
    const uint8_t u[4] = {q->u0, q->u1, q->u2, q->u3};
    const uint8_t v[4] = {q->v0, q->v1, q->v2, q->v3};
    const float depth = q->depth / 256.0f;
    const float pixel = depth / view.projection;
    ShMapVertex corners[4] = {0};
    for (unsigned corner = 0; corner < 4; corner++) {
        for (int axis = 0; axis < 3; axis++) {
            corners[corner].pos[axis] = camera->pos[axis] + view.forward[axis] * depth +
                view.right[axis] * (x[corner] * pixel) - view.up[axis] * (y[corner] * pixel);
        }
        corners[corner].uv[0] = (u[corner] - SH_FLAME_TEXTURE_U + 0.5f) / SH_FLAME_TEXTURE_W;
        corners[corner].uv[1] = 1.0f - (v[corner] + 0.5f) / SH_FLAME_TEXTURE_H;
        corners[corner].color[0] = corners[corner].color[1] = corners[corner].color[2] = q->shade;
        corners[corner].color[3] = 255;
    }
    out[0] = corners[0]; out[1] = corners[1]; out[2] = corners[2];
    out[3] = corners[2]; out[4] = corners[1]; out[5] = corners[3];
    return 6;
}
