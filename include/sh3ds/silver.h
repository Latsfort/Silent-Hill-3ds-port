#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "sh3ds/map.h"

typedef struct ShSilverMotion {
    int32_t position[3]; /* PS1 Q12, Y down. */
    int16_t rotation[2]; /* PS1 Q12 angle, X/Y. */
    int32_t velocity;
} ShSilverMotion;

void sh_silver_init(ShSilverMotion *silver);
void sh_silver_ground(ShSilverMotion *silver);
void sh_silver_swing(ShSilverMotion *silver, int32_t delta);
bool sh_silver_fall(ShSilverMotion *silver, int32_t delta, int32_t gravity);
void sh_silver_pose(const ShSilverMotion *silver, float position[3], float rotation[3]);
void sh_silver_transform_vertices(const ShSilverMotion *silver, const ShMapVertex *source,
                                  ShMapVertex *destination, size_t count);
