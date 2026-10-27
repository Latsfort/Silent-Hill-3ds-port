#pragma once
#include "sh3ds/flame.h"
#include "sh3ds/chara.h"
#include "sh3ds/gfx.h"

bool sh_flame_decode(const ShTim *tim, uint32_t *tiled_rgba, size_t pixels);
void sh_flame_update(ShFlame *flame, const ShCamera *camera, const ShCharaBone *world_hand, float dt);
size_t sh_flame_build_mesh(const ShFlame *flame, const ShCamera *camera, ShMapVertex *out, size_t capacity);
