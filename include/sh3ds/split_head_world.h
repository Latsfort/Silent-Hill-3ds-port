#pragma once
#include "sh3ds/split_head.h"
#include "sh3ds/coll.h"
#include "sh3ds/combat_ray.h"

/* Existing native map collision backend for Chara_MovementUpdate1 requests. */
bool sh_split_head_map_collision(const ShCollWorld *world, const ShSplitHead *boss,
    const ShPathPoint *offset, ShSplitMoveResult *result);
void sh_split_head_ray_shape(const ShSplitHead *boss, ShRayCharacter *character);
