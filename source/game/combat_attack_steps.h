/* Generated from complete func_8008A3E0/8008B15C/8008B1DC and attack timing helpers; do not edit. */
static const int16_t s_attack_angles[16] = {4096, 4096, 2048, 1365, 1024, 819, 682, 585, 512, 455, 409, 372, 341, 315, 292, 273};
static uint32_t attack_start(ShAttackContext *ctx, int32_t idx)
{


    switch (idx)
    {
        case 2:
        case 5:
        case 12:
        case 15:
        case 22:
        case 25:
            if (ctx->damage->charge > 1048576)
            {
                return 0;
            }
            break;

        default:
            break;
    }


    return sh_combat_attack_info((unsigned)idx)->field_e;

}
static uint32_t attack_duration(ShAttackContext *ctx, int32_t arg0)
{

    if (ctx->damage->charge > 1048576)
    {
        switch (arg0)
        {
            case 5:
            case 25:
                return 18;

            case 15:
                return 15;

            case 2:
            case 12:
            case 22:
                return 13;
        }
    }

    return sh_combat_attack_info((unsigned)arg0)->field_f;

}
static void attack_vector(ShCombatActor *chara, int32_t angle0, int32_t angle1)
{

    int32_t                temp_s0;
    int32_t             sinAngle1;
    int32_t             cosAngle1;
    int32_t                temp_s3;
    int32_t                temp_v1_2;
    int32_t                i;
    int32_t                temp_s2;
    int32_t                idx46;
    int32_t                vx;
    int32_t                vy;
    int32_t                vz;
    int32_t                vx2;
    int32_t                vy2;
    int32_t                vz2;
    int32_t                vx3;
    int32_t                vy3;
    int32_t                vz3;
    const ShAttackInfo*        ptr;
    ShCombatActor* base;

    base  = chara;
    idx46 = chara->attack;

    for (i = 0; i >= 0; i--)
    {
        vx = base->current[i].vx;
        vy = base->current[i].vy;
        vz = base->current[i].vz;

        base->current[i + 1].vx = vx;
        base->current[i + 1].vy = vy;
        base->current[i + 1].vz = vz;

        vx2 = base->previous[i].vx;
        vy2 = base->previous[i].vy;
        vz2 = base->previous[i].vz;

        base->previous[i + 1].vx = vx2;
        base->previous[i + 1].vy = vy2;
        base->previous[i + 1].vz = vz2;
    }

    ptr = sh_combat_attack_info((unsigned)idx46);

    temp_s2 = ptr->field_0;
    temp_s0 = ptr->field_2;

    vx3 = chara->attack_origin.vx;
    vy3 = chara->attack_origin.vy;
    vz3 = chara->attack_origin.vz;

    vx2 = vx3;
    vy2 = vy3;
    vz2 = vz3;

    cosAngle1 = sh_fixed_cos(angle1);
    sinAngle1 = sh_fixed_sin(angle1);

    temp_s3   = ATTACK_MULT(sinAngle1, sh_fixed_sin(angle0));
    temp_v1_2 = ATTACK_MULT(sinAngle1, sh_fixed_cos(angle0));

    temp_s2 -= temp_s0;

    vx2                          += ATTACK_MULT(temp_s0, temp_s3);
    chara->previous[0].vx = vx2;

    vy2                          += ATTACK_MULT(temp_s0, cosAngle1);
    chara->previous[0].vy = vy2;

    vz2                          += ATTACK_MULT(temp_s0, temp_v1_2);
    chara->previous[0].vz = vz2;

    vx                            = ATTACK_MULT(temp_s2, temp_s3);
    chara->current[0].vx = vx;

    vy                            = ATTACK_MULT(temp_s2, cosAngle1);
    chara->current[0].vy = vy;

    vz                            = ATTACK_MULT(temp_s2, temp_v1_2);
    chara->current[0].vz = vz;

}
static void attack_begin(ShCombatActor *chara)
{

    int32_t temp_s2;
    int32_t temp_s3;
    int32_t i;

    temp_s3 = chara->angle_c;
    temp_s2 = chara->angle_e;

    chara->angle_10 = temp_s3;
    chara->angle_12 = temp_s2;

    for (i = 1; i > 0; i--)
    {
        attack_vector(chara, temp_s3, temp_s2);
        chara->previous[2] = chara->attack_origin;
    }

}
static int32_t attack_update(ShCombatActor *chara, ShAttackContext *ctx)
{

    int32_t                sp10;
    int32_t                sp14;
    int32_t                sp18;
    const ShAttackInfo*        sp1C;
    int32_t                sp20;
    int32_t                sp24;
    int32_t                sp28;
    int32_t                sp2C;
    int32_t                sp30;
    int32_t                sp34;
    uint32_t                sp38;
    int32_t                sp3C;
    uint32_t                sp40;
    int32_t                charaId;
    int32_t                sp48;
    int32_t                sp4C;
    int32_t                sp50;
    int32_t                sp54;
    int32_t                sp58;
    int32_t                sp5C;
    int32_t                sp60;
    int32_t                sp64;
    int32_t                sp68;
    int32_t                sp6C;
    int32_t                sp70;
    int32_t                sp74;
    int32_t             sp78;
    int32_t                sp7C;
    int32_t             sp80;
    int32_t                sp84;
    int32_t             sp88;
    int32_t             sp8C;
    int32_t                sp90;
    int32_t                sp94;
    ShAttackAngles*           temp_s0_14;
    ShPathPoint*           temp_a0;
    ShPathPoint*           temp_v0_11;
    ShCombatActor* temp_v0_12;
    int32_t                temp_s0;
    int32_t                temp_s0_11;
    int32_t             angle;
    int32_t                temp_s0_7;
    int32_t                temp_s0_8;
    int32_t                temp_s0_9;
    int32_t             temp_s0_10;
    int32_t                var_s0_2;
    int32_t                temp_s1_3;
    int32_t                temp_s1_4;
    int32_t             temp_s1_5;
    int32_t                temp_s2_2;
    int32_t                var_s2;
    int32_t                temp_v0_10;
    int32_t                temp_v0_4;
    int32_t                temp_v0_5;
    int32_t                temp_v0_6;
    int32_t                temp_v1;
    int32_t                var_a0_2;
    int32_t                var_a0_3;
    int32_t                temp_a1;
    int32_t                var_a1;
    int32_t                var_fp;
    int32_t                var_s3;
    int32_t                j;
    int32_t                var_s6;
    int32_t                i;
    int32_t                var_v1;
    ShCombatActor*    temp_s1_2;
    uint32_t                temp_s1;
    int32_t                var_a0;
    ShCombatActor*    ptr;
    bool               hasHit;
    int32_t                temp_t3_2;
    int32_t                temp2;
    int32_t                temp3;
    int32_t                temp4;
    int32_t                temp5;
    int32_t                temp6;
    ShAttackTrace  D_800C4728;
    ShAttackAngles     D_800C4748[4];
    ShPathPoint     D_800C4758[4];

    sp30 = -1;
    chara->flags |= 128;

    sp10 = chara->attack_state;
    sp14 = (uint8_t)chara->attack;

    if (sp10 == 0)
    {
        chara->shot_count  = 0;
        chara->hit_mask  = 0;
        chara->attack_frame = 0;
        return 0;
    }

    if (sp10 == sp30)
    {
        chara->hit_mask  = 0;
        chara->attack_frame = 0;
        return 0;
    }

    sp18 = 0;

    sp1C = sh_combat_attack_info((unsigned)sp14);

    temp_s1 = sp1C->field_10;

    sp28 = ctx->animation_time - ctx->animation_start * 4096;

    var_s0_2 = attack_start(ctx, sp14);
    var_a0   = attack_duration(ctx, sp14);

    i   = chara->attack_frame;
    sp2C     = ATTACK_Q12(var_s0_2 + var_a0);
    var_s0_2 = ATTACK_Q12(var_s0_2);

    if (sp28 < i)
    {
        sp10                   = 1;
        chara->attack_state = sp10;
    }

    if (sp10 == 1)
    {
        chara->shot_count = 0;
        sp10                   = 2;
    }

    if (i < var_s0_2)
    {
        i                  = var_s0_2;
        chara->attack_frame = i;
    }
    else if (sp2C < i)
    {
        i                  = sp2C;
        chara->attack_frame = i;
    }

    if (sp28 < var_s0_2)
    {
        return 0;
    }

    if (ctx->delta != 0)
    {
        if (sp28 >= sp2C)
        {
            sp28 = sp2C;
            sp18 = 1;
        }

        sp20 = chara->angle_c;
        sp24 = chara->angle_e;

        if (temp_s1 == 3)
        {
            sp3C = chara->shot_budget;
            sp40 = chara->shot_count;
            sp34 = ((uint16_t)(uint8_t)sp1C->field_6 | ((uint16_t)(uint8_t)sp1C->unk_7 << 8));

            sp54 = sp1C->field_0;

            if (sp34 != 0)
            {
                sp38 = sp1C->field_8;
                if (sp38 == 0)
                {
                    sp38 = 1;
                }
            }
            else
            {
                sp38 = 1;
            }

            sp50 = sp1C->field_b;
            sp4C = sp50 >> 4;
            sp48 = sp1C->field_a;
            sp4C--;
            sp4C &= 0xF;
            sp50  = s_attack_angles[sp50 & 0xF];
            sp4C++;
            charaId = sp1C->chara_id;
            sp4C = sp4C * sp50;

            if (sp34 == 0 && charaId == 1)
            {
                if (chara->chara_id == charaId)
                {
                    var_a0_2 = ctx->player_accuracy;
                }
                else
                {
                    var_a0_2 = 8192;
                }

                if (var_a0_2 < 2048)
                {
                    var_a0_2 = 2048;
                }
                else if (var_a0_2 > 8192)
                {
                    var_a0_2 = 8192;
                }

                temp_v1 = ATTACK_Q12(var_a0_2 - 2048) / 6144;
                var_s2  = sh_q12_mul(temp_v1, -6144) + 7168;
                var_s2  = sh_q12_mul(var_s2, sp48);

                if (var_s2 < 170)
                {
                    if (ctx->dark)
                    {
                        var_s2 = (var_s2 + 170) / 2;
                    }

                    
                    goto j_3d8;
                j_3a8:
                    sp30 = 3;
                    goto j_584;
                j_3b0:
                    sp30 = 2;
                    goto j_584;
                j_3bc:
                    sp30    = 1;
                    var_s2 -= var_s2 / 2;
                    goto j_584;
                }
                else
                {
                    var_s2 = 170;
                }

            j_3d8:
                temp_t3_2 = sp54 / 3;
                sp30      = -1;

                if (ctx->target_index >= 0)
                {
                    temp_s1_2 = ctx->npcs[ctx->target_index];

                    if (temp_s1_2->chara_id >= 2)
                    {
                        do
                        {
                            if (temp_s1_2->chara_id <= 24)
                            {
                                temp_v0_4 = chara->position.vx - temp_s1_2->position.vx;
                                temp_v0_5 = chara->position.vy - temp_s1_2->position.vy;
                                temp_v0_6 = chara->position.vz - temp_s1_2->position.vz;

                                temp6   = sh_q12_mul(temp_v0_4, temp_v0_4) + sh_q12_mul(temp_v0_5, temp_v0_5);
                                temp_s0 = sh_q12_mul(temp_v0_6, temp_v0_6) + temp6;

                                sp30 = sh_fixed_sqrt12(temp_s0);

                                if (temp_s1_2->flags & 2)
                                {
                                    temp_s0 = temp_s0 - 0x800;
                                    if (temp_s0 < 0)
                                    {
                                        var_s2 = 0;
                                    }
                                    else if (temp_s0 <= 0xFFFF)
                                    {
                                        temp_v0_10 = ATTACK_Q12(temp_s0);
                                        if (temp_v0_10 < 0)
                                        {
                                            temp_v0_10 += 0xFFFF;
                                        }
                                        var_s2 = sh_q12_mul(var_s2, temp_v0_10 >> 16);
                                    }
                                }

                                if (!(ctx->dark))
                                {
                                    if (temp_t3_2 >= sp30)
                                    {
                                        if (sp30 <= 8192)
                                        {
                                            sp30   = 0;
                                            var_s2 = 0;
                                        }
                                        else
                                        {
                                            goto j_3bc;
                                            
                                            
                                        }
                                    }
                                    else
                                    {
                                        goto j_3b0;
                                        
                                    }
                                }
                                else
                                {
                                    goto j_3a8;
                                    
                                }
                            }
                        } while (0); 
                    }
                }

            j_584:
                sp48 = sh_q12_mul(var_s2, attack_random());
            }

            var_s6 = ATTACK_MULT(sp4C, sp10 - 2);

            for (i = i + sp34;
                 sp3C != 0 && sp40 < sp38 && sp28 >= i;
                 i += sp34, sp40++, sp3C--)
            {
                for (j = charaId; j > 0; j--)
                {
                    temp_v0_10 = attack_random();
                    temp_s2_2  = sh_q12_mul(sp48, temp_v0_10);

                    temp_v0_10 = attack_random() - 2048;
                    angle  = var_s6 + sh_q12_mul(sp50, temp_v0_10);

                    temp3     = sp20 + ATTACK_MULT(temp_s2_2, sh_fixed_sin(angle));
                    temp_s0_7 = sp24 + ATTACK_MULT(temp_s2_2, sh_fixed_cos(angle));
                    temp2     = sh_fixed_cos(temp_s0_7);
                    temp_s0_8 = sh_fixed_sin(temp_s0_7);
                    temp_s1_4 = ATTACK_MULT(temp_s0_8, sh_fixed_sin(temp3));
                    temp_s0_9 = ATTACK_MULT(temp_s0_8, sh_fixed_cos(temp3));

                    chara->current[0].vx = ATTACK_MULT(sp54, temp_s1_4);
                    chara->current[0].vy = ATTACK_MULT(sp54, temp2);
                    chara->current[0].vz = ATTACK_MULT(sp54, temp_s0_9);

                    var_s6 += sp4C;

                    if (chara->chara_id == 1)
                    {
                        ctx->damage->rumble(ctx->damage->user, 5);
                    }

                    hasHit = ctx->trace(ctx->user, &chara->attack_origin, &chara->current[0], chara, &D_800C4728);
                    ptr  = D_800C4728.character;
                    if (hasHit && ptr != NULL)
                    {
                        chara->current[1].vx = temp_s1_4;
                        chara->current[1].vy = temp2;
                        chara->current[1].vz = temp_s0_9;
                        attack_damage(chara, ptr, &D_800C4728.target, ctx);
                    }
                }
            }

            if (i >= sp2C)
            {
                sp18 = 1;
            }
            else
            {
                i -= sp34;
            }

            if (sp3C == 0)
            {
                i = sp28;
                sp18   = 1;
            }

            if (sp40 >= sp38)
            {
                i = sp28;
                sp18   = 1;
            }

            chara->attack_frame = i;
            chara->shot_count  = sp40;
            chara->shot_budget  = sp3C;
        }
        else
        {
            if (var_a0 <= 0)
            {
                var_a0 = 1;
            }

            if (temp_s1 == 1)
            {
                sp58 = 0;
                if (i < sp28)
                {
                    sp5C = (sp28 - i) / (var_a0 * 4);
                }
                else
                {
                    sp5C = 0;
                }
            }
            else
            {
                sp58 = 1;
                sp5C = 4096;
            }

            if (sp10 == 2)
            {
                attack_begin(chara);
                sp10 = 3;
            }

            temp_s0_10 = chara->angle_12;
            temp_s1_5  = chara->angle_10;

            sp60 = chara->previous[2].vx;
            sp64 = chara->previous[2].vy;
            sp68 = chara->previous[2].vz;

            sp6C = chara->attack_origin.vx;
            sp70 = chara->attack_origin.vy;
            sp74 = chara->attack_origin.vz;

            sp7C = sh_fixed_cos(temp_s0_10);
            sp80 = sh_fixed_sin(temp_s0_10);

            sp78 = ATTACK_MULT(sp80, sh_fixed_sin(temp_s1_5));
            sp80 = ATTACK_MULT(sp80, sh_fixed_cos(temp_s1_5));

            sp88 = sh_fixed_cos(sp24);
            sp8C = sh_fixed_sin(sp24);

            sp84 = ATTACK_MULT(sp8C, sh_fixed_sin(sp20));
            sp8C = ATTACK_MULT(sp8C, sh_fixed_cos(sp20));
            sp90 = 0;
            sp94 = 0;

            for (var_s2 = 4, var_fp = 0, var_s3 = 0, i = 0, var_s6 = 0;
                 var_s2 > 0;
                 sp90  += sp68,
                 sp94   += sp64,
                 var_fp += sp60,
                 var_s3 += sp80,
                 i += sp7C,
                 var_s6 += sp78,
                 var_s2--)
            {
                var_a0_3 = (sp84 * var_s2) + var_s6;
                if (var_a0_3 < 0)
                {
                    var_a0_3 += 3;
                }
                temp5 = var_a0_3 >> 2;

                var_a1 = (sp88 * var_s2) + i;
                if (var_a1 < 0)
                {
                    var_a1 += 3;
                }
                temp4 = var_a1 >> 2;

                var_s0_2 = (sp8C * var_s2) + var_s3;
                if (var_s0_2 < 0)
                {
                    var_s0_2 += 3;
                }
                temp_s0_11 = var_s0_2 >> 2;

                temp_s0 = sh_fixed_atan2(temp5, temp_s0_11);

                D_800C4748[var_s2 - 1].vy = sh_fixed_atan2(attack_magnitude(temp5, temp_s0_11), temp4);
                D_800C4748[var_s2 - 1].vx = temp_s0;

                temp_a1 = var_s2 - 1;
                temp_a0 = &D_800C4758[temp_a1];

                var_v1 = (sp6C * var_s2) + var_fp;
                if (var_v1 < 0)
                {
                    var_v1 += 3;
                }
                temp_a0->vx = var_v1 >> 2;

                temp_v0_10 = (sp70 * var_s2) + sp94;
                if (temp_v0_10 < 0)
                {
                    temp_v0_10 += 3;
                }
                temp_a0->vy = temp_v0_10 >> 2;

                temp_v0_10 = (sp74 * var_s2) + sp90;
                if (temp_v0_10 < 0)
                {
                    temp_v0_10 += 3;
                }
                temp_a0->vz = temp_v0_10 >> 2;
            }

            chara->angle_10 = chara->angle_c;
            chara->angle_12 = chara->angle_e;

            for (var_s2 = 0, temp_s1_3 = 4; (temp_s1_3 > 0) && (var_s2 == 0); temp_s1_3--)
            {
                temp_v0_11                 = &D_800C4758[4 - temp_s1_3];
                chara->attack_origin.vx = temp_v0_11->vx;
                chara->attack_origin.vy = temp_v0_11->vy;

                temp_s0_14                 = &D_800C4748[4 - temp_s1_3];
                chara->attack_origin.vz = temp_v0_11->vz;

                attack_vector(chara, temp_s0_14->vx, temp_s0_14->vy);

                if (ctx->melee(ctx->user, chara, temp_s0_14->vx, sp1C, sp5C) != 0)
                {
                    var_s2 = sp58;
                    sp18  |= var_s2;
                }
            }

            i     = sp28;
            temp_v0_12 = chara;

            temp_v0_12->attack_origin.vx       = sp6C;
            chara->previous[2].vx = sp6C;
            temp_v0_12->attack_origin.vy       = sp70;
            chara->previous[2].vy = sp70;
            temp_v0_12->attack_origin.vz       = sp74;
            chara->previous[2].vz = sp74;
        }

        if (sp18 != 0)
        {
            chara->attack_state = -1;

            if (chara->chara_id == 1)
            {
                switch (sp14)
                {
                    case 32:
                    case 33:
                        if (sp30 != -1)
                        {
                            if (ctx->dark)
                            {
                                var_s0_2 = 3;
                            }
                            else
                            {
                                var_s0_2 = sp30;
                            }
                        }
                        else
                        {
                            var_s0_2 = 5;
                        }
                        break;

                    case 34:
                        var_s0_2 = 5;
                        break;

                    case 35:
                        var_s0_2 = 4;
                        break;

                    case 2:
                    case 5:
                    case 6:
                    case 12:
                    case 15:
                    case 16:
                    case 22:
                    case 25:
                    case 26:
                        var_s0_2 = 7;
                        break;

                    case 8:
                    case 9:
                        var_s0_2 = -1;
                        break;

                    default:
                        var_s0_2 = 6;
                        break;
                }

                if (var_s0_2 != -1)
                {
                    ctx->feedback(ctx->user, var_s0_2, chara->hit_mask != 0, ctx->player_accuracy);
                }
            }
        }
        else
        {
            chara->attack_state  = sp10;
            chara->attack_frame = i;
        }
    }

    return chara->hit_mask;

}
