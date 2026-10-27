#include "sh3ds/flame_mesh.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ProjectionProbe { int32_t quarter_depth; unsigned calls; ShFlamePoint hand; } ProjectionProbe;

static int32_t project(void *user, bool hand, const ShFlamePoint *point, ShFlameScreen *screen)
{
    ProjectionProbe *probe = user;
    probe->calls++;
    if (hand) {
        probe->hand = *point;
        screen->vx = 100; screen->vy = 200;
        return probe->quarter_depth;
    }
    screen->vx = point->vx; screen->vy = point->vy;
    return point->vz >> 2;
}

static ShFlame timed(int refresh)
{
    ShFlame flame = {0};
    ShCamera camera = {.pos = {0, 1, 0}, .fov_deg = 53.13f};
    ShCharaBone hand = {.rotation = {{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, .translation = {0, -256, 512}};
    sh_rng_set_seed(1);
    for (int i = 0; i < refresh * 2; i++) { sh_flame_update(&flame, &camera, &hand, 1.0f / refresh); }
    assert(flame.visible);
    ShMapVertex mesh[6];
    assert(sh_flame_build_mesh(&flame, &camera, mesh, 5) == 0);
    assert(sh_flame_build_mesh(&flame, &camera, mesh, 6) == 6);
    assert(mesh[0].uv[0] == 0.5f / SH_FLAME_TEXTURE_W);
    assert(mesh[1].uv[0] == (SH_FLAME_TEXTURE_W - 0.5f) / SH_FLAME_TEXTURE_W);
    for (int vertex = 0; vertex < 6; vertex++) {
        for (int axis = 0; axis < 3; axis++) { assert(isfinite(mesh[vertex].pos[axis])); }
        assert(mesh[vertex].uv[0] >= 0 && mesh[vertex].uv[0] <= 1);
        assert(mesh[vertex].uv[1] >= 0 && mesh[vertex].uv[1] <= 1);
        assert(mesh[vertex].color[0] == flame.sprite.shade);
    }
    return flame;
}

int main(void)
{
    uint32_t expected_seed;
    sh_rng_set_seed(123);
    for (int i = 0; i < 8; i++) { sh_rng_rand16(); }
    expected_seed = sh_rng_get_seed();
    sh_rng_set_seed(123);
    ShFlame flame = {0};
    ProjectionProbe probe = {.quarter_depth = 32};
    assert(!sh_flame_frame(&flame, true, project, &probe));
    assert(probe.calls == 1 && sh_rng_get_seed() == expected_seed);
    probe.quarter_depth = 33; probe.calls = 0;
    assert(sh_flame_frame(&flame, false, project, &probe));
    assert(probe.calls == 5 && sh_rng_get_seed() == expected_seed);
    assert(probe.hand.vx >= 1 && probe.hand.vx <= 5 && probe.hand.vy == -7);
    assert(probe.hand.vz >= 33 && probe.hand.vz <= 37);
    assert(flame.sprite.depth == 132 && flame.sprite.ordering == 14);
    assert(flame.sprite.tpage == 44 && flame.sprite.clut == 4146);
    assert(flame.sprite.u0 == 128 && flame.sprite.u1 == 191 && flame.sprite.v2 == 63);
    probe.quarter_depth = 4098; probe.calls = 0;
    assert(!sh_flame_frame(&flame, false, project, &probe) && probe.calls == 1);
    probe.quarter_depth = 4097;
    assert(sh_flame_frame(&flame, false, project, &probe));
    flame.counter = 0; flame.random[2] = 4095;
    assert(sh_flame_frame(&flame, false, project, &probe));
    assert(flame.counter == 0 && flame.sprite.shade == 48);
    probe.quarter_depth = 256;
    for (int i = 0; i < 10000; i++) {
        assert(sh_flame_frame(&flame, true, project, &probe));
        assert(flame.counter <= 32 && flame.sprite.shade >= 48 && flame.sprite.shade <= 80);
    }
    ShFlame a = timed(30), b = timed(60);
    assert(a.counter == b.counter && memcmp(a.random, b.random, sizeof(a.random)) == 0);
    assert(memcmp(&a.sprite, &b.sprite, sizeof(a.sprite)) == 0);
    ShCamera moving_camera = {.pos = {0, 1, 0}, .fov_deg = 53.13f};
    ShCharaBone moving_hand = {.rotation = {{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, .translation = {256, -256, 512}};
    const uint32_t before_seed = sh_rng_get_seed(), before_counter = b.counter;
    const int16_t before_x = b.sprite.x0;
    sh_flame_update(&b, &moving_camera, &moving_hand, 0.001f);
    assert(b.sprite.x0 != before_x && b.counter == before_counter && sh_rng_get_seed() == before_seed);

    FILE *file = fopen("romfs/game/tim/FLAME.TIM", "rb");
    assert(file);
    uint8_t bytes[SH_FLAME_TIM_BYTES];
    assert(fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes) && fclose(file) == 0);
    ShTim tim;
    assert(sh_tim_parse(bytes, sizeof(bytes), &tim));
    uint32_t rgba[SH_FLAME_TEXTURE_W * SH_FLAME_TEXTURE_H];
    assert(!sh_flame_decode(&tim, rgba, 1));
    assert(sh_flame_decode(&tim, rgba, sizeof(rgba) / sizeof(rgba[0])));
    ShTim no_palette = tim;
    no_palette.clut = NULL;
    assert(!sh_flame_decode(&no_palette, rgba, sizeof(rgba) / sizeof(rgba[0])));
    unsigned transparent = 0, half = 0;
    for (unsigned i = 0; i < sizeof(rgba) / sizeof(rgba[0]); i++) {
        const unsigned alpha = rgba[i] & 255;
        assert(alpha == 0 || alpha == 128);
        transparent += alpha == 0;
        half += alpha == 128;
    }
    assert(transparent > 0 && half > 0);
    puts("PASS: lighter RNG order, pause, clipping, unsigned color wrap, 30/60 Hz, geometry and original half-blend texture");
    return 0;
}
