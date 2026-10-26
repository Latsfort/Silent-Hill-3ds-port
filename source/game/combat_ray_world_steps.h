/* Generated from retail collision/ray.c; explicit portable PS1 scratch/GTE adapters. */
static void world_cell(RayWorldWalk *arg0, uint32_t flags, int32_t posX, int32_t posZ)
{

    int32_t tempPosX;

    if (flags & 4)
    {
        tempPosX = posX;
        posX     = posZ;
        posZ     = tempPosX;
    }

    if (flags & 2)
    {
        posZ = -posZ;
    }

    if (flags & 1)
    {
        posX = -posX;
    }

    posX = WORLD_FROM(posX, 12);
    posZ = WORLD_FROM(posZ, 12);
    if (posX >= 0 && posX < arg0->field_10 &&
        posZ >= 0 && posZ < arg0->field_14)
    {
        arg0->field_20[arg0->field_1C].vx = posX;
        arg0->field_20[arg0->field_1C].vz = posZ;
        arg0->field_1C++;
    }

}
static void world_walk(RayWorldWalk *arg0, RayWorldPair arg1, RayWorldPair arg2)
{

    RayWorldPair subroutine_arg4;
    RayWorldVector  sp18;
    RayWorldVector  pos;
    int16_t     temp_lo;
    int16_t     temp_lo_2;
    int32_t     temp_a0_3;
    int32_t     temp_lo_4;
    int32_t     temp_t0;
    int32_t     temp_t1;
    int32_t     var_a2;
    int32_t     var_a3;
    uint32_t     flags;

    flags = 0;
    arg0->field_1C = 0;

    if (arg0->field_8.vx < 0 && arg0->field_C < 0 &&
        arg0->field_8.vy < 0 && arg0->field_E < 0)
    {
        return;
    }

    temp_lo   = arg0->field_10 * arg0->field_18;
    temp_lo_2 = arg0->field_14 * arg0->field_18;

    if (arg0->field_8.vx >= temp_lo && arg0->field_C >= temp_lo &&
        arg0->field_8.vy >= temp_lo_2 && arg0->field_E >= temp_lo_2)
    {
        return;
    }

    subroutine_arg4 = arg0->field_8;

    sp18.vz  = 0;
    sp18.pad = 0;

    sp18.vx = WORLD_Q12(arg0->field_10);
    sp18.vy = WORLD_Q12(arg0->field_14);

    if (arg1.vx < 0)
    {
        flags             |= 1;
        subroutine_arg4.vx = -subroutine_arg4.vx;

        arg1.vx = -arg1.vx;
        sp18.vz = -sp18.vx;
        sp18.vx = 0;
    }

    if (arg2.vx < 0)
    {
        flags             |= 2;
        subroutine_arg4.vy = -subroutine_arg4.vy;
        arg2.vx            = -arg2.vx;
        sp18.pad           = -sp18.vy;
        sp18.vy            = 0;
    }

    if (arg1.vx < arg2.vx)
    {
        flags |= 4;

        temp_a0_3          = subroutine_arg4.vx;
        subroutine_arg4.vx = subroutine_arg4.vy;
        subroutine_arg4.vy = temp_a0_3;

        temp_a0_3 = arg1.vx;
        arg1.vx   = arg2.vx;
        arg2.vx   = temp_a0_3;

        temp_a0_3 = sp18.vz;
        sp18.vz   = sp18.pad;
        sp18.pad  = temp_a0_3;
        temp_a0_3 = sp18.vx;
        sp18.vx   = sp18.vy;
        sp18.vy   = temp_a0_3;
    }

    if (subroutine_arg4.vx + arg1.vx < WORLD_MULT(arg0->field_18, sp18.vx))
    {
        sp18.vx = WORLD_Q12(subroutine_arg4.vx + arg1.vx) / arg0->field_18;
    }

    pos.vx = WORLD_Q12(subroutine_arg4.vx) / arg0->field_18;
    pos.vy = WORLD_Q12(subroutine_arg4.vy) / arg0->field_18;
    pos.vz = 4096;

    pos.pad  = WORLD_Q12(arg2.vx) / arg1.vx;
    temp_lo_4 = WORLD_MULT(pos.pad, WORLD_FRACT(pos.vx));

    if (WORLD_FROM(sp18.vx, 12) < WORLD_FROM(pos.vx, 12))
    {
        return;
    }

    do
    {
        world_cell(arg0, flags, pos.vx, pos.vy);

        temp_t0 = pos.vy;
        temp_t1 = pos.vx;
        var_a3  = temp_t0 + pos.pad;
        var_a2  = temp_t1 + 4096;

        pos.vy = var_a3;
        pos.vx = var_a2;

        if (WORLD_FROM(temp_t0, 12) < WORLD_FROM(var_a3, 12))
        {
            if (WORLD_FRACT(var_a3) < temp_lo_4)
            {
                world_cell(arg0, flags, var_a2, temp_t0);
            }
            else
            {
                world_cell(arg0, flags, temp_t1, var_a3);
            }
        }
    }
    while (arg0->field_1C < 20 && WORLD_FROM(pos.vx, 12) <= WORLD_FROM(sp18.vx, 12));

}
static void world_edge(ShRayState *state, RayWorldEdge *subcell, const uint8_t *splitVerts, const uint8_t *surfaces, bool hasSurface)
{

    RayWorldShort   sp0;
    RayWorldShort   sp8;
    RayWorldShort  maybeSplitDiff;
    int32_t     hitDist;
    int32_t    offsetX;
    int32_t    offsetZ;
    int32_t       var_a2;
    int32_t       groundType; 
    RayWorldShort* splitVert1;
    RayWorldShort* splitVert0;
    int32_t       unkXz;
    int32_t       var_v1;

    groundType = 0;
    RayWorldShort vertex1 = world_vertex(splitVerts, subcell->splitVertexIdx1);
    splitVert1 = &vertex1;
    RayWorldShort vertex0 = world_vertex(splitVerts, subcell->splitVertexIdx0);
    splitVert0 = &vertex0;

    if (state->field_5E >= splitVert1->vy || state->field_5E >= splitVert0->vy)
    {
        if (subcell->surfaceIdx0 != UCHAR_MAX)
        {
            groundType = world_ground(surfaces, subcell->surfaceIdx0);
        }
        if (subcell->surfaceIdx1 != UCHAR_MAX)
        {
            groundType = world_ground(surfaces, subcell->surfaceIdx1);
        }

        unkXz = world_packed(state);
        sp0 = world_project(unkXz, splitVert1->vx - state->field_6C.groundHeight, splitVert1->vz - state->field_6C.topHeight);
        sp8 = world_project(unkXz, splitVert0->vx - state->field_6C.groundHeight, splitVert0->vz - state->field_6C.topHeight);

        if ((sp0.vy & 0x8000) != (sp8.vy & 0x8000))
        {
            if (state->field_0 == true)
            {
                

                if (world_nclip(sp0, sp8) >= 0)
                {
                    if (subcell->surfaceIdx0 != UCHAR_MAX)
                    {
                        return;
                    }
                }
                else
                {
                    if (subcell->surfaceIdx1 != UCHAR_MAX)
                    {
                        return;
                    }
                }
            }

            if (sp0.vy != sp8.vy)
            {
                var_v1 = ((sp0.vy * 4096) / (sp0.vy - sp8.vy));
                hitDist = (((sp8.vx - sp0.vx) * var_v1) >> 12) + sp0.vx;
                if (hitDist >= 0 && state->rayDistance >= hitDist)
                {
                    maybeSplitDiff = world_gpf(var_v1, splitVert0->vx - splitVert1->vx, splitVert0->vy - splitVert1->vy, splitVert0->vz - splitVert1->vz);

                    maybeSplitDiff.vx += splitVert1->vx;
                    maybeSplitDiff.vy += splitVert1->vy;
                    maybeSplitDiff.vz += splitVert1->vz;

                    var_a2 = state->from.vy + state->field_4E;
                    if (state->offset.vy != 0)
                    {
                        var_a2 += (state->offset.vy * hitDist) / state->rayDistance;
                    }

                    if (var_a2 >= maybeSplitDiff.vy && hitDist < state->hitDistance)
                    {
                        offsetX = subcell->field_2_0;
                        offsetZ = -subcell->field_0_0;
                        if (state->field_0 != true && hasSurface && (sp8.vy - sp0.vy) > 0)
                        {
                            offsetX = -offsetX;
                            offsetZ = subcell->field_0_0;
                        }

                        state->hitDistance  = hitDist;
                        state->target.vx    = (maybeSplitDiff.vx + state->field_6C.positionX);
                        state->target.vy    = (var_a2 - state->field_4E);
                        state->target.vz    = (maybeSplitDiff.vz + state->field_6C.positionZ);
                        state->groundHeight = maybeSplitDiff.vy;
                        state->offsetX      = offsetX;
                        state->offsetZ      = offsetZ;
                        state->character    = NULL;
                        state->groundType   = groundType;
                    }
                }
            }
        }
    }

}
static void world_prop(ShRayState *state, RayWorldProp *arg1)
{

    RayWorldShort sp10;
    RayWorldShort sp18; 
    int16_t    hitDist;
    int32_t   temp_v0;
    int16_t    radiusOffset;
    int32_t     temp_v1;

    radiusOffset = arg1->radiusOffset;
    if (state->field_5E <= arg1->offset.vy)
    {
        return;
    }

    temp_v1 = world_packed(state);
    sp10 = world_project(temp_v1, arg1->offset.vx - state->field_6C.groundHeight, arg1->offset.vz - state->field_6C.topHeight);

    if (-radiusOffset < sp10.vx && sp10.vx < (state->rayDistance + radiusOffset) &&
        -radiusOffset < sp10.vy && sp10.vy < radiusOffset)
    {
        temp_v0   = sh_fixed_sqrt0(WORLD_SQUARE(radiusOffset) - WORLD_SQUARE(sp10.vy));
        hitDist = sp10.vx - temp_v0;

        if (hitDist >= -temp_v0 && state->rayDistance >= hitDist && hitDist < state->hitDistance)
        {
            sp18 = world_gpf(WORLD_Q12(hitDist) / state->rayDistance, state->offset.vx, state->offset.vy, state->offset.vz);

            if ((sp18.vy + state->from.vy + state->field_4E) >= arg1->offset.vy)
            {
                state->hitDistance  = hitDist;
                state->target.vx    = sp18.vx + state->field_6C.groundHeight + state->field_6C.positionX;
                state->target.vy    = sp18.vy + state->from.vy;
                state->target.vz    = sp18.vz + state->field_6C.topHeight + state->field_6C.positionZ;
                state->groundHeight = arg1->offset.vy;
                state->offsetX      = (sp18.vx + state->field_6C.groundHeight) - arg1->offset.vx;
                state->offsetZ      = (sp18.vz + state->field_6C.topHeight)    - arg1->offset.vz;
                state->character    = NULL;
                state->groundType   = arg1->groundType;
            }
        }
    }

}
static void world_range(ShRayState *state, const uint8_t *subcellRanges, const ShCollChunk *collData, bool checked[256])
{

    int32_t                    i;
    int32_t                    temp_v0;
    int32_t                    temp_v0_2;
    bool                   hasSurface;
    bool                   cond1;
    bool                   cond2;
    int32_t                    surfaceIdx0;
    int32_t                    surfaceIdx1;
    int32_t                    idx;
    RayWorldProp* temp_a1_2;
    RayWorldEdge*      curSubcell;

    for (i = world_read16(subcellRanges); i < world_read16(subcellRanges + 4); i++)
    {
        idx = collData->ptr28[i];

        if (!checked[idx])
        {
            checked[idx] = true;

            if (idx < collData->sub_n)
            {
                RayWorldEdge decoded_edge = world_decode_edge(collData->subcells + idx * 10);
                curSubcell = &decoded_edge;

                temp_v0 = (uint16_t)state->field_4 >> (curSubcell->field_0_14 * 4 | curSubcell->field_2_14);

                if (temp_v0 & (1 << 0))
                {
                    surfaceIdx0 = curSubcell->surfaceIdx0;
                    surfaceIdx1   = curSubcell->surfaceIdx1;

                    hasSurface = surfaceIdx0 != UCHAR_MAX && surfaceIdx1 != UCHAR_MAX;

                    if (state->field_0 == true)
                    {
                        if (hasSurface)
                        {
                            continue;
                        }
                    }
                    else
                    {
                        cond1 = false;
                        cond2 = false;

                        if (surfaceIdx0 == UCHAR_MAX || world_ground(collData->surfaces, surfaceIdx0) == 0 ||
                            world_ground(collData->surfaces, surfaceIdx0) == 12)
                        {
                            cond1 = true;
                        }

                        if (surfaceIdx1 == UCHAR_MAX || world_ground(collData->surfaces, surfaceIdx1) == 0 ||
                            world_ground(collData->surfaces, surfaceIdx1) == 12)
                        {
                            cond2 = true;
                        }

                        if (cond1 && cond2)
                        {
                            continue;
                        }
                    }

                    world_edge(state, curSubcell, collData->split_vertices, collData->surfaces, hasSurface);
                }
            }
            else
            {
                RayWorldProp decoded_prop = world_decode_prop(collData->ptr18 + (idx - collData->sub_n) * 10);
                temp_a1_2 = &decoded_prop;
                temp_v0_2 = (uint16_t)state->field_4 >> temp_a1_2->field_0_8;

                if ((temp_v0_2 & (1 << 0)) &&
                    (state->field_0 == true ||
                     (temp_a1_2->groundType != 0 &&
                      temp_a1_2->groundType != 12)) &&
                    temp_a1_2->radiusOffset >= state->radiusOffset)
                {
                    world_prop(state, temp_a1_2);
                }
            }
        }
    }

}
