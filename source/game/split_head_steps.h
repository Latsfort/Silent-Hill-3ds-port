/* Generated from SplitHead_DamageTake and every original control function; ROM state verified. */
void sh_split_head_control_reset(ShSplitHead *splitHead) { splitHead->wait_limit = 26214; splitHead->idle_timer = 0; }
void sh_split_head_begin(ShSplitHead *splitHead)
{


    splitHead->health           = 25500;
    splitHead->props.flags     = 0;
    splitHead->model.alpha = 0;
    splitHead->move_speed        = 0;
    splitHead->heading_angle     = splitHead->rotation.vy;
    splitHead->collision_state  = 4;
    memset(&splitHead->props, 0, sizeof(splitHead->props));

    splitHead->model.control_state = 8;
    splitHead->model.animation_status = 21;
    splitHead->model.time = 162 * 4096;
    splitHead->model.keyframe = 162;


    splitHead->movement_offset[0] = 0;
    splitHead->movement_offset[1] = 0;

    (splitHead->damage_amount = 0);

    splitHead->props.field_ea = 196;
    splitHead->flags    |= 256 | 4;

    sh_split_head_control_reset(splitHead);
    memset(splitHead->contact_points, 0, sizeof(splitHead->contact_points));
    memset(splitHead->edge_type, 0, sizeof(splitHead->edge_type));
    memset(splitHead->edge_side, 0, sizeof(splitHead->edge_side));
    splitHead->first_edge = 0; splitHead->attack_overlap = 0;
    splitHead->player_offset[0] = splitHead->player_offset[1] = 0;
}
static bool split_stride_end(const ShSplitHead *splitHead)
{

    int32_t animTime;

    animTime = splitHead->props.anim_time;
    return ((animTime <  0 || animTime > 811008) && splitHead->model.time < 716800) ||
           ((animTime >= 0 && animTime < 716800) && splitHead->model.time > 811008);

}
void sh_split_head_damage(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    uint8_t  isPlayerDead;
    int32_t damageAmount;
    int32_t newHealth;





    split_player_damage(ctx, &isPlayerDead, splitHead->health == 0);


    if (splitHead->damage_amount == 0 || splitHead->health == 0)
    {
        return;
    }

    damageAmount = SPLIT_FP_FROM(splitHead->damage_amount, 12);

    if (ctx->difficulty == 1)
    {
        damageAmount >>= 1;
    }

    if (damageAmount == 0)
    {
        damageAmount = 1;
    }

    if (splitHead->props.flags & 1)
    {
        if (ctx->difficulty != 1 &&
            ctx->weapon_attack == 34)
        {
            damageAmount <<= 5;
        }
        else
        {
            damageAmount <<= 4;
        }
    }

    newHealth = splitHead->health - damageAmount;
    if (isPlayerDead)
    {
        if (newHealth <= 0)
        {
            newHealth = 1;
        }
    }
    else if (newHealth < 0)
    {
        newHealth = 0;
    }
    splitHead->health = newHealth;

    if (splitHead->health < 24000 && !(splitHead->props.flags & 16))
    {
        splitHead->model.control_state = 4;
        splitHead->props.flags   |= 16;
    }
    else if (splitHead->health == 0)
    {
        if (SPLIT_ANIM_INDEX(splitHead->model.animation_status) == 2)
        {
            splitHead->model.animation_status = 16;
        }
        else if (splitHead->move_speed > 0)
        {
            splitHead->props.flags |= 32;
        }
        else if (splitHead->move_speed < 0)
        {
            splitHead->props.flags &= ~32;
        }

        splitHead->model.control_state = 6;
    }
    else if (splitHead->props.flags & 1)
    {
        splitHead->props.flags |= 64 | 128;
    }

    (splitHead->damage_amount = 0);

    split_player_damage(ctx, &isPlayerDead, splitHead->health == 0);

}
static void split_control_1(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    ShPathPoint unkPos;
    int16_t  angleDeltaToPlayer;
    int32_t distToPlayer;

    distToPlayer       = split_distance(ctx->player.vx - splitHead->position.vx,
                                                   ctx->player.vz - splitHead->position.vz);
    angleDeltaToPlayer = split_angle_signed(split_angle_between(splitHead->position, ctx->player) -
                                                   splitHead->rotation.vy);

    if (splitHead->model.animation_status == 23)
    {
        if (distToPlayer > 14336)
        {
            split_speed(splitHead, ctx, 9830, 9830);
        }
        else
        {
            if (abs(angleDeltaToPlayer) > 1024 || distToPlayer > 20480)
            {
                splitHead->model.control_state = 5;
            }
            else
            {
                splitHead->props.flags |= 8;
                split_speed(splitHead, ctx, 19660, 0);

                if (split_stride_end(splitHead) != false)
                {
                    splitHead->model.animation_status = 4;
                    split_sound(splitHead, ctx, 1475, 0, 128, 0, 0, 0);
                }
            }
        }

        if (SPLIT_TIMESTEP_ANGLE(1, 4) < abs(angleDeltaToPlayer))
        {
            if (angleDeltaToPlayer > 0)
            {
                splitHead->rotation.vy += sh_q12_mul(ctx->delta, 512);
            }
            else
            {
                splitHead->rotation.vy -= sh_q12_mul(ctx->delta, 512);
            }
        }
        else
        {
            splitHead->rotation.vy = split_angle_between(splitHead->position, ctx->player);
        }
    }
    else if (SPLIT_ANIM_INDEX(splitHead->model.animation_status) == 4)
    {
        split_speed(splitHead, ctx, 19660, 0);
    }
    else if (SPLIT_ANIM_INDEX(splitHead->model.animation_status) == 10)
    {
        split_speed(splitHead, ctx, 19660, 0);

        if (splitHead->props.flags & 64)
        {
            splitHead->props.flags &= ~64;

            if (!(sh_rng_rand16() & 0x3))
            {
                splitHead->model.control_state  = 7;
                splitHead->model.animation_status = 18;
            }
            else
            {
                splitHead->model.animation_status = 8;
            }
        }
        else if (splitHead->model.animation_status == 21 &&
                 !split_random(0, 31))
        {
            splitHead->model.animation_status = 22;
            splitHead->model.control_state  = 5;
        }
    }
    else if (splitHead->props.flags & 2)
    {
        if (splitHead->model.animation_status == 5)
        {
            splitHead->model.animation_status = 28;
        }

        split_speed(splitHead, ctx, 4915, 0);

        if (SPLIT_TIMESTEP_ANGLE(3, 4) < abs(angleDeltaToPlayer))
        {
            if (angleDeltaToPlayer > 0)
            {
                splitHead->rotation.vy += sh_q12_mul(ctx->delta, 170);
            }
            else
            {
                splitHead->rotation.vy -= sh_q12_mul(ctx->delta, 170);
            }
        }
        else
        {
            splitHead->rotation.vy = split_angle_between(splitHead->position, ctx->player);
        }

        if (SPLIT_FP_FROM(splitHead->model.time, 12) == 35)
        {
            unkPos.vx = SPLIT_RANDOM_ADD(ctx->player.vx, -1024, 0 - 1);
            unkPos.vy = (ctx->player.vy - split_random(0, 2048 - 1)) - 4096;
            unkPos.vz = SPLIT_RANDOM_ADD(ctx->player.vz, -1024, 0 - 1);
            ctx->player_effect(ctx->user, &unkPos, 2, 7);
        }
    }
    else
    {
        if (distToPlayer < 11468)
        {
            split_speed(splitHead, ctx, 4915, 0);
        }
        else
        {
            split_speed(splitHead, ctx, 4915, 2457);
        }

        if (SPLIT_TIMESTEP_ANGLE(3, 4) < abs(angleDeltaToPlayer))
        {
            if (angleDeltaToPlayer > 0)
            {
                splitHead->rotation.vy += sh_q12_mul(ctx->delta, 170);
            }
            else
            {
                splitHead->rotation.vy -= sh_q12_mul(ctx->delta, 170);
            }
        }
        else
        {
            splitHead->rotation.vy = split_angle_between(splitHead->position, ctx->player);
        }
    }

}
static void split_control_2(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    ShSplitRay trace;
    ShPathPoint    dir;
    int16_t      angleToPlayer2;
    int16_t      angleDeltaToPlayer;
    int16_t      angle1;
    int16_t      angle;
    int16_t      angleToPlayer;
    int32_t     distToPlayer;
    int32_t     distMax;
    int32_t     activeDistMax;
    int32_t        i;
    int32_t        angleMult;

    distToPlayer = split_distance(ctx->player.vx - splitHead->position.vx,
                                             ctx->player.vz - splitHead->position.vz);

    if (splitHead->props.timer_f2 > 2457)
    {
        angle = 0;
        angleToPlayer = split_angle_between(splitHead->position, ctx->player);

        if (splitHead->props.flags & 4)
        {
            angleMult             = 1;
            splitHead->props.flags &= ~4;
        }
        else
        {
            angleMult             = -1;
            splitHead->props.flags |= 4;
        }

        for (i = 0; i < 16; i++)
        {
            if (i != 0 && i != 15)
            {
                angleToPlayer2 = split_random(-64, 64 - 1);
                if (i & 0x1)
                {
                    angle1 = angleToPlayer2 * angleMult;
                }
                else
                {
                    angle1 = angleToPlayer2 * -angleMult;
                }
            }
            else
            {
                angle1 = 0;
            }

            if (angle < 1024)
            {
                distMax = 7372;
            }
            else
            {
                distMax = SPLIT_FP_FROM((distToPlayer * sh_fixed_cos(angle)) + (sh_fixed_sin(angle) * 7372), 12);
            }

            if (distToPlayer < distMax)
            {
                activeDistMax = distMax;
            }
            else
            {
                activeDistMax = distToPlayer;
            }
            distMax = activeDistMax;

            dir.vx = SPLIT_MULT(distMax, sh_fixed_sin((angleToPlayer + angle) + angle1));
            dir.vy = 0;
            dir.vz = SPLIT_MULT(distMax, sh_fixed_cos((angleToPlayer + angle) + angle1));
            (trace = ctx->ray(ctx->user, &splitHead->position, &dir));

            if (!trace.has_hit || trace.character_hit)
            {
                splitHead->props.angle_f0 = angle1 + (angleToPlayer + angle);
                break;
            }

            if (!(i & 1))
            {
                angle += 256;
            }
        }

        if (i != 16)
        {
            splitHead->props.timer_f2 = 0;
        }
    }

    splitHead->props.timer_f2 += ctx->delta;

    angleToPlayer = split_angle_signed((splitHead->props.angle_f0 - splitHead->rotation.vy));
    if (SPLIT_TIMESTEP_ANGLE(3, 3) < abs(angleToPlayer))
    {
        if (angleToPlayer > 0)
        {
            splitHead->rotation.vy += sh_q12_mul(ctx->delta, 341);
        }
        else
        {
            splitHead->rotation.vy -= sh_q12_mul(ctx->delta, 341);
        }
    }
    else
    {
        splitHead->rotation.vy = splitHead->props.angle_f0;
    }

    angleToPlayer      = abs(split_angle_signed(splitHead->props.angle_f0 - splitHead->rotation.vy));
    angleDeltaToPlayer = abs(split_angle_signed(split_angle_between(splitHead->position, ctx->player) -
                                                       splitHead->rotation.vy));

    if (abs(angleToPlayer - angleDeltaToPlayer) < 170)
    {
        if (distToPlayer <= 23552 && angleToPlayer < 682)
        {
            split_speed(splitHead, ctx, 3276, -4915);
        }
        else if ((distToPlayer <= 23552 && angleToPlayer > 682) ||
                 (distToPlayer >  23552 && angleToPlayer > 1024))
        {
            split_speed(splitHead, ctx, 3276, 0);
        }
        else
        {
            split_speed(splitHead, ctx, 3276, 4915);
        }
    }
    else
    {
        if (angleToPlayer < 341)
        {
            split_speed(splitHead, ctx, 3276, 4915);
        }
        else
        {
            split_speed(splitHead, ctx, 3276, 0);
        }
    }

    angleToPlayer2 = split_angle_between(splitHead->position, ctx->player);
    dir.vx = SPLIT_MULT(distToPlayer, sh_fixed_sin(angleToPlayer2));
    dir.vy = 0;
    dir.vz = SPLIT_MULT(distToPlayer, sh_fixed_cos(angleToPlayer2));

    (trace = ctx->ray(ctx->user, &splitHead->position, &dir));

    if (distToPlayer < 26214 &&
        (angleDeltaToPlayer < 85 || (distToPlayer > 13107 && angleDeltaToPlayer < 170)) &&
        (!trace.has_hit || trace.character_hit))
    {
        splitHead->model.control_state = 3;
    }

    if (!split_random(0, 127))
    {
        splitHead->props.flags |= 256;
    }

    if (!split_random(0, 63))
    {
        splitHead->props.flags |= 128;
    }

}
static void split_control_3(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    int16_t  angleDeltaToPlayer;
    int32_t distToPlayer;

    distToPlayer       = split_distance(ctx->player.vx - splitHead->position.vx,
                                                   ctx->player.vz - splitHead->position.vz);
    angleDeltaToPlayer = split_angle_signed(split_angle_between(splitHead->position, ctx->player) -
                                                   splitHead->rotation.vy);

    if (splitHead->model.animation_status == 23)
    {
        if (distToPlayer > 16384)
        {
            split_speed(splitHead, ctx, 16384, 9830);
        }
        else
        {
            if (abs(angleDeltaToPlayer) > 1024 ||
                distToPlayer > 32768 ||
                (distToPlayer < 13107 && abs(angleDeltaToPlayer) > 256))
            {
                splitHead->model.control_state = 2;
            }
            else
            {
                splitHead->props.flags |= 8;
                split_speed(splitHead, ctx, 65536, 0);

                if (split_stride_end(splitHead))
                {
                    splitHead->model.animation_status = 6;
                    split_sound(splitHead, ctx, 1471, 0, 128, 0, 0, 0);
                }
            }
        }

        if (SPLIT_TIMESTEP_ANGLE(3, 3) < abs(angleDeltaToPlayer))
        {
            if (angleDeltaToPlayer > 0)
            {
                splitHead->rotation.vy += sh_q12_mul(ctx->delta, 341);
            }
            else
            {
                splitHead->rotation.vy -= sh_q12_mul(ctx->delta, 341);
            }
        }
        else
        {
            splitHead->rotation.vy = split_angle_between(splitHead->position, ctx->player);
        }
    }
    else if (SPLIT_ANIM_INDEX(splitHead->model.animation_status) == 10)
    {
        split_speed(splitHead, ctx, 6553 - 1, 0);

        splitHead->idle_timer += ctx->delta;

        if (splitHead->model.animation_status == 21 &&
            (!split_random(0, 63) || splitHead->idle_timer > 16384))
        {
            splitHead->idle_timer      = 0;
            splitHead->model.animation_status  = 22;
            splitHead->model.control_state = 2;
        }
    }
    else
    {
        if (distToPlayer < 26214)
        {
            split_speed(splitHead, ctx, 16384, 0);
        }
        else
        {
            split_speed(splitHead, ctx, 16384, 9830);
        }

        if (SPLIT_TIMESTEP_ANGLE(3, 4) < abs(angleDeltaToPlayer))
        {
            if (angleDeltaToPlayer > 0)
            {
                splitHead->rotation.vy += sh_q12_mul(ctx->delta, 170);
            }
            else
            {
                splitHead->rotation.vy -= sh_q12_mul(ctx->delta, 170);
            }
        }
        else
        {
            splitHead->rotation.vy = split_angle_between(splitHead->position, ctx->player);
        }
    }

}
static void split_control_4(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    int32_t vol;
    int32_t timer0;
    int16_t timer1;
    int16_t timer1_div6;

    split_speed(splitHead, ctx, 6552, 0);

    switch (splitHead->model.animation_status)
    {
        case 22:
        case 23:
            splitHead->props.flags |= 8;

            if (!split_stride_end(splitHead))
            {
                break;
            }
            /* fall through */

        case 21:
            splitHead->model.animation_status = 2;
            split_sound(splitHead, ctx, 1477, 2, 0, 0, 0, 0);
            break;

        case 2:
        case 3:
            timer0      = splitHead->props.timer_f4;
            timer1      = splitHead->wait_limit;
            timer1_div6 = timer1 / 6;

            if (timer0 < timer1_div6)
            {
                vol = (timer0 << 7) / timer1_div6;
            }
            else if (((timer1 * 5) / 6) < timer0)
            {
                vol = ((splitHead->wait_limit - timer0) << 7) / timer1_div6;
            }
            else
            {
                vol = 128;
            }

            split_sound(splitHead, ctx, 1477, 1, vol, 65536, 0, 0);

            if (splitHead->props.timer_f4 > splitHead->wait_limit)
            {
                splitHead->wait_limit     = 15564;
                splitHead->model.control_state = 5;
                splitHead->props.timer_f4       = 0;
                splitHead->model.animation_status  = 22;

                split_sound(splitHead, ctx, 1477, 3, 0, 0, 0, 0);
                break;
            }

            splitHead->props.timer_f4 += ctx->delta;
            break;
    }

}
static void split_control_5(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    ShSplitRay trace;
    ShPathPoint    dir;
    int16_t      angle1;
    int16_t      angle2;
    int16_t      angle3;
    int16_t      angle;
    int16_t      angleToPlayer;
    int32_t     distToPlayer;
    int32_t     distMax;
    int32_t        i;
    int32_t        angleSign;
    int32_t     activeDistMax;

    distToPlayer = split_distance(ctx->player.vx - splitHead->position.vx,
                                             ctx->player.vz - splitHead->position.vz);

    if (splitHead->props.timer_f2 > 1638)
    {
        angle         = 0;
        angleToPlayer = split_angle_between(splitHead->position, ctx->player);

        if (splitHead->props.flags & 4)
        {
            angleSign             = 1;
            splitHead->props.flags &= ~4;
        }
        else
        {
            angleSign             = -1;
            splitHead->props.flags |= 4;
        }

        for (i = 0; i < 16; i++)
        {
            if (i != 0 && i != 15)
            {
                angle1 = split_random(-64, 64 - 1);
                if (i & 0x1)
                {
                    angle3 = angle1 * angleSign;
                }
                else
                {
                    angle3 = angle1 * -angleSign;
                }
            }
            else
            {
                angle3 = 0;
            }

            if (angle < 1024)
            {
                distMax = 8192;
            }
            else
            {
                distMax = SPLIT_FP_FROM((distToPlayer * sh_fixed_cos(angle)) + (sh_fixed_sin(angle) << 13), 12);
            }

            if (distToPlayer < distMax)
            {
                activeDistMax = distMax;
            }
            else
            {
                activeDistMax = distToPlayer;
            }
            distMax = activeDistMax;

            dir.vx = SPLIT_MULT(distMax, sh_fixed_sin((angleToPlayer + angle) + angle3));
            dir.vy = 0;
            dir.vz = SPLIT_MULT(distMax, sh_fixed_cos((angleToPlayer + angle) + angle3));
            (trace = ctx->ray(ctx->user, &splitHead->position, &dir));

            if (!trace.has_hit || trace.character_hit)
            {
                splitHead->props.angle_f0 = angle3 + (angleToPlayer + angle);
                break;
            }

            if (!(i & 0x1))
            {
                angle += 256;
            }
        }

        if (i != 16)
        {
            splitHead->props.timer_f2 = 0;
        }
    }

    splitHead->props.timer_f2 += ctx->delta;

    angleToPlayer = split_angle_signed((splitHead->props.angle_f0 - splitHead->rotation.vy));

    if (SPLIT_TIMESTEP_ANGLE(1, 4) < abs(angleToPlayer))
    {
        if (angleToPlayer > 0)
        {
            splitHead->rotation.vy += sh_q12_mul(ctx->delta, 512);
        }
        else
        {
            splitHead->rotation.vy -= sh_q12_mul(ctx->delta, 512);
        }
    }
    else
    {
        splitHead->rotation.vy = splitHead->props.angle_f0;
    }

    angleToPlayer = abs(split_angle_signed(splitHead->props.angle_f0 - splitHead->rotation.vy));
    angle2        = abs(split_angle_signed(split_angle_between(splitHead->position, ctx->player) - splitHead->rotation.vy));

    if (abs(angleToPlayer - angle2) < 170)
    {
        if (distToPlayer <= 19660 && angle2 > 42 && angleToPlayer < 682)
        {
            split_speed(splitHead, ctx, 9830, -9830);
        }
        else if ((distToPlayer <= 19660 && angleToPlayer > 682) ||
                 (distToPlayer >  19660 && angleToPlayer > 1024))
        {
            split_speed(splitHead, ctx, 9830, 0);
        }
        else
        {
            split_speed(splitHead, ctx, 9830, 9830);
        }
    }
    else
    {
        if (angleToPlayer < 341)
        {
            split_speed(splitHead, ctx, 9830, 9830);
        }
        else
        {
            split_speed(splitHead, ctx, 9830, 0);
        }
    }

    angle1 = split_angle_between(splitHead->position, ctx->player);

    dir.vx = SPLIT_MULT(distToPlayer, sh_fixed_sin(angle1));
    dir.vy = 0;
    dir.vz = SPLIT_MULT(distToPlayer, sh_fixed_cos(angle1));

    (trace = ctx->ray(ctx->user, &splitHead->position, &dir));

    if (distToPlayer > 11673 && distToPlayer < 16384 &&
        angle2 < 170 &&
        (!trace.has_hit || trace.character_hit))
    {
        splitHead->model.control_state = 1;
        if (!split_random(0, 7))
        {
            splitHead->props.flags |= 256;
        }
    }

    if (!split_random(0, 127))
    {
        splitHead->props.flags |= 256;
    }

    if (!split_random(0, 63))
    {
        splitHead->props.flags |= 128;
    }

}
static void split_control_6(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    switch (splitHead->model.animation_status)
    {
        case 23:
            split_speed(splitHead, ctx, 2048, 0);

            if (!split_stride_end(splitHead))
            {
                break;
            }
            /* fall through */

        case 2:
        case 3:
        case 20:
        case 21:
        case 22:
            splitHead->model.animation_status = 14;
            split_speed(splitHead, ctx, 4096, 0);
            break;

        default:
            break;

        case 14:
        case 15:
        case 16:
        case 17:
        case 24:
        case 25:
        case 26:
        case 27:
            split_speed(splitHead, ctx, 4096, 0);

            if (!sh_save_event_get(ctx->save, 131))
            {
                split_sound(splitHead, ctx, 1479, 0, 128, 0, 0, 0);
            }

            sh_save_event_set(ctx->save, 131);
            break;
    }

}
static void split_control_7(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    int16_t angleDeltaToPlayer;

    if (SPLIT_ANIM_INDEX(splitHead->model.animation_status) == 9)
    {
        split_speed(splitHead, ctx, 3276, -4915);

        angleDeltaToPlayer = split_angle_signed(split_angle_between(splitHead->position, ctx->player) -
                                                       splitHead->rotation.vy);
        if (SPLIT_TIMESTEP_ANGLE(3, 3) < abs(angleDeltaToPlayer))
        {
            if (angleDeltaToPlayer > 0)
            {
                splitHead->rotation.vy += sh_q12_mul(ctx->delta, 341);
            }
            else
            {
                splitHead->rotation.vy -= sh_q12_mul(ctx->delta, 341);
            }
        }
        else
        {
            splitHead->rotation.vy = split_angle_between(splitHead->position, ctx->player);
        }
    }
    else
    {
        splitHead->model.control_state = 4;
    }

}
static void split_control_8(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    if (!sh_save_event_get(ctx->save, 130))
    {
        return;
    }

    splitHead->model.control_state = 2;
    splitHead->model.animation_status  = 22;
    splitHead->props.flags         |= 256;

}
bool sh_split_head_control(ShSplitHead *splitHead, ShSplitContext *ctx)
{
    if (!ctx->ray || !ctx->player_effect) { return false; }
    ctx->sound_count = 0;



    splitHead->props.flags &= ~8;
    switch (splitHead->model.control_state) {
case 1: split_control_1(splitHead, ctx); break;
case 2: split_control_2(splitHead, ctx); break;
case 3: split_control_3(splitHead, ctx); break;
case 4: split_control_4(splitHead, ctx); break;
case 5: split_control_5(splitHead, ctx); break;
case 6: split_control_6(splitHead, ctx); break;
case 7: split_control_7(splitHead, ctx); break;
case 8: split_control_8(splitHead, ctx); break;
default: return false;
}

    if (splitHead->model.animation_status == 23)
    {
        splitHead->props.anim_time = splitHead->model.time;
    }
    else if (splitHead->model.animation_status == 22)
    {
        splitHead->props.anim_time = 823296;
    }

    return true;
}

static const struct { int kind; ShAnimClipTiming clip; int32_t rate; } s_split_clips[] = {
    {1, {-1, 0, 0}, 0},
    {-1, {0, 0, 0}, 0},
    {1, {-1, 0, 3}, 8192},
    {0, {0, 13, 255}, 14336},
    {1, {-1, 14, 5}, 32768},
    {2, {14, 40, 20}, 0},
    {1, {-1, 41, 7}, 73728},
    {2, {41, 50, 20}, 49152},
    {1, {-1, 51, 9}, 49152},
    {2, {51, 60, 20}, 49152},
    {1, {-1, 61, 11}, 49152},
    {2, {61, 63, 12}, 49152},
    {1, {-1, 78, 13}, 49152},
    {2, {78, 80, 20}, 49152},
    {1, {-1, 94, 15}, 16384},
    {2, {94, 114, 24}, 8192},
    {1, {-1, 115, 17}, 2048},
    {2, {115, 131, 26}, 6144},
    {1, {-1, 132, 19}, 16384},
    {2, {132, 161, 2}, 0},
    {1, {-1, 162, 21}, 32768},
    {0, {162, 171, 255}, 24576},
    {1, {-1, 172, 23}, 16384},
    {0, {172, 201, 255}, 0},
    {1, {-1, 114, 25}, 32768},
    {0, {114, 115, 255}, 0},
    {1, {-1, 131, 27}, 32768},
    {0, {131, 132, 255}, 0},
    {1, {-1, 34, 29}, 8192},
    {0, {34, 36, 255}, 8192},
};
bool sh_split_head_timing(ShSplitHead *splitHead, ShSplitContext *ctx, ShAnimPoseTiming *pose)
{
    const int status = splitHead->model.animation_status;
    if (status < 0 || status >= (int)(sizeof(s_split_clips) / sizeof(s_split_clips[0])) || s_split_clips[status].kind < 0) { return false; }
    splitHead->rotation.vy = split_angle_signed(splitHead->rotation.vy);
switch (splitHead->model.animation_status)
    {
        case 2:
        case 3:
        case 4:
        case 5:
        case 8:
        case 14:
        case 16:
        case 17:
        case 26:
        case 27:
        case 28:
        case 29:
            (splitHead->mesh_variant = 2);
            break;

        default:
            (splitHead->mesh_variant = 1);
            break;
    }
    int32_t duration = s_split_clips[status].rate;
switch (splitHead->model.animation_status)
    {
        case 19:
        case 23:
            switch (splitHead->model.control_state)
            {
                case 2:
                case 7:
                    splitHead->movement_duration = sh_q12_mul(splitHead->move_speed, 38092);

                    if (splitHead->rotation.vy != splitHead->props.angle_ec)
                    {
                        if (splitHead->move_speed >= 0)
                        {
                            splitHead->movement_duration = SPLIT_MAX(splitHead->movement_duration, 32768);
                        }
                        else
                        {
                            splitHead->movement_duration = SPLIT_MIN(splitHead->movement_duration, -32768);
                        }
                    }
                    break;

                case 1:
                case 3:
                    if (!(splitHead->props.flags & 8))
                    {
                        splitHead->movement_duration = SPLIT_MAX(sh_q12_mul(splitHead->move_speed, 38092), 49152);

                        if (splitHead->move_speed > 0)
                        {
                            splitHead->props.flags |= 32;
                        }
                        else if (splitHead->move_speed < 0)
                        {
                            splitHead->props.flags &= ~32;
                        }
                    }
                    else
                    {
                        if (splitHead->props.flags & 32)
                        {
                            splitHead->movement_duration = 98304;
                        }
                        else
                        {
                            splitHead->movement_duration = -98304;
                        }
                    }
                    break;

                case 4:
                    if (!(splitHead->props.flags & 8))
                    {
                        splitHead->movement_duration = SPLIT_MAX(sh_q12_mul(splitHead->move_speed, 38092), 49152);
                    }
                    else
                    {
                        if (SPLIT_FP_FROM(splitHead->model.time, 12) > 187 &&
                            SPLIT_FP_FROM(splitHead->model.time, 12) < 203)
                        {
                            splitHead->movement_duration = 98304;
                        }
                        else
                        {
                            splitHead->movement_duration = -98304;
                        }
                    }
                    break;

                case 5:
                    splitHead->movement_duration = sh_q12_mul(splitHead->move_speed, 38092);

                    if (splitHead->rotation.vy != splitHead->props.angle_ec)
                    {
                        if (splitHead->move_speed >= 0)
                        {
                            splitHead->movement_duration = SPLIT_MAX(splitHead->movement_duration, 40960);
                        }
                        else
                        {
                            splitHead->movement_duration = SPLIT_MIN(splitHead->movement_duration, -40960);
                        }
                    }
                    break;

                case 6:
                    splitHead->movement_duration = sh_q12_mul(splitHead->move_speed, 38092);

                    if (splitHead->props.flags & 32)
                    {
                        splitHead->movement_duration = SPLIT_MAX(splitHead->movement_duration, 49152);
                    }
                    else
                    {
                        splitHead->movement_duration = SPLIT_MIN(splitHead->movement_duration, -49152);
                    }
                    break;
            }

            duration = splitHead->movement_duration;

        case 7:
            break;

        case 5:
            if (ctx->difficulty == 0)
            {
                duration = 49152;
            }
            else if (ctx->difficulty == -1)
            {
                duration = 32768;
            }
            else
            {
                duration = 65536;
            }
            break;
    }
    ShAnimTiming timing = {status, splitHead->model.keyframe, splitHead->model.time, splitHead->model.alpha};
    const int32_t timestep = sh_q12_mul(duration, ctx->delta);
    if (s_split_clips[status].kind == 1) { sh_anim_blend_timing(&timing, &s_split_clips[status].clip, timestep, pose); }
    else if (s_split_clips[status].kind == 2) { sh_anim_once_timing(&timing, &s_split_clips[status].clip, timestep, pose); }
    else { sh_anim_loop_timing(&timing, &s_split_clips[status].clip, timestep, pose); }
    splitHead->model.animation_status = timing.status; splitHead->model.keyframe = timing.keyframe;
    splitHead->model.time = timing.time; splitHead->model.alpha = timing.alpha;
if (splitHead->model.animation_status == 23 &&
        (splitHead->rotation.vy != splitHead->props.angle_ec))
    {
        if ((splitHead->props.angle_ec < splitHead->rotation.vy && splitHead->move_speed >= 0) ||
            (splitHead->rotation.vy < splitHead->props.angle_ec && splitHead->move_speed <  0))
        {
            splitHead->props.field_ee = SPLIT_MAX(splitHead->props.field_ee - sh_q12_mul(ctx->delta, 160), -192);
        }
        else
        {
            splitHead->props.field_ee = SPLIT_MIN(splitHead->props.field_ee + sh_q12_mul(ctx->delta, 160), 192);
        }
    }
    else
    {
        if (splitHead->props.field_ee > 0)
        {
            splitHead->props.field_ee = SPLIT_MAX(splitHead->props.field_ee - sh_q12_mul(ctx->delta, 160), 0);
        }
        else
        {
            splitHead->props.field_ee = SPLIT_MIN(splitHead->props.field_ee + sh_q12_mul(ctx->delta, 160), 0);
        }
    }


    splitHead->props.angle_ec = splitHead->rotation.vy;
    return true;
}

typedef struct { int32_t field_0, field_4, field_8; uint8_t field_C, field_D, field_E; } ShSplitProjection;
static uint32_t split_segment_projection(int16_t *arg0, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{

    int32_t   sp10;
    int16_t angle0;
    int16_t angle1;
    int16_t angle2;
    int32_t   mag0;
    int32_t   mag1;
    int32_t   var0;
    int32_t   var_s1;
    int32_t   var_v0_4;
    int32_t   var1;
    int32_t   temp;

    angle0 = split_angle_signed(sh_fixed_atan2(x0 - x1, y0 - y1));
    angle1 = split_angle_signed(sh_fixed_atan2(x0 - x2, y0 - y2));
    angle2 = split_angle_signed(sh_fixed_atan2(x2 - x1, y2 - y1));
    mag0   = split_distance(x0 - x1, y0 - y1);
    mag1   = split_distance(x0 - x2, y0 - y2);

    if ((mag0 * sh_fixed_sin(angle0 - angle2)) < 0)
    {
        sp10 = -SPLIT_MULT(mag0, sh_fixed_sin(angle0 - angle2));
    }
    else
    {
        sp10 = SPLIT_MULT(mag0, sh_fixed_sin(angle0 - angle2));
    }

    if (mag1 < mag0)
    {
        var0 = mag0;
    }
    else
    {
        var0 = mag1;
    }

    if (!(mag1 < mag0))
    {
        var1 = mag0;
    }
    else
    {
        var1 = mag1;
    }

    var0 = SPLIT_SQUARE(var0 >> 6);
    var1 = SPLIT_SQUARE(var1 >> 6);

    if (var0 < (var1 + (SPLIT_SQUARE((x2 - x1) >> 6) + SPLIT_SQUARE((y2 - y1) >> 6))))
    {
        if (arg0 != NULL)
        {
            if (mag0 * sh_fixed_cos(angle0 - angle2) < 0)
            {
                var_s1 = -SPLIT_MULT(mag0, sh_fixed_cos(angle0 - angle2));
            }
            else
            {
                var_s1 = SPLIT_MULT(mag0, sh_fixed_cos(angle0 - angle2));
            }

            if ((mag1 * sh_fixed_cos(angle1 - angle2)) < 0)
            {
                var_v0_4 = -SPLIT_MULT(mag1, sh_fixed_cos(angle1 - angle2));
            }
            else
            {
                var_v0_4 = SPLIT_MULT(mag1, sh_fixed_cos(angle1 - angle2));
            }

            temp  = var_s1 + var_v0_4;
            *arg0 = ((var_s1 << 12) / temp);
        }

        return abs(sp10);
    }

    if (mag0 < mag1)
    {
        if (arg0 != NULL)
        {
            *arg0 = 0;
        }

        return abs(mag0);
    }

    if (arg0 != NULL)
    {
        *arg0 = 4096;
    }

    return abs(mag1);

}
static void split_edge_projection(ShSplitProjection *arg0, int32_t posX, int32_t posZ, const ShPathPoint *vec0, const ShPathPoint *vec1)
{

    int16_t angle;
    int16_t angle1;

    int32_t   temp_s0_2;
    int32_t   temp_s2;
    int32_t   temp_v0;

    angle       = sh_fixed_atan2(vec1->vx - vec0->vx, vec1->vz - vec0->vz) + 1024;
    temp_s0_2     = posX + sh_fixed_sin(angle);
    temp_v0       = sh_fixed_cos(angle);
    angle1     = (((posX - vec0->vx) >> 2) * ((vec1->vz - vec0->vz) >> 2)) - (((posZ - vec0->vz) >> 2) * ((vec1->vx - vec0->vx) >> 2)) < 1;
    arg0->field_D = angle1;

    if ((((vec0->vx - posX) >> 2) * (temp_v0 >> 2)) - (((vec0->vz - posZ) >> 2) * ((temp_s0_2 - posX) >> 2)) < 0)
    {
        arg0->field_0 = sh_fixed_sqrt0(SPLIT_SQUARE((posX - vec0->vx) >> 3) + SPLIT_SQUARE((posZ - vec0->vz) >> 3)) << 3;
        arg0->field_4 = vec0->vx;
        arg0->field_8 = vec0->vz;
        arg0->field_C = 0;
    }
    else if ((((vec1->vx - posX) >> 2) * (temp_v0 >> 2)) - (((vec1->vz - posZ) >> 2) * ((temp_s0_2 - posX) >> 2)) > 0)
    {
        arg0->field_0 = sh_fixed_sqrt0(SPLIT_SQUARE((posX - vec1->vx) >> 3) + SPLIT_SQUARE((posZ - vec1->vz) >> 3)) << 3;
        arg0->field_4 = vec1->vx;
        arg0->field_8 = vec1->vz;
        arg0->field_C = 1;
    }
    else
    {
        angle       = sh_fixed_atan2(vec1->vx - vec0->vx, vec1->vz - vec0->vz);
        temp_s2       = sh_fixed_sqrt0(SPLIT_SQUARE((posX - vec0->vx) >> 3) + SPLIT_SQUARE((posZ - vec0->vz) >> 3)) << 3;
        angle1     = sh_fixed_atan2(posX - vec0->vx, posZ - vec0->vz) - angle;
        arg0->field_0 = abs(SPLIT_FP_FROM(temp_s2 * sh_fixed_sin(angle1), 12));
        temp_s2       = SPLIT_FP_FROM(temp_s2 * sh_fixed_cos(angle1), 12);
        arg0->field_4 = SPLIT_FP_FROM(temp_s2 * sh_fixed_sin(angle), 12) + vec0->vx;
        arg0->field_8 = SPLIT_FP_FROM(temp_s2 * sh_fixed_cos(angle), 12) + vec0->vz;
        arg0->field_C = 2;
    }

}
/* Retail sp20[4] aliases the immediately following sp50[4] on the PS1 stack. */
static void split_hit_shape(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    ShPathPoint sp20[4];
    ShPathPoint sp50[4];
    int32_t     sp80[4];
    int16_t     sp90[4];
    int16_t     sp98[3];
    int32_t  deltaX;
    int32_t  deltaZ;
    int32_t  dist;
    int32_t     i;
    int32_t     unkIdx;
    int32_t  offsetX;
    int32_t  offsetZ;


    split_bone_point(ctx, &sp20[0], 2, 0, -69, 251);
    split_bone_point(ctx, &sp50[0], 2, 0, 237, 166);
    split_bone_point(ctx, &sp20[1], 19, -36, 5, 113);
    split_bone_point(ctx, &sp20[2], 23, 36, 5, 113);

    deltaX = SPLIT_Q12_TO_Q6(ctx->player.vx - splitHead->position.vx);
    deltaZ = SPLIT_Q12_TO_Q6(ctx->player.vz - splitHead->position.vz);
    dist   = SPLIT_Q6_TO_Q12(sh_fixed_sqrt0(SPLIT_SQUARE(deltaX) + SPLIT_SQUARE(deltaZ)));

    sp98[0] = sh_fixed_atan2(ctx->player.vx - sp20[0].vx,
                     ctx->player.vz - sp20[0].vz);
    sp98[1] = split_angle_signed((sh_fixed_atan2(sp20[1].vx - sp20[0].vx, sp20[1].vz - sp20[0].vz) - sp98[0]));
    sp98[2] = split_angle_signed((sh_fixed_atan2(sp20[2].vx - sp20[0].vx, sp20[2].vz - sp20[0].vz) - sp98[0]));

    if (sp98[1] < 1024 && sp98[2] > -1024)
    {
        if ((sp98[1] >= 0 && sp98[2] <= 0) ||
            (dist > 11264 && sp98[1] >= -0x40 && sp98[2] <= 0x40))
        {
            if (abs(sp98[1] - sp98[2]) >= 114)
            {
                splitHead->props.flags                 |= 1;
                splitHead->hit_shape.top          = sp20[0].vy;
                splitHead->hit_shape.height       = sp50[0].vy;
                splitHead->hit_shape.radius = 2457;

                offsetX = (sp20[0].vx - splitHead->position.vx) * 3;
                if (offsetX < 0)
                {
                    offsetX += 3;
                }
                splitHead->hit_shape.offset.vx = SPLIT_DIV4(offsetX);

                offsetZ = (sp20[0].vz - splitHead->position.vz) * 3;
                if (offsetZ < 0)
                {
                    offsetZ += 3;
                }

                splitHead->hit_shape.offset.vz = SPLIT_DIV4(offsetZ);
                return;
            }
        }
    }

    if (dist > 11264)
    {
        splitHead->props.flags &= ~1;
    }

    split_bone_point(ctx, &sp20[3], 1, 0, -0x46, -0xB7);

    unkIdx = 1;

    sp80[0] = split_segment_projection(&sp90[0], ctx->player.vx,
                            ctx->player.vz, sp20[0].vx, sp20[0].vz,
                            (sp20[1].vx + sp20[2].vx) >> 1, (sp20[1].vz + sp20[2].vz) >> 1);

    for (i = 1; i < 4; i++)
    {
        sp80[i] = split_segment_projection(&sp90[i], ctx->player.vx,
                                ctx->player.vz, sp20[0].vx, sp20[0].vz, sp20[i].vx, sp20[i].vz);

        if (sp80[i] < sp80[unkIdx])
        {
            unkIdx = i;
        }
    }

    if (unkIdx < 3)
    {
        if (sp90[0] > 3481)
        {
            sp90[unkIdx] = 3481;
        }
        else
        {
            sp90[unkIdx] = sp90[0];
        }

        if (unkIdx == 1)
        {
            split_bone_point(ctx, &sp50[1], 19, -0x24, 0x88, 0x16);
        }
        else
        {
            split_bone_point(ctx, &sp50[2], 23, 0x24, 0x88, 0x16);
        }
    }
    else
    {
        if (sp90[3] > 3686)
        {
            sp90[3] = 3686;
        }
        split_bone_point(ctx, &sp50[3], 1, 0, 149, -319);
    }

    splitHead->hit_shape.top = SPLIT_FP_FROM((sp20[0].vy * (4096 - sp90[unkIdx])) + (sp20[unkIdx].vy * sp90[unkIdx]), 12);
    splitHead->hit_shape.height = SPLIT_FP_FROM((sp50[0].vy * (4096 - sp90[unkIdx])) + (sp50[unkIdx].vy * sp90[unkIdx]), 12);
    splitHead->hit_shape.offset_y = (splitHead->hit_shape.top + splitHead->hit_shape.height) >> 1;

    if (unkIdx == 3)
    {
        splitHead->hit_shape.radius = 2457;
    }
    else
    {
        splitHead->hit_shape.radius = SPLIT_FP_FROM(((4096 - sp90[unkIdx]) * 2457) + (sp90[unkIdx] * 1433), 12);
    }

    splitHead->hit_shape.offset.vx = SPLIT_FP_FROM((sp20[0].vx * (4096 - sp90[unkIdx])) + (sp20[unkIdx].vx * sp90[unkIdx]), 12) - splitHead->position.vx;
    splitHead->hit_shape.offset.vz = SPLIT_FP_FROM((sp20[0].vz * (4096 - sp90[unkIdx])) + (sp20[unkIdx].vz * sp90[unkIdx]), 12) - splitHead->position.vz;

}
/* Retail stack sp18+32 is sp38; preserve its typed projection entries. */
static void split_player_offset(ShSplitHead *splitHead, ShSplitContext *ctx, int32_t *offsetX, int32_t *offsetZ)
{

    uint8_t                           sp18[9] = {0};
    uint8_t                           sp28[9] = {0};
    ShSplitProjection  sp38[9];
    int16_t                          spC8[9];
    int32_t                       newPosX;
    int32_t                       newPosZ;
    int32_t                          temp_s7;
    int32_t                          j;
    int32_t                          k;
    int32_t                          var_a0;
    int32_t                          var_a1;
    int32_t                          i;
    int32_t                          new_var;
    ShSplitProjection* ptr0;
    ShSplitProjection* ptr1;

    k = 0;

    if (splitHead->props.flags & 2)
    {
        *offsetZ = 0;
        *offsetX = 0;
        return;
    }

    newPosX = ctx->player.vx + *offsetX;
    newPosZ = ctx->player.vz + *offsetZ;

    for (i = splitHead->first_edge; i < SPLIT_ARRAY_SIZE(sp38); i++)
    {
        split_edge_projection(&sp38[k], newPosX, newPosZ, &splitHead->contact_points[i], &splitHead->contact_points[(i == 8) ? splitHead->first_edge : (i + 1)]);

        sp18[i]         = sp38[k].field_C;
        sp28[i]         = sp38[k].field_D;
        sp38[k].field_E = i;

        if (ctx->player_radius >= sp38[k].field_0)
        {
            k++;
        }
    }

    temp_s7 = k;

    while (k != 0)
    {
        for (k = 0, j = 0; j < 5 && k == 0; j++)
        {
            for (i = 0; i < temp_s7; i++)
            {
                if (j < 3)
                {
                    if ((j == 0 && sp38[i].field_D != 0 && sp38[i].field_C == 2) ||
                        (j == 1 && sp38[i].field_C == j && sp38[(i + 1) % temp_s7].field_C == 0 &&
                         (sp38[i].field_E == 8 ? splitHead->first_edge : sp38[i].field_E + 1) == sp38[(i + 1) % temp_s7].field_E) ||
                        (j == 2 && sp38[i].field_D != 0))
                    {
                        ptr0 = &sp38[i];
                        ptr1 = &sp38[k];

                        ptr1->field_0 = ptr0->field_0;
                        ptr1->field_4 = ptr0->field_4;
                        ptr1->field_8 = ptr0->field_8;
                        ptr1->field_C = ptr0->field_C;
                        ptr1->field_D = ptr0->field_D;
                        ptr1->field_E = ptr0->field_E;

                        sp38[k].field_0 = ctx->player_radius - sp38[k].field_0;
                        spC8[k]         = sh_fixed_atan2(newPosX - sp38[i].field_4, newPosZ - sp38[i].field_8);

                        k++;

                        if (j == 1)
                        {
                            i++;
                        }
                    }
                }
                else
                {
                    if (j == 3)
                    {
                        if (sp38[i].field_E != splitHead->first_edge)
                        {
                            var_a0 = sp38[i].field_E - 1;
                        }
                        else
                        {
                            var_a0 = 8;
                        }

                        if (sp38[i].field_E == 8)
                        {
                            var_a1 = splitHead->first_edge;
                        }
                        else
                        {
                            var_a1 = sp38[i].field_E + 1;
                        }
                    }
                    else
                    {
                        if (splitHead->edge_type[sp38[i].field_E] != 0)
                        {
                            if (sp38[i].field_E == 8)
                            {
                                var_a0 = splitHead->first_edge;
                            }
                            else
                            {
                                var_a0 = sp38[i].field_E + 1;
                            }
                            var_a1 = 0;
                        }
                        else
                        {
                            if (sp38[i].field_E != splitHead->first_edge)
                            {
                                var_a0 = sp38[i].field_E - 1;
                            }
                            else
                            {
                                var_a0 = 8;
                            }
                            var_a1 = 0;
                        }
                    }

                    new_var = var_a0;

                    if ((j == 3 && splitHead->edge_type[sp38[i].field_E] == 2 &&
                         (splitHead->edge_side[sp38[i].field_E] != 0 ||
                          (splitHead->edge_type[new_var] == 1 && splitHead->edge_side[new_var] != 0) ||
                          (splitHead->edge_type[var_a1] == 0 && splitHead->edge_side[var_a1] != 0))) ||
                        (j == 4 && splitHead->edge_type[sp38[i].field_E] != 2 &&
                         (splitHead->edge_side[sp38[i].field_E] != 0 || splitHead->edge_side[new_var] != 0)))
                    {
                        ptr0 = &sp38[i];
                        ptr1 = &sp38[k];

                        ptr1->field_0 = ptr0->field_0;
                        ptr1->field_4 = ptr0->field_4;
                        ptr1->field_8 = ptr0->field_8;
                        ptr1->field_C = ptr0->field_C;
                        ptr1->field_D = ptr0->field_D;
                        ptr1->field_E = ptr0->field_E;

                        sp38[k].field_0 = ctx->player_radius + sp38[k].field_0;
                        spC8[k]         = sh_fixed_atan2(sp38[i].field_4 - newPosX, sp38[i].field_8 - newPosZ);

                        k++;

                        sp28[sp38[i].field_E] = splitHead->edge_side[sp38[i].field_E];
                        sp18[sp38[i].field_E] = splitHead->edge_type[sp38[i].field_E];
                    }
                }
            }
        }

        if (k == 0)
        {
            for (i = 0; i < temp_s7; i++)
            {
                sp38[i].field_0 = ctx->player_radius + sp38[i].field_0;
                spC8[i]         = sh_fixed_atan2(sp38[i].field_4 - newPosX, sp38[i].field_8 - newPosZ);
            }

            k = temp_s7;
        }
        break;
    }

    *offsetZ = 0;
    *offsetX = 0;

    for (i = 0; i < k; i++)
    {
        *offsetX += SPLIT_MULT(sp38[i].field_0, sh_fixed_sin(spC8[i]));
        *offsetZ += SPLIT_MULT(sp38[i].field_0, sh_fixed_cos(spC8[i]));

        if (SPLIT_FP_FROM(splitHead->model.time, 12) > 13 && SPLIT_FP_FROM(splitHead->model.time, 12) < 34 &&
            (sp38[i].field_E < 2 || sp38[i].field_E > 6))
        {
            splitHead->attack_overlap = 1;
        }
    }

    for (i = 0; i < SPLIT_ARRAY_SIZE(sp18); i++)
    {
        splitHead->edge_type[i] = sp18[i];
        splitHead->edge_side[i] = sp28[i];
    }

}
void sh_split_head_offset_prepare(ShSplitHead *splitHead, ShSplitContext *ctx)
{
    splitHead->player_offset[0] = splitHead->player_offset[1] = 0;
    split_player_offset(splitHead, ctx, &splitHead->player_offset[0], &splitHead->player_offset[1]);
}
static const ShPathPoint s_split_contact_offsets[] = {
    {31, 30, 93},
    {-169, 83, 327},
    {-36, 5, 113},
    {69, 65, 69},
    {209, 190, -114},
};
static void split_player_push(ShSplitContext *ctx, int32_t x, int32_t z, const ShPathPoint *velocity)
{
    ctx->player_push = (ShPathPoint){x, 0, z}; ctx->player_push_velocity = *velocity;
}
bool sh_split_head_contact(ShSplitHead *splitHead, ShSplitContext *ctx)
{
    if (!ctx->world_bones || ctx->bone_count <= 23) { return false; }
    split_hit_shape(splitHead, ctx);

    ShSplitProjection sp18;
    ShSplitProjection sp28;
    ShPathPoint                     sp38;
    int32_t                         offsetX;
    int32_t                         offsetZ;
    int32_t                         temp_v0;
    int32_t                      deltaX;
    int32_t                      deltaZ;
    int32_t                         var_v1;
    int16_t                       var_v1_3;


    splitHead->hit_shape.bottom               = 0;
    splitHead->hit_shape.cylinder_radius          = 0;
    splitHead->hit_shape.cylinder_offset.vx = 0;
    splitHead->hit_shape.cylinder_offset.vz = 0;
    splitHead->collision_state                    = 0;
    splitHead->collision_state                    = 4;

    split_bone_point(ctx, &splitHead->contact_points[0], 17, s_split_contact_offsets[0].vx, s_split_contact_offsets[0].vy, s_split_contact_offsets[0].vz);
    split_bone_point(ctx, &splitHead->contact_points[1], 21, (int16_t)((uint16_t)s_split_contact_offsets[0].vx * -1), s_split_contact_offsets[0].vy, s_split_contact_offsets[0].vz);

    splitHead->contact_points[0].vx = (splitHead->contact_points[0].vx + splitHead->contact_points[1].vx) >> 1;
    splitHead->contact_points[0].vy = (splitHead->contact_points[0].vy + splitHead->contact_points[1].vy) >> 1;
    splitHead->contact_points[0].vz = (splitHead->contact_points[0].vz + splitHead->contact_points[1].vz) >> 1;

    split_bone_point(ctx, &splitHead->contact_points[1], 19, s_split_contact_offsets[2].vx, s_split_contact_offsets[2].vy, s_split_contact_offsets[2].vz);
    split_bone_point(ctx, &splitHead->contact_points[2], 19, s_split_contact_offsets[3].vx, s_split_contact_offsets[3].vy, s_split_contact_offsets[3].vz);
    split_bone_point(ctx, &splitHead->contact_points[3], 2, (int16_t)((uint16_t)s_split_contact_offsets[1].vx * -1), s_split_contact_offsets[1].vy, s_split_contact_offsets[1].vz);
    split_bone_point(ctx, &splitHead->contact_points[4], 3, s_split_contact_offsets[4].vx, s_split_contact_offsets[4].vy, s_split_contact_offsets[4].vz);
    split_bone_point(ctx, &splitHead->contact_points[5], 7, (int16_t)((uint16_t)s_split_contact_offsets[4].vx * -1), s_split_contact_offsets[4].vy, s_split_contact_offsets[4].vz);
    split_bone_point(ctx, &splitHead->contact_points[6], 2, s_split_contact_offsets[1].vx, s_split_contact_offsets[1].vy, s_split_contact_offsets[1].vz);
    split_bone_point(ctx, &splitHead->contact_points[7], 23, (int16_t)((uint16_t)s_split_contact_offsets[3].vx * -1), s_split_contact_offsets[3].vy, s_split_contact_offsets[3].vz);
    split_bone_point(ctx, &splitHead->contact_points[8], 23, (int16_t)((uint16_t)s_split_contact_offsets[2].vx * -1), s_split_contact_offsets[2].vy, s_split_contact_offsets[2].vz);

    if (!(splitHead->props.flags & 1))
    {
        splitHead->first_edge       = 2;
        splitHead->contact_points[8].vx = ((splitHead->contact_points[8].vx + splitHead->contact_points[1].vx) >> 1);
        splitHead->contact_points[8].vy = ((splitHead->contact_points[8].vy + splitHead->contact_points[1].vy) >> 1);
        splitHead->contact_points[8].vz = ((splitHead->contact_points[8].vz + splitHead->contact_points[1].vz) >> 1);
    }
    else
    {
        splitHead->first_edge = 0;
    }

    splitHead->attack_overlap = 0;
    offsetX       = splitHead->player_offset[0];
    offsetZ       = splitHead->player_offset[1];
    split_player_offset(splitHead, ctx, &offsetX, &offsetZ);

    if (splitHead->attack_overlap != 0)
    {
        sp38.vx = 0;
        sp38.vy = 0;
        sp38.vz = 0;
    }
    else
    {
        temp_v0 = abs(offsetX) - 32;
        var_v1  = (temp_v0 < 0) ? 0 : (temp_v0 >> 4);
        sp38.vx = (offsetX <= 0) ? (var_v1 * -var_v1) : SPLIT_SQUARE(var_v1);

        sp38.vy = 0;

        temp_v0 = abs(offsetZ) - 32;
        var_v1  = (temp_v0 < 0) ? 0 : (temp_v0 >> 4);
        sp38.vz = (offsetZ <= 0) ? (var_v1 * -var_v1) : SPLIT_SQUARE(var_v1);
    }

    split_player_push(ctx, offsetX + splitHead->player_offset[0], offsetZ + splitHead->player_offset[1], &sp38);

    if (splitHead->props.flags & 1)
    {
        deltaX = SPLIT_Q12_TO_Q6(ctx->player.vx - splitHead->position.vx);
        deltaZ = SPLIT_Q12_TO_Q6(ctx->player.vz - splitHead->position.vz);

        if (SPLIT_Q6_TO_Q12((sh_fixed_sqrt0(SPLIT_SQUARE(deltaX) + SPLIT_SQUARE(deltaZ)))) < 13516)
        {
            split_edge_projection(&sp18, ctx->player.vx,
                                      ctx->player.vz, &splitHead->contact_points[0], &splitHead->contact_points[1]);
            split_edge_projection(&sp28, ctx->player.vx,
                                      ctx->player.vz, &splitHead->contact_points[0], &splitHead->contact_points[8]);

            if (splitHead->model.time < 126976)
            {
                var_v1_3 = 0;
            }
            else if (splitHead->model.time < 135168)
            {
                var_v1_3 = sh_q12_mul(ctx->player_radius,
                                               splitHead->model.time + -126976) >> 1;
            }
            else
            {
                var_v1_3 = 0;
                if (splitHead->model.time < 151552)
                {
                    var_v1_3 = ctx->player_radius;
                }
            }

            if (sp18.field_0 < var_v1_3 &&
                sp28.field_0 < var_v1_3 &&
                !(splitHead->props.flags & 512))
            {
                (ctx->player_attack = 47);

                splitHead->props.flags |= 2 | 512;

                split_sound_at(ctx, &ctx->player, 1473, 255, 2);
            }
        }
    }

    return true;
}

bool sh_split_head_movement(ShSplitHead *splitHead, ShSplitContext *ctx)
{
    if (!ctx->world_bones || ctx->bone_count <= 2 || !ctx->move) { return false; }


    ShPathPoint    pos;

    splitHead->heading_angle = splitHead->rotation.vy;

    split_bone_point(ctx, &pos, 2, 0, -57, 3);

    splitHead->hit_shape.cylinder_offset.vx = pos.vx - splitHead->position.vx;
    splitHead->hit_shape.top                  = pos.vy;
    splitHead->hit_shape.cylinder_offset.vz = pos.vz - splitHead->position.vz;

    if (SPLIT_ANIM_INDEX(splitHead->model.animation_status) == 2 ||
        SPLIT_ANIM_INDEX(splitHead->model.animation_status) == 14)
    {
        splitHead->hit_shape.cylinder_radius = 5324;
    }
    else
    {
        splitHead->hit_shape.cylinder_radius = 6963;
    }

    splitHead->collision_state = 4;

    ctx->move(ctx->user, splitHead, splitHead->movement_offset[0], splitHead->movement_offset[1]);

    splitHead->movement_offset[0] = 0;
    splitHead->movement_offset[1] = 0;

    return true;
}

void sh_split_head_sounds(ShSplitHead *splitHead, ShSplitContext *ctx)
{

    int32_t     keyframeIdx;
    int32_t     sfxVol;
    int32_t     sfxPitch;
    ShPathPoint sfxPos;

    keyframeIdx = SPLIT_FP_FROM(splitHead->model.time, 12);
    sfxVol      = (SPLIT_DIV(splitHead->movement_duration, 98304) >> 7) + 96;
    sfxPitch    = (SPLIT_DIV(splitHead->movement_duration, 98304) >> 6) - 64 + split_random(0, 7);

    if ((keyframeIdx >= 147 && keyframeIdx < 162) ||
        (keyframeIdx >= 187 && keyframeIdx < 201))
    {
        if (splitHead->props.sound_latches[0] == 0)
        {
            sfxPos.vx = splitHead->position.vx + SPLIT_FP_FROM(sh_fixed_sin(splitHead->rotation.vy + 512) * 4915, 12);
            sfxPos.vy = splitHead->position.vy;
            sfxPos.vz = splitHead->position.vz + SPLIT_FP_FROM(sh_fixed_cos(splitHead->rotation.vy + 512) * 4915, 12);

            split_pitch_sound(ctx, &sfxPos, 1472, sfxVol, sfxPitch);
            splitHead->props.sound_latches[0]++;
        }
    }
    else
    {
        splitHead->props.sound_latches[0] = 0;
    }

    if ((keyframeIdx >= 132 && keyframeIdx < 148) ||
        keyframeIdx == 161 ||
        (keyframeIdx >= 172 && keyframeIdx < 185) ||
        (keyframeIdx >= 200 && keyframeIdx < 202))
    {
        if (splitHead->props.sound_latches[1] == 0)
        {
            sfxPos.vx = splitHead->position.vx + SPLIT_FP_FROM(sh_fixed_sin(splitHead->rotation.vy + 1536) * 4915, 12);
            sfxPos.vy = splitHead->position.vy;
            sfxPos.vz = splitHead->position.vz + SPLIT_FP_FROM(sh_fixed_cos(splitHead->rotation.vy + 1536) * 4915, 12);
            splitHead->props.sound_latches[1]++;
        }
    }
    else
    {
        splitHead->props.sound_latches[1] = 0;
    }

    if ((keyframeIdx >= 132 && keyframeIdx < 147) ||
        (keyframeIdx >= 160 && keyframeIdx < 162) ||
        (keyframeIdx >= 172 && keyframeIdx < 187) ||
        (keyframeIdx >= 199 && keyframeIdx < 202))
    {
        if (splitHead->props.sound_latches[2] == 0)
        {
            sfxPos.vx = splitHead->position.vx + SPLIT_FP_FROM(sh_fixed_sin(splitHead->rotation.vy - 512) * 4915, 12);
            sfxPos.vy = splitHead->position.vy;
            sfxPos.vz = splitHead->position.vz + SPLIT_FP_FROM(sh_fixed_cos(splitHead->rotation.vy - 512) * 4915, 12);

            split_pitch_sound(ctx, &sfxPos, 1472, sfxVol, sfxPitch);
            splitHead->props.sound_latches[2]++;
        }
    }
    else
    {
        splitHead->props.sound_latches[2] = 0;
    }

    if ((keyframeIdx >= 146 && keyframeIdx < 161) ||
        (keyframeIdx >= 186 && keyframeIdx < 200))
    {
        if ((uint8_t)splitHead->props.sound_latches[3] == 0)
        {
            sfxPos.vx = splitHead->position.vx + SPLIT_FP_FROM(sh_fixed_sin(splitHead->rotation.vy - 1536) * 4915, 12);
            sfxPos.vy = splitHead->position.vy;
            sfxPos.vz = splitHead->position.vz + SPLIT_FP_FROM(sh_fixed_cos(splitHead->rotation.vy - 1536) * 4915, 12);
            splitHead->props.sound_latches[3]++;
        }
    }
    else
    {
        splitHead->props.sound_latches[3] = 0;
    }

    if (splitHead->props.sound_timer == 0)
    {
        if (splitHead->props.flags & 256)
        {
            split_sound(splitHead, ctx, 1422, 0, 128, 0, 0, 0);
            splitHead->props.sound_timer = split_random(12288, 20480 - 1);
        }
        else if (splitHead->props.flags & 128)
        {
            split_sound(splitHead, ctx, 1474, 0, 128, 0, 0, 0);
            splitHead->props.sound_timer = split_random(6144, 10240 - 1);
        }
    }
    else
    {
        splitHead->props.sound_timer -= ctx->delta;
        if (splitHead->props.sound_timer < 0)
        {
            splitHead->props.sound_timer = 0;
        }
    }

    splitHead->props.flags &= ~(256 | 128);

}

static const struct { ShPathPoint vec; int16_t idx; } s_split_effect_points[] = {
    {{-23, 51, -39}, 19},
    {{-13, 1, -7}, 19},
    {{-25, 83, 0}, 19},
    {{-30, 105, 9}, 19},
    {{-21, 33, 13}, 19},
    {{-27, 70, 21}, 19},
    {{-31, 4, 85}, 19},
    {{-23, 3, 45}, 19},
    {{-25, -8, 46}, 19},
    {{-33, -53, 45}, 19},
    {{-27, -52, 18}, 19},
    {{31, 87, -31}, 23},
    {{28, 66, -45}, 23},
    {{23, 76, -2}, 23},
    {{23, 53, -42}, 23},
    {{30, 51, 49}, 23},
    {{24, 47, 19}, 23},
    {{17, 27, -3}, 23},
    {{13, 2, -6}, 23},
    {{20, 3, 31}, 23},
    {{32, 4, 90}, 23},
    {{32, -48, 45}, 23},
    {{-26, 108, 160}, 18},
    {{-33, 127, 149}, 18},
    {{-37, 108, 78}, 18},
    {{-32, 68, 27}, 18},
    {{-34, 23, 210}, 18},
    {{-36, -16, 146}, 18},
    {{-40, -47, 119}, 18},
    {{31, 115, 141}, 22},
    {{38, 126, 117}, 22},
    {{38, 97, 42}, 22},
    {{24, 48, 58}, 22},
    {{37, 2, 194}, 22},
    {{36, -33, 107}, 22},
    {{42, -68, 83}, 22},
    {{-35, 25, 29}, 17},
    {{35, 25, 29}, 21},
};
static const int16_t s_split_effect_edges[6][3] = {
    {2, 13, 2},
    {10, 21, 1},
    {24, 31, 0},
    {26, 33, 1},
    {36, 37, 0},
    {1, 18, 2},
};
bool sh_split_head_effects(ShSplitHead *splitHead, ShSplitContext *ctx)
{
    if (!ctx->shed || !ctx->strand || !ctx->world_bones || ctx->bone_count <= 23) { return false; }

    ShPathPoint unkPos;
    ShPathPoint  sp30[2];
    int32_t     i;
    int32_t     j;
    uint32_t  moveSpeed;
    int16_t   moveSpeedDiv2;
    int16_t   headingAngle;
    int32_t     animIdx;
    int     new_var;

    if (ctx->delta != 0)
    {
        new_var = 8192;

        animIdx = SPLIT_ANIM_INDEX(splitHead->model.animation_status);

        if ((SPLIT_ANIM_INDEX(splitHead->model.animation_status) != 12) && animIdx != 13)
        {
            if ((SPLIT_FP_FROM(splitHead->model.time, 12) < 20 || SPLIT_FP_FROM(splitHead->model.time, 12) > 35) &&
                ((SPLIT_FP_FROM(splitHead->model.time, 12) > 14 && SPLIT_FP_FROM(splitHead->model.time, 12) < 20) ||
                 ((animIdx == 1 || animIdx == 9) && !split_random(0, 3)) || !split_random(0, 31)))
            {
                if (splitHead->model.animation_status == 7)
                {
                    i = split_random(0, 37);
                }
                else
                {
                    i = split_random(0, 21);
                }

                split_bone_point(ctx, &unkPos, s_split_effect_points[i].idx, s_split_effect_points[i].vec.vx, s_split_effect_points[i].vec.vy, s_split_effect_points[i].vec.vz);

                sp30[0].vx = SPLIT_Q12_TO_Q8(unkPos.vx);
                sp30[0].vy = SPLIT_Q12_TO_Q8(unkPos.vy);
                sp30[0].vz = SPLIT_Q12_TO_Q8(unkPos.vz);

                if (SPLIT_TIME_RANGE(splitHead->model.time, 15, 19))
                {
                    moveSpeedDiv2 = new_var;
                    if (s_split_effect_points[i].idx < 20)
                    {
                        headingAngle = splitHead->rotation.vy + 1024;
                    }
                    else
                    {
                        headingAngle = splitHead->rotation.vy - 1024;
                    }
                }
                else
                {
                    moveSpeed = splitHead->move_speed;

                    if (splitHead->move_speed > 0)
                    {
                        headingAngle = splitHead->rotation.vy;
                        moveSpeedDiv2 = moveSpeed >> 1;
                    }
                    else
                    {
                        moveSpeed = -moveSpeed;
                        moveSpeedDiv2  = moveSpeed >> 1;
                        headingAngle  = splitHead->rotation.vy + 2048;
                    }
                }

                ctx->shed(ctx->user, sp30[0].vx, sp30[0].vy, sp30[0].vz, moveSpeedDiv2, SPLIT_FRACT(headingAngle));
            }
        }
    }

    switch (splitHead->model.animation_status)
    {
        case 16:
        case 17:
        case 26:
        case 27:
            splitHead->props.field_ea = SPLIT_MAX(0, splitHead->props.field_ea - ctx->delta / 68);
            /* fall through */

        case 3:
        case 5:
        case 28:
        case 29:
            for (i = 0; i < SPLIT_ARRAY_SIZE(s_split_effect_edges); i++)
            {
                for (j = 0; j < 2; j++)
                {
                    split_bone_point(ctx, &unkPos, s_split_effect_points[s_split_effect_edges[i][j]].idx, s_split_effect_points[s_split_effect_edges[i][j]].vec.vx,
                                              s_split_effect_points[s_split_effect_edges[i][j]].vec.vy, s_split_effect_points[s_split_effect_edges[i][j]].vec.vz);

                    sp30[j].vx = SPLIT_Q12_TO_Q8(unkPos.vx);
                    sp30[j].vy = SPLIT_Q12_TO_Q8(unkPos.vy);
                    sp30[j].vz = SPLIT_Q12_TO_Q8(unkPos.vz);
                }

                ctx->strand(ctx->user, s_split_effect_edges[i][2], splitHead->props.field_ea,
                                          sp30[0].vx, sp30[0].vy, sp30[0].vz, sp30[1].vx, sp30[1].vy, sp30[1].vz);
            }
            break;
    }

    return true;
}

bool sh_split_head_position_step(ShSplitHead *splitHead, ShSplitContext *ctx, int16_t offsetX, int16_t offsetZ)
{
    if (!ctx->collide) { return false; }

    ShSplitMoveResult newCollResult;
    ShPathPoint           offset;
    int32_t            headingAngle;
    int32_t            moveSpeed;
    int32_t               moveDist;
    int32_t               scaleRestoreShift;
    int32_t               scaleReduceShift;
    int32_t               temp_v0_2;
    int32_t               temp_v0_4;
    int32_t            sinHeadingAngle;


    headingAngle      = splitHead->heading_angle;
    moveSpeed         = sh_q12_mul(ctx->delta, splitHead->move_speed);
    scaleRestoreShift = SPLIT_OVERFLOW_GUARD(moveSpeed);
    scaleReduceShift  = scaleRestoreShift >> 1;

    sinHeadingAngle = sh_fixed_sin(headingAngle);
    moveDist        = moveSpeed >> scaleReduceShift;
    temp_v0_2       = sinHeadingAngle >> scaleReduceShift;

    offset.vx       = SPLIT_RESTORE(sh_q12_mul(moveDist, temp_v0_2), scaleRestoreShift);
    temp_v0_4       = sh_fixed_cos(headingAngle) >> scaleReduceShift;
    offset.vz       = SPLIT_RESTORE(sh_q12_mul(moveDist, temp_v0_4), scaleRestoreShift);
    sinHeadingAngle = splitHead->fall_speed;

    offset.vx += offsetX;
    offset.vy  = sh_q12_mul(ctx->delta, sinHeadingAngle);
    offset.vz += offsetZ;

    if (!ctx->collide(ctx->user, splitHead, &offset, &newCollResult)) { return false; }

    splitHead->position.vx += newCollResult.offset.vx;
    splitHead->position.vy += newCollResult.offset.vy;
    splitHead->position.vz += newCollResult.offset.vz;

    if (splitHead->position.vy > newCollResult.ground_height)
    {
        splitHead->position.vy = newCollResult.ground_height;
        splitHead->fall_speed   = 0;
    }


    return true;
}
