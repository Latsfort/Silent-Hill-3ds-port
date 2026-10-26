#pragma once

#include "sh3ds/tim.h"

bool sh_event_image_transition(ShTim *out, uint8_t *pixels, size_t capacity,
                               const ShTim *from, const ShTim *to, float alpha);
bool sh_event_image_shade(ShTim *out, uint8_t *pixels, size_t capacity, const ShTim *source, unsigned shade);
