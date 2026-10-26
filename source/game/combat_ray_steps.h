/* Generated from collision/ray.c: Ray_TraceSetup, 8006EE0C, 8006EEB8, Ray_MissSet. */
bool sh_combat_ray_setup(ShRayState *state, bool useCylinder, int16_t radiusOffset, const ShPathPoint *from, const ShPathPoint *offset, int32_t arg5, int32_t arg6, ShRayCharacter **collCharas, int32_t collCharaCount, int16_t collision_flags)
{

    if (offset->vx == 0 && offset->vz == 0)
    {
        return false;
    }

    state->field_0      = useCylinder;
    state->field_4      = collision_flags;  
    state->radiusOffset = radiusOffset;
    state->hitDistance  = SHRT_MAX;
    state->character    = NULL;

    state->from.vx = RAY_TO_Q8(from->vx);
    state->from.vy = RAY_TO_Q8(from->vy);
    state->from.vz = RAY_TO_Q8(from->vz);

    state->offset.vx = RAY_TO_Q8(offset->vx);
    state->offset.vy = RAY_TO_Q8(offset->vy);
    state->offset.vz = RAY_TO_Q8(offset->vz);

    state->field_3C = state->from.vx + state->offset.vx;

    state->field_4C = RAY_TO_Q8(arg5);
    state->field_4E = RAY_TO_Q8(arg6);

    state->field_40 = state->from.vy + state->offset.vy;
    state->field_44 = state->from.vz + state->offset.vz;

    state->rayDistance = ray_magnitude(state->offset.vx, state->offset.vz);
    if (state->rayDistance == 0)
    {
        return false;
    }

    state->field_58 = (state->offset.vx * 4096) / state->rayDistance;
    state->field_5A = (state->offset.vz * 4096) / state->rayDistance;

    if (state->offset.vy < 0)
    {
        state->field_5E = state->from.vy + state->field_4E;
        state->field_60 = state->field_40 + state->field_4E;
    }
    else
    {
        state->field_60 = state->from.vy + state->field_4E;
        state->field_5E = state->field_40 + state->field_4E;
    }

    state->collCharas     = collCharas;
    state->collCharaCount = collCharaCount;

    return true;

}
static void ray_shape(ShRayShape *arg0, bool useCylinder, const ShRayCharacter *chara)
{

    int32_t offsetZ;
    int32_t offsetX;
    int32_t topHeight;

    if (useCylinder == true)
    {
        arg0->field_C = RAY_TO_Q8(chara->collision.cylinder.radius);
        offsetX       = chara->collision.shapeOffsets.cylinder.vx;
        offsetZ       = chara->collision.shapeOffsets.cylinder.vz;
        topHeight     = chara->position.vy + chara->collision.box.bottom;
    }
    else
    {
        arg0->field_C = RAY_TO_Q8(chara->collision.cylinder.field_2);
        offsetX       = chara->collision.shapeOffsets.box.vx;
        offsetZ       = chara->collision.shapeOffsets.box.vz;
        topHeight     = chara->position.vy + chara->collision.box.height;
    }

    arg0->topHeight    = RAY_TO_Q8(topHeight);
    arg0->positionX    = RAY_TO_Q8(chara->position.vx + offsetX);
    arg0->positionZ    = RAY_TO_Q8(chara->position.vz + offsetZ);
    arg0->groundHeight = RAY_TO_Q8(chara->position.vy + chara->collision.box.top);

}
static void ray_character(ShRayState *state, ShRayCharacter *chara)
{

    ShPathPoint offset; 
    int32_t     bound;
    int16_t   alpha;
    int16_t   clampedRayDist;
    int32_t   x1;
    int32_t   z1;
    int32_t   x0;
    int32_t   z0;
    int32_t  var_v1;

    if (state->from.vx <= state->field_3C)
    {
        x0 = state->from.vx;
        z0 = state->field_3C;
    }
    else
    {
        x0 = state->field_3C;
        z0 = state->from.vx;
    }

    if (state->from.vz <= state->field_44)
    {
        z1 = state->from.vz;
        x1 = state->field_44;
    }
    else
    {
        z1 = state->field_44;
        x1 = state->from.vz;
    }

    bound = state->field_6C.field_C;
    if ((state->field_6C.positionX + bound) < x0 ||
         z0 < (state->field_6C.positionX - bound))
    {
        return;
    }

    if ((state->field_6C.positionZ + bound) < z1 || x1 < (state->field_6C.positionZ - bound) ||
        ((state->from.vy + state->field_4E) < state->field_6C.groundHeight && (state->field_40 + state->field_4E) < state->field_6C.groundHeight) ||
        ((state->from.vy + state->field_4C) > state->field_6C.topHeight && state->field_6C.topHeight < (state->field_40 + state->field_4C)))
    {
        return;
    }

    alpha = ray_circle((uint32_t)state->field_58 | ((uint32_t)(uint16_t)state->field_5A << 16), state->rayDistance,
                          state->field_6C.positionX - state->from.vx,
                          state->field_6C.positionZ - state->from.vz,
                          bound);
    if (alpha == -1)
    {
        return;
    }

    clampedRayDist = RAY_MULT(state->rayDistance, alpha);
    if (clampedRayDist >= state->hitDistance)
    {
        return;
    }

    offset.vy = state->from.vy + (RAY_MULT(state->offset.vy, alpha));
    if ((offset.vy + state->field_4E) < state->field_6C.groundHeight ||
        state->field_6C.topHeight < (offset.vy + state->field_4C))
    {
        if (state->offset.vy == 0)
        {
            return;
        }

        if ((offset.vy + state->field_4E) < state->field_6C.groundHeight)
        {
            var_v1 = RAY_TO_Q12_SCALE(state->field_6C.groundHeight - (state->from.vy + state->field_4E)) / state->offset.vy;
            if (var_v1 > 4096)
            {
                return;
            }
            offset.vy = state->field_6C.groundHeight - state->field_4E;
        }
        else
        {
            var_v1 = RAY_TO_Q12_SCALE(state->field_6C.topHeight - (state->from.vy + state->field_4C)) / state->offset.vy;
            if (var_v1 > 4096)
            {
                return;
            }
            offset.vy = state->field_6C.topHeight - state->field_4C;
        }

        offset.vx = state->from.vx + RAY_MULT(state->offset.vx, var_v1);
        offset.vz = state->from.vz + RAY_MULT(state->offset.vz, var_v1);
        if ((RAY_SQUARE(state->field_6C.positionX - offset.vx) + RAY_SQUARE(state->field_6C.positionZ - offset.vz)) >= RAY_SQUARE(state->field_6C.field_C))
        {
            return;
        }
    }
    else
    {
        offset.vx = state->from.vx + RAY_MULT(state->offset.vx, alpha);
        offset.vz = state->from.vz + RAY_MULT(state->offset.vz, alpha);
    }

    state->hitDistance  = clampedRayDist;
    state->target.vx    = offset.vx;
    state->target.vy    = offset.vy;
    state->target.vz    = offset.vz;
    state->groundHeight = state->field_6C.groundHeight;
    state->offsetX      = offset.vx - state->field_6C.positionX;
    state->offsetZ      = offset.vz - state->field_6C.positionZ;
    state->character    = chara;
    state->groundType   = 0;

}
static void ray_miss(ShRayTrace *trace, const ShPathPoint *from, const ShPathPoint *offset, int32_t dist)
{

    trace->hasHit       = false;
    trace->groundType   = 0;
    trace->target.vx    = from->vx + offset->vx;
    trace->target.vy    = from->vy + offset->vy;
    trace->target.vz    = from->vz + offset->vz;
    trace->character    = NULL;
    trace->hitDistance  = RAY_TO_Q12(dist);
    trace->groundHeight = 7680;      
    trace->headingAngle = 0;

}
