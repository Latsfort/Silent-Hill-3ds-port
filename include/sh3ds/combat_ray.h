#pragma once
#include "sh3ds/path.h"
#include <stdint.h>
#include <stdbool.h>
struct ShCollWorld;

typedef struct ShRayCharacter {
    ShPathPoint position;
    struct {
        struct { int32_t radius, field_2; } cylinder;
        struct { int32_t bottom, height, top; } box;
        struct { ShPathPoint cylinder, box; } shapeOffsets;
    } collision;
    void *user;
} ShRayCharacter;

typedef struct {
    int32_t positionX, positionZ;
    int16_t groundHeight, topHeight, field_C;
} ShRayShape;

typedef struct ShRayState {
    bool field_0;
    int16_t field_4, radiusOffset, hitDistance;
    ShPathPoint target;
    int16_t groundHeight;
    ShRayCharacter *character;
    int16_t offsetX, offsetZ;
    int32_t groundType;
    ShPathPoint from;
    int32_t field_3C, field_40, field_44;
    int16_t field_4C, field_4E;
    struct { int16_t vx, vy, vz; } offset;
    uint16_t field_58;
    int16_t field_5A, rayDistance, field_5E, field_60;
    ShRayCharacter **collCharas;
    int32_t collCharaCount;
    ShRayShape field_6C;
} ShRayState;

typedef struct {
    bool hasHit;
    int groundType;
    ShPathPoint target;
    ShRayCharacter *character;
    int32_t hitDistance, groundHeight;
    int16_t headingAngle;
} ShRayTrace;

bool sh_combat_ray_setup(ShRayState *state, bool useCylinder, int16_t radiusOffset,
                        const ShPathPoint *from, const ShPathPoint *offset,
                        int32_t arg5, int32_t arg6, ShRayCharacter **collCharas,
                        int32_t collCharaCount, int16_t collision_flags);
void sh_combat_ray_character(ShRayState *state, ShRayCharacter *character);
void sh_combat_ray_finish(ShRayTrace *trace, const ShRayState *state,
                          const ShPathPoint *from, const ShPathPoint *offset);
void sh_combat_ray_world(ShRayState *state, const struct ShCollWorld *world);
bool sh_combat_ray_query(ShRayTrace *trace, const struct ShCollWorld *world,
                         const ShPathPoint *from, const ShPathPoint *offset,
                         ShRayCharacter **characters, int count, bool cylinder,
                         int16_t collision_flags);
