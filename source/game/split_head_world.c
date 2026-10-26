#include "sh3ds/split_head_world.h"

void sh_split_head_ray_shape(const ShSplitHead *boss, ShRayCharacter *character)
{
    character->position = boss->position;
    character->collision.cylinder.radius = boss->hit_shape.cylinder_radius;
    character->collision.cylinder.field_2 = boss->hit_shape.radius;
    character->collision.box.top = boss->hit_shape.top;
    character->collision.box.height = boss->hit_shape.height;
    character->collision.box.bottom = boss->hit_shape.bottom;
    character->collision.shapeOffsets.box = boss->hit_shape.offset;
    character->collision.shapeOffsets.cylinder = boss->hit_shape.cylinder_offset;
}

bool sh_split_head_map_collision(const ShCollWorld *world, const ShSplitHead *boss,
    const ShPathPoint *offset, ShSplitMoveResult *result)
{
    if (!world || !result || !offset) { return false; }
    const float center_x = SH_Q12_TO_FLOAT(boss->position.vx + boss->hit_shape.cylinder_offset.vx);
    const float center_z = -SH_Q12_TO_FLOAT(boss->position.vz + boss->hit_shape.cylinder_offset.vz);
    const float feet = -SH_Q12_TO_FLOAT(boss->position.vy);
    ShCollHit ground;
    if (!sh_coll_ground_near(world, center_x, center_z, feet, &ground)) { return false; }
    float x = center_x, y = feet, z = center_z;
    sh_coll_move(world, &x, &y, &z, SH_Q12_TO_FLOAT(offset->vx), -SH_Q12_TO_FLOAT(offset->vz),
        SH_Q12_TO_FLOAT(boss->hit_shape.cylinder_radius), 0.0f);
    if (!sh_coll_ground_near(world, x, z, feet, &ground)) { return false; }
    result->offset = (ShPathPoint){SH_Q12(x - center_x), offset->vy, -SH_Q12(z - center_z)};
    result->ground_height = -SH_Q12(ground.height);
    return true;
}
