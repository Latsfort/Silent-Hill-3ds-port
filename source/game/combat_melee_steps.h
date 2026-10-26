/* Generated from complete func_8008BF84; do not edit. */
static int32_t melee_apply(ShCombatActor *chara, int32_t angle, const ShAttackInfo *arg2, int32_t arg3, ShMeleeContext *ctx)
{

    ShCombatActor*   chara1;
    int32_t               sp14;
    int32_t               sp18;
    int32_t               i;
    int32_t            sinAngle;
    int32_t            cosAngle;
    int32_t               countX;
    int32_t               countY;
    int32_t               countZ;
    int32_t            sp34;
    int32_t               sp38;
    int32_t               sp3C;
    int32_t               sp40;
    int32_t               sp44;
    int32_t               sp48;
    int32_t               sp4C;
    int32_t            posX;
    int32_t            posZ;
    int32_t            sp58;
    int32_t               sp5C;
    int32_t               temp_s5;
    int32_t               temp_s6;
    int32_t            temp_a0;
    int32_t               temp_a0_2;
    int32_t            temp_a0_3;
    int32_t               temp_lo_5;
    int32_t               temp_s0;
    int32_t               temp_s1;
    int32_t            unkX;
    int32_t            temp_s3;
    int32_t            temp_t2;
    int32_t               j;
    int32_t            temp_t4;
    int32_t            temp_t4_2;
    int32_t            temp_t5;
    int32_t               angle1;
    int32_t               temp_v0_6;
    int32_t               temp_v0_7;
    int32_t            temp_v0_8;
    int32_t            var_a1_2;
    int32_t               var_fp;
    int32_t            unkZ;
    int32_t            var_s7;
    int32_t               var_t1;
    int32_t               var_t2;
    int32_t               var_v0;
    int32_t            var_v0_3;
    int32_t               var_v0_6;
    int32_t            var_v1;
    int32_t               temp_a2;
    uint16_t               temp_v1;
    int32_t            posY;
    int32_t               temp4;
    ShPathPoint    D_800C47B8;
    ShPathPoint    D_800C47C8[2];
    ShPathPoint    D_800C47E8;
    ShRayTrace D_800C47F8;

    countX = chara->previous[0].vx;
    countY = chara->previous[0].vy;
    countZ = chara->previous[0].vz;

    sinAngle = sh_fixed_sin(angle);
    cosAngle = sh_fixed_cos(angle);

    sp34   = chara->current[0].vx;
    var_fp = chara->current[0].vz;

    sp44 = 0;
    sp38 = 0;
    sp48 = 0;
    sp3C = 0;
    sp4C = 0;
    sp40 = 0;

    var_s7           = chara->current[0].vy;

    temp_t4          = sh_q12_mul(sp34, cosAngle) - sh_q12_mul(var_fp, sinAngle);

    temp_a0          = sh_q12_mul(sp34, sinAngle) + sh_q12_mul(var_fp, cosAngle);

    temp4 = var_s7;

    if (temp_t4 > 0)
    {
        sp38 = temp_t4;
    }
    else if (temp_t4 < 0)
    {
        sp44 = temp_t4;
    }

    if (sp3C < temp4)
    {
        sp3C = temp4;
    }
    else if (temp4 < sp48)
    {
        sp48 = temp4;
    }

    if (sp40 < temp_a0)
    {
        sp40 = temp_a0;
    }
    else if (temp_a0 < sp4C)
    {
        sp4C = temp_a0;
    }

    temp_v1   = ((uint16_t)(uint8_t)arg2->field_6 | ((uint16_t)(uint8_t)arg2->unk_7 << 8));
    temp_a0_2 = ((uint16_t)(uint8_t)arg2->field_8 | ((uint16_t)(uint8_t)arg2->chara_id << 8));
    temp_a2   = ((uint16_t)(uint8_t)arg2->field_a | ((uint16_t)(uint8_t)arg2->field_b << 8));

    var_v0    = (sp38 - sp44);
    temp_v0_7 = (sp3C - sp48);
    var_v0_6  = (sp40 - sp4C);

    if (var_v0 < temp_v1)
    {
        sp38 += ((uint16_t)(uint8_t)arg2->field_6 | ((uint16_t)(uint8_t)arg2->unk_7 << 8)) >> 1;
        sp44 -= ((uint16_t)(uint8_t)arg2->field_6 | ((uint16_t)(uint8_t)arg2->unk_7 << 8)) >> 1;
    }

    if (temp_v0_7 < temp_a0_2)
    {
        sp3C += ((uint16_t)(uint8_t)arg2->field_8 | ((uint16_t)(uint8_t)arg2->chara_id << 8)) >> 1;
        sp48 -= ((uint16_t)(uint8_t)arg2->field_8 | ((uint16_t)(uint8_t)arg2->chara_id << 8)) >> 1;
    }

    if (var_v0_6 < temp_a2)
    {
        sp40 += ((uint16_t)(uint8_t)arg2->field_a | ((uint16_t)(uint8_t)arg2->field_b << 8)) >> 1;
        sp4C -= ((uint16_t)(uint8_t)arg2->field_a | ((uint16_t)(uint8_t)arg2->field_b << 8)) >> 1;
    }

    if (chara->is_player)
    {
        var_v0 = 6;
        var_v1 = 1;
    }
    else
    {
        var_v0 = 1;
        var_v1 = -1;
    }

    sp18 = var_v1;
    
    sp14 = 0;

    posX = chara->position.vx;
    posZ = chara->position.vz;

    posY  = chara->position.vy;
    posY += chara->shape->collision.box.top;

    D_800C47C8[0].vx = posX;
    D_800C47C8[0].vy = posY;
    D_800C47C8[0].vz = posZ;

    for (i = var_v0; i > 0; i--, sp18 *= 2)
    {
        chara1 = chara->is_player ? ctx->npcs[6 - i] : ctx->player;
        if (!chara1) { continue; }
        if (ctx->game_over ||
            ctx->player->health <= 0 ||
            chara1 == chara ||
            chara1->chara_id == 0 ||
            chara1->health < 0 ||
            chara1->collision_state == 0)
        {
            continue;
        }

        D_800C47E8.vx = chara1->position.vx + chara1->shape->collision.shapeOffsets.box.vx;
        D_800C47E8.vy = chara1->position.vy;
        D_800C47E8.vz = chara1->position.vz + chara1->shape->collision.shapeOffsets.box.vz;
        if (melee_distance(&chara->position, &D_800C47E8) > 12288)
        {
            continue;
        }

        temp_s1 = chara1->position.vx;
        var_v1  = chara1->position.vy;
        unkX = chara1->position.vz;

        temp_s0 = chara1->shape->collision.shapeOffsets.box.vx;
        temp_s3 = chara1->shape->collision.shapeOffsets.box.vz;

        temp_s1 += temp_s0;

        var_v1  += chara1->shape->collision.box.top;
        unkX += temp_s3;

        D_800C47C8[1].vy = var_v1;
        temp_s3          = chara1->shape->collision.cylinder.field_2;

        angle1 = sh_fixed_atan2(temp_s1 - posX, unkX - posZ);
        temp_v0_6 = sh_fixed_sin(angle1);
        temp_s0   = sh_q12_mul(temp_s3, temp_v0_6);

        unkZ = sh_fixed_cos(angle1);

        posX = temp_s1 - temp_s0;

        D_800C47C8[1].vx = posX;
        posZ             = unkX - sh_q12_mul(temp_s3, unkZ);

        D_800C47C8[1].vz = posZ;
        var_v1           = melee_world_trace(&D_800C47F8, &D_800C47C8[0], &D_800C47C8[1], ctx);

        if (var_v1 != false)
        {
            var_v1 = true;
        }

        if (var_v1 == true)
        {
            continue;
        }

        temp_t4_2  = chara1->position.vx;
        temp_t4_2 += chara1->shape->collision.shapeOffsets.box.vx;

        var_v1   = temp_t4_2 - countX;
        temp_t5  = chara1->position.vz;
        temp_t5 += chara1->shape->collision.shapeOffsets.box.vz;

        temp_v0_8 = temp_t5 - countZ;

        sp58    = chara1->position.vy;
        temp_s6 = chara1->shape->collision.box.height;
        temp_s5 = chara1->shape->collision.box.top;

        j   = sp58 - countY;
        temp_t2   = sh_q12_mul(var_v1, cosAngle) - sh_q12_mul(temp_v0_8, sinAngle);
        temp_a0_3 = sh_q12_mul(var_v1, sinAngle) + sh_q12_mul(temp_v0_8, cosAngle);

        if ((j < (sp48 - temp_s6)) || ((sp3C - temp_s5) < j))
        {
            continue;
        }

        temp_s3  = chara1->shape->collision.cylinder.field_2;
        var_a1_2 = 0;

        if (temp_t2 < sp44)
        {
            var_a1_2 = sp44 - temp_t2;
        }
        else if (sp38 < temp_t2)
        {
            var_a1_2 = temp_t2 - sp38;
        }

        var_v1 = 0;

        if (temp_a0_3 < sp4C)
        {
            var_v1 = sp4C - temp_a0_3;
        }
        else if (sp40 < temp_a0_3)
        {
            var_v1 = temp_a0_3 - sp40;
        }

        if (var_a1_2 | var_v1)
        {
            if (var_a1_2 == 0)
            {
                if (temp_s3 < var_v1)
                {
                    continue;
                }
            }
            else
            {
                if (var_v1 == 0)
                {
                    if (temp_s3 < var_a1_2)
                    {
                        continue;
                    }
                }
                else
                {
                    var_v0_3 = sh_q12_mul(var_a1_2, var_a1_2) + sh_q12_mul(var_v1, var_v1);
                    if (sh_q12_mul(temp_s3, temp_s3) < var_v0_3)
                    {
                        continue;
                    }
                }
            }
        }

        var_v0 = sp34;
        if (var_v0 < 0)
        {
            var_v0 += 3;
        }
        sp34 -= var_v0 >> 2;

        var_v0 = var_s7;
        if (var_v0 < 0)
        {
            var_v0 += 3;
        }
        var_s7 -= var_v0 >> 2;

        var_v0 = var_fp;
        if (var_v0 < 0)
        {
            var_v0 += 3;
        }
        var_fp -= var_v0 >> 2;

        sp34   += countX;
        var_s7 += countY;
        var_fp += countZ;

        sp5C = (temp_s5 + temp_s6) / 2;

        unkX = sp34 - temp_t4_2;
        unkZ  = var_fp - temp_t5;
        temp_s0 = var_s7 - (sp58 + sp5C);

        var_t1 = melee_length(unkX, unkZ);
        var_t2 = (temp_s6 - temp_s5) / 2;

        if (var_t2 < 0)
        {
            var_t2 = 0;
        }

        for (j = 2; (var_t2 < temp_s0 || temp_s0 < -var_t2 || temp_s3 < var_t1) && j > 0; j--)
        {
            if (temp_s3 < var_t1)
            {
                temp_lo_5 = MELEE_Q12(temp_s3) / var_t1;
                var_t1    = temp_s3;
                unkX   = sh_q12_mul(temp_lo_5, unkX);
                temp_s0   = sh_q12_mul(temp_lo_5, temp_s0);
                unkZ    = sh_q12_mul(temp_lo_5, unkZ);
            }
            else if (var_t2 < temp_s0)
            {
                temp_lo_5 = MELEE_Q12(var_t2) / temp_s0;
                temp_s0   = var_t2;
                var_t1    = sh_q12_mul(temp_lo_5, var_t1);
                unkX   = sh_q12_mul(temp_lo_5, unkX);
                unkZ    = sh_q12_mul(temp_lo_5, unkZ);
            }
            else if (temp_s0 < -var_t2)
            {
                temp_lo_5 = MELEE_Q12(-var_t2) / temp_s0;
                temp_s0   = -var_t2;
                var_t1    = sh_q12_mul(temp_lo_5, var_t1);
                unkX   = sh_q12_mul(temp_lo_5, unkX);
                unkZ    = sh_q12_mul(temp_lo_5, unkZ);
            }
        }

        D_800C47B8.vx = temp_t4_2 + unkX;
        D_800C47B8.vy = (sp58 + sp5C) + temp_s0;
        D_800C47B8.vz = temp_t5 + unkZ;

        if (melee_damage(chara, chara1, &D_800C47B8, arg3, ctx))
        {
            sp14 |= sp18;
        }
    }

    chara->hit_mask |= sp14;
    return sp14;

}
