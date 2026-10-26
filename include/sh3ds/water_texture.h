#pragma once
#include "sh3ds/tim.h"
#include "sh3ds/water_render_data.h"

bool sh_water_spray_decode_atlas(const ShTim *tim, uint32_t *out, size_t pixels);
bool sh_water_ribbon_decode_atlas(const ShTim *tim, uint32_t *out, size_t pixels);
bool sh_water_surface_decode(const ShTim *tim, uint32_t *out, size_t pixels);
unsigned sh_water_spray_shade_slot(uint8_t shade);
