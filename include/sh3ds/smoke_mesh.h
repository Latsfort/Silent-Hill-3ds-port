#pragma once
#include "sh3ds/smoke.h"
#include "sh3ds/gfx.h"
#include "sh3ds/smoke_render_data.h"

bool sh_smoke_decode_atlas(const ShTim *tim, uint32_t *tiled_rgba, size_t pixels);
size_t sh_smoke_build_mesh(const ShSmoke *smoke, const ShCamera *camera, ShMapVertex *out, size_t capacity);
