/* Generated from Player_ReceiveDamage, Math_ShortestAngleGet; do not edit. */
static void player_damage_angle(int16_t angleFrom, int16_t angleTo, int16_t *shortestAngle)
{

    int16_t adjAngle;

    if (angleTo > angleFrom)
    {
        if ((angleTo - angleFrom) < 2048)
        {
            *shortestAngle = angleTo - angleFrom;
        }
        else
        {
            adjAngle       = angleTo  - 4096;
            *shortestAngle = adjAngle - angleFrom;
        }
    }
    else
    {
        if ((angleFrom - angleTo) < 2048)
        {
            *shortestAngle = angleTo - angleFrom;
        }
        else
        {
            adjAngle       = angleFrom - 4096;
            *shortestAngle = angleTo   - adjAngle;
        }
    }

}
static void player_receive(ShPlayerDamage *player, ShPlayerDamageContext *ctx)
{

    int16_t headingAngle;
    uint16_t enemyRotY;
    int32_t   i;
    int32_t   sfxId;
    int32_t   angleState;

    
    sfxId = 1326;
    if (player->actor.attack_received != -1)
    {
        switch (sh_combat_attack_info((unsigned)player->actor.attack_received)->field_11)
        {
            case 2:
                sfxId = 1327;
                break;

            case 4:
                sfxId = 1328;
                break;

            case 5:
                sfxId = 1329;
                break;

            case 0:
                break;
        }
    }

    if (ctx->disable_control || ctx->disable_damage)
    {
        player->actor.damage_amount = 0;
        return;
    }

    switch (player->state)
    {
        case 3:
        case 4:
        case 41:
        case 42:
        case 45:
        case 46:
        case 49:
        case 50:
            break;

        case 8:
        case 9:
            return;

        
        case 10:
        case 13:
        case 14:
        case 15:
        case 32:
        case 33:
        case 39:
        case 40:
        case 47:
        case 48:
            
            if (player->actor.damage_amount != 0 && !(player->flags & 16384))
            {
                player->flags |= 16384;
                ctx->sound(ctx->user, sfxId, &player->position, 32, 0);
                player->field_10c = 64;
            }

            if (player->actor.damage_amount == 0)
            {
                player->flags &= ~16384;
            }

            ctx->grab_feedback(ctx->user);
            break;

        case 7:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 35:
        case 37:
        case 38:
            player->actor.damage_position.vz = 0;
            player->actor.damage_position.vy = 0;
            player->actor.damage_position.vx = 0;

            if (player->actor.attack_received == 47)
            {
                player->collision_radius = 0;
                player_damage_state(player, 9);
                return;
            }

            if (player->actor.attack_received >= 68 &&
                player->actor.attack_received <  70)
            {
                player->actor.damage_amount = 0;
            }
            break;

        default:
            if (ctx->walk_to_run)
            {
                ctx->previous_attack = player->actor.attack_received;
                return;
            }

            if (ctx->previous_attack != -1)
            {
                player->actor.attack_received = ctx->previous_attack;
                ctx->previous_attack = -1;
            }

            if (player->actor.attack_received <= 0)
            {
                break;
            }

            ctx->target_npc    = -1;
            player->weapon_attack = (ctx->save->equipped_weapon == 0) ? -1 :
                                                                                                  (ctx->save->equipped_weapon - 128);

            if (player->weapon_attack == 2)
            {
                ctx->motor(ctx->user, 2, 3);
            }

            if (player->state >= 3 &&
                player->state <  7)
            {
                ctx->collision_reset(ctx->user);
            }

            enemyRotY = ctx->npc_yaws[player->actor.last_attacker];
            if (player->actor.attack_received >= 64 && player->actor.attack_received < 66)
            {
                enemyRotY -= 1024;
            }
            else if (player->actor.attack_received == 69)
            {
                enemyRotY = 1024;
            }
            else if (player->actor.attack_received == 68)
            {
                enemyRotY = player->actor.damage_position.vy;
            }

            enemyRotY = PLAYER_DAMAGE_ANGLE((enemyRotY - player->yaw) + 4096);

            switch (player->actor.attack_received)
            {
                case 67:
                    player_damage_state(player, 7);
                    break;

                case 63:
                    player->move_speed = 6144;
                    player_damage_angle(player->yaw, ctx->npc_yaws[0], &headingAngle);
                    player->heading_angle = headingAngle;

                    if (enemyRotY >= 1024 && enemyRotY < 3072)
                    {
                        player_damage_state(player, 45);
                    }
                    else
                    {
                        player_damage_state(player, 46);
                    }
                    break;

                case 60:
                case 62:
                    player->actor.damage_amount = 40960;
                    player_damage_state(player, 19);
                    break;

                case 41:
                case 42:
                    player_damage_state(player, 19);
                    break;

                case 49: 
                case 54: 
                case 56: 
                case 66:
                    if (enemyRotY >= 1024 &&
                        enemyRotY <  3072)
                    {
                        ctx->npc_indices[0] = player->actor.last_attacker;

                        switch (player->actor.attack_received)
                        {
                            case 54:
                                player_damage_state(player, 37);
                                break;

                            case 45:
                            case 56:
                                player_damage_state(player, 10);
                                break;

                            case 49:
                                player_damage_state(player, 14);
                                break;

                            case 66:
                                player_damage_state(player, 32);
                                break;

                        }
                    }
                    else
                    {
                        ctx->npc_indices[1] = player->actor.last_attacker;

                        switch (player->actor.attack_received)
                        {
                            case 54:
                                player_damage_state(player, 38);
                                break;

                            case 45:
                            case 56:
                                player_damage_state(player, 13);
                                break;

                            case 49:
                                player_damage_state(player, 15);
                                break;

                            case 66:
                                player_damage_state(player, 33);
                                break;
                        }
                    }
                    break;

                case 47:
                    player->actor.health                     = -1;
                    player->collision_radius = 0;
                    player_damage_state(player, 9);
                    return;

                case 69:
                    player->move_speed = 6553;
                    player_damage_angle(player->yaw, 1024, &headingAngle);
                    player->heading_angle = headingAngle;

                    /* fall through */
                case 68:
                    if (player->actor.attack_received != 69)
                    {
                        player->move_speed = 16384;
                        player_damage_angle(player->yaw, (int16_t)player->actor.damage_position.vy, &headingAngle);
                        player->heading_angle = headingAngle;
                    }

                    /* fall through */
                case 40:
                case 43:
                case 44:
                case 45:
                case 46:
                case 48:
                case 50:
                case 52:
                case 57:
                case 58:
                case 59:
                case 61:
                case 64:
                case 65:
                    
                    if (enemyRotY >= 512 && enemyRotY < 1536)
                    {
                        angleState = 26;
                    }
                    
                    else if (enemyRotY >= 1536 && enemyRotY < 2560)
                    {
                        angleState = 24;
                    }
                    
                    else if (enemyRotY >= 2560 && enemyRotY < 3584)
                    {
                        angleState = 25;
                    }
                    
                    else
                    {
                        angleState = 23;
                    }

                    player_damage_state(player, angleState);
                    break;

                case 53:
                    if (enemyRotY < 2047)
                    {
                        angleState = 27;
                    }
                    else
                    {
                        angleState = 28;
                    }

                    player_damage_state(player, angleState);
                    break;
            }

            if ((uint32_t)player->state >= 3)
            {
                player->afk_timer = 0;
                player->field_f4 = 0;
                player->flags                 &= ~4096;
                player->aiming              = false;
                player->flags                 &= ~512;
                player->actor.attack_state           = -1;
            }
            break;
    }

    
    if (player->state == 8)
    {
        player_damage_clear(player);
        return;
    }

    
    if (player->actor.damage_amount != 0)
    {
        player->flags &= ~4;
        if (!(player->flags & 16384))
        {
            ctx->sound(ctx->user, sfxId, &player->position, 32, 0);
            player->flags |= 16384;
            player->field_10c = 0x40;
        }

        if (ctx->save->map_idx == 0)
        {
            player->actor.health -= player->actor.damage_amount * 2;
        }
        else
        {
            switch (ctx->difficulty)
            {
                case -1:
                    player->actor.damage_amount = PLAYER_DAMAGE_DIV(player->actor.damage_amount * 3, 4);
                    break;

                case 1:
                    player->actor.damage_amount = PLAYER_DAMAGE_DIV(player->actor.damage_amount * 6, 4);
                    break;
            }

            player->actor.health -= player->actor.damage_amount;
        }

        if (player->actor.health < 0)
        {
            player->actor.health = -1;
            ctx->dead  = true;
        }

        ctx->damage_feedback(ctx->user, player->actor.damage_amount);
        player->actor.damage_amount = 0;
    }

    if (player->actor.health <= 0 && player->state != 8 &&
        player->state != 36 && player->state != 39 &&
        player->state != 40 && player->state != 47 &&
        player->state != 48 && !ctx->walk_to_run)
    {
        player->actor.last_attacker                     = -1;
        ctx->save->health_saturation = 0;

        for (i = 0; i < 4; i++)
        {
            ctx->npc_indices[i] = -1;
        }

        if (player->actor.attack_received == 66)
        {
            player_damage_state(player, 36);
        }
        else
        {
            player_damage_state(player, 8);
        }
    }

    

}
