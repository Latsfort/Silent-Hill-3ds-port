#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sh3ds/border_data.h"

typedef struct ShBorder { uint8_t state; int32_t shade; } ShBorder;
typedef struct ShBorderRect { int x, y, w, h; } ShBorderRect;

void sh_border_update(ShBorder *border, int32_t delta);
const ShBorderRect *sh_border_rects(int *count);
void sh_border_colors(int32_t shade, uint8_t *outer, uint8_t *inner);
