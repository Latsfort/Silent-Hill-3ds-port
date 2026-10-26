/* Generated original500-slot emitter CPU logic; GPU projection/draw remains separate. */
static const int16_t s_boss_particle_curve[34] = {1014, 993, 973, 952, 932, 911, 891, 870, 849, 827, 806, 784, 762, 740, 717, 694, 670, 646, 622, 596, 570, 543, 515, 486, 455, 422, 387, 349, 307, 258, 197, 87, 0, 0};
static int32_t boss_particle_curve(int32_t angle) { return s_boss_particle_curve[angle >> 5]; }
static void boss_particle_spawn(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config, int32_t idx, bool arg1)
{

    int32_t   rand;
    int16_t randAngle;

    if (config->field_1 == 1 &&
        boss_particle_random(0, 4095) < config->field_10 &&
        boss_particle_random(0, 7) == 0) 
    {
        pool->slots[idx].field_A = 12;
        return;
    }

    if ((config->flags & (1 << 4)) &&
        boss_particle_random(0, 3) == 0) 
    {
        pool->slots[idx].field_A = 12;
        return;
    }

    rand      = boss_particle_random(0, (int16_t)config->field_1C - 1);
    randAngle = boss_particle_random(0, 4096 - 1);

    if ((config->flags & (1 << 2)) &&
        boss_particle_random(0, 63) == 0) 
    {
        pool->slots[idx].field_A = 10;
    }
    else
    {
        pool->slots[idx].field_A = 8;
    }

    if (arg1 == true)
    {
        pool->slots[idx].field_C.s_0.field_2 = boss_particle_random(0, 15);
    }
    else
    {
        pool->slots[idx].field_C.s_0.field_2 = boss_particle_random(0, 4095);
    }

    if (config->flags & (1 << 4))
    {
        pool->slots[idx].field_C.s_0.field_0 = BOSS_PARTICLE_MIN((((config->field_3 << 4) * rand) / (int16_t)config->field_1C) +
                                                                 (uint16_t)pool->slots[idx].field_C.s_0.field_2 +
                                                                 config->field_10 +
                                                                 boss_particle_random(0, 1023),
                                                                 4095);
    }
    else
    {
        pool->slots[idx].field_C.s_0.field_0 = BOSS_PARTICLE_MIN((((config->field_3 << 4) * rand) / (int16_t)config->field_1C) +
                                                                 (uint16_t)pool->slots[idx].field_C.s_0.field_2 +
                                                                 config->field_10,
                                                                 4095);
    }

    pool->slots[idx].field_10.s_2.field_0 = (uint8_t)sh_rng_rand16() | 0x80; 
    pool->slots[idx].field_10.s_2.field_1 = (uint8_t)sh_rng_rand16() | 0x80;

    pool->slots[idx].field_10.s_0.field_2 = boss_particle_random(0, 4095);

    if (config->flags & (1 << 0))
    {
        pool->slots[idx].field_0.vx_0 = BOSS_PARTICLE_FROM(rand * sh_fixed_cos(randAngle), 12);
        pool->slots[idx].field_4.vz_4 = BOSS_PARTICLE_FROM(rand * sh_fixed_sin(randAngle), 12);
    }
    else
    {
        pool->slots[idx].field_0.vx_0 = boss_particle_random(-config->field_18, config->field_18 - 1);
        pool->slots[idx].field_4.vz_4 = boss_particle_random(-config->field_1A, config->field_1A - 1);
    }

    pool->slots[idx].vy_8    = config->field_14 + BOSS_PARTICLE_FROM((uint16_t)pool->slots[idx].field_C.s_0.field_2 * config->field_16, 12);
    pool->slots[idx].field_B = boss_particle_random(0, 1) + (boss_particle_random(0, 2) * 2);

}
static void boss_particle_init(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config)
{

    int32_t i;
    int32_t counterC;

    if (config->flags & (1 << 0))
    {
        config->field_1A = config->field_1C * 2;
        config->field_18 = config->field_1A;
    }
    else
    {
        config->field_1C = (config->field_18 + config->field_1A) >> 1;
    }

    config->field_16 = config->field_12 - config->field_14;

    if (config->field_1)
    {
        config->field_10 = 4096;
    }
    else
    {
        config->field_10 = 0;
    }

    counterC = config->field_C;

    for (i = 500 - 1; i >= 0; i--)
    {
        if (pool->slots[i].field_A == 0)
        {
            if (config->field_1 == 0)
            {
                boss_particle_spawn(pool, config, i, false);
            }
            else
            {
                pool->slots[i].field_A = 12;
            }

            counterC--;
            if (counterC == 0)
            {
                break;
            }
        }
    }

    pool->effect_flags |= 1 << 0;

}
static void boss_particle_setup(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config, int32_t delta, ShBossParticleFrame *ptr)
{
int32_t i;
ptr->field_C4 = sh_q12_mul(delta, config->field_E);
        ptr->field_C8 = config->field_4 * 16;
        ptr->field_CA = config->field_5 * 16;
        ptr->field_CC = (0x100 - config->field_5) * 16;
        ptr->field_CE = (delta * config->field_E) / config->field_16;
        ptr->field_C6 = (uint16_t)config->field_A >> 2;
        ptr->field_D0 = config->field_20;
        ptr->field_D4 = config->field_24;

        switch (config->field_1)
        {
            case 0:
                for (i = 0; i < 500; i++)
                {
                    if (pool->slots[i].field_A == 12)
                    {
                        boss_particle_spawn(pool, config, i, 0);
                    }
                }
                break;

            case 1:
                config->field_10 = BOSS_PARTICLE_MIN(config->field_10 + ((delta * config->field_2) >> 5),
                                                            4096);
                break;

            case 2:
                for (i = 0; i < 500; i++)
                {
                    switch (pool->slots[i].field_A)
                    {
                        case 8:
                        case 9:
                        case 10:
                        case 11:
                            pool->slots[i].field_A = 12;
                            break;
                    }
                }
                break;

            case 3:
                config->field_10 = BOSS_PARTICLE_MAX(config->field_10 - ((delta * config->field_2) >> 5),
                                                                  0);

                for (i = 0; i < 500; i++)
                {
                    if (pool->slots[i].field_A == 12 &&
                        boss_particle_random(0, 4095) > config->field_10 &&
                        boss_particle_random(0, 15) != 0)
                    {
                        boss_particle_spawn(pool, config, i, 1);
                    }
                }
                break;
        }
    
}

static bool boss_particle_rise(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config, int32_t idx, int32_t delta, const ShBossParticleFrame *frame, ShPathPoint *position)
{
int32_t var_s1;
pool->slots[idx].vy_8 = pool->slots[idx].vy_8 + frame->field_C4;

    var_s1 = (config->field_8 * pool->slots[idx].field_10.s_2.field_1) >> 8;

    position->vx = pool->slots[idx].field_0.vx_0 +
                     BOSS_PARTICLE_MULT(sh_fixed_sin(pool->slots[idx].field_10.s_0.field_2), var_s1);

    position->vz = pool->slots[idx].field_4.vz_4 +
                     BOSS_PARTICLE_MULT(sh_fixed_cos(pool->slots[idx].field_10.s_0.field_2), var_s1);

    if ((uint16_t)pool->slots[idx].field_C.s_0.field_2 < (uint16_t)frame->field_C8)
    {
        int32_t angle = boss_particle_curve(1024 - (((uint16_t)pool->slots[idx].field_C.s_0.field_2 << 6) / config->field_4));
        var_s1    = BOSS_PARTICLE_MULT((uint16_t)frame->field_CA, sh_fixed_sin(angle)) + (uint16_t)frame->field_CC;

        position->vx = BOSS_PARTICLE_MULT(position->vx, var_s1);
        position->vz = BOSS_PARTICLE_MULT(position->vz, var_s1);
    }

    position->vx += frame->field_D0;
    position->vz += frame->field_D4;

    if (pool->slots[idx].field_B & (1 << 0))
    {
        pool->slots[idx].field_10.s_0.field_2 += sh_q12_mul(delta, ((uint16_t)config->field_A * pool->slots[idx].field_10.s_2.field_0) >> 8);
    }
    else
    {
        pool->slots[idx].field_10.s_0.field_2 -= sh_q12_mul(delta, ((uint16_t)config->field_A * pool->slots[idx].field_10.s_2.field_0) >> 8);
    }

    pool->slots[idx].field_C.s_0.field_0 += frame->field_CE;
    pool->slots[idx].field_C.s_0.field_2 += frame->field_CE;

    if ((uint16_t)pool->slots[idx].field_C.s_0.field_0 >= 4096)
    {
        if (config->flags & (1 << 1) && pool->slots[idx].field_A == 8)
        {
            pool->slots[idx].field_C.s_0.field_0 = (uint16_t)config->field_10 + boss_particle_random(0, 0xFFF);
            if ((config->flags & (1 << 3)) &&
                boss_particle_random(0, 15) == 0) 
            {
                pool->slots[idx].field_A = 11;
            }
            else
            {
                pool->slots[idx].field_A = 9;
            }
        }
        else
        {
            if (config->flags & (1 << 3) &&
                boss_particle_random(0, 15) == 0) 
            {
                int32_t randVal                                        = boss_particle_random(2048, 4095);
                pool->slots[idx].field_C.s_0.field_0 = randVal + (config->field_10 >> 1);
                pool->slots[idx].field_A             = 11;
            }
            else
            {
                boss_particle_spawn(pool, config, idx, true);
            }

            return 0;
        }
    }


position->vy = pool->slots[idx].vy_8;
return true;
}

static bool boss_particle_fall(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config, int32_t idx, int32_t delta, const ShBossParticleFrame *frame, ShPathPoint *position)
{
int32_t var_s1;
pool->slots[idx].vy_8 = pool->slots[idx].vy_8 + frame->field_C4;

    var_s1 = (config->field_8 * pool->slots[idx].field_10.s_2.field_1) >> 8;

    position->vx = config->field_20 + pool->slots[idx].field_0.vx_0 +
                     BOSS_PARTICLE_MULT(sh_fixed_sin(pool->slots[idx].field_10.s_0.field_2), var_s1);

    position->vz = config->field_24 + pool->slots[idx].field_4.vz_4 +
                     BOSS_PARTICLE_MULT(sh_fixed_cos(pool->slots[idx].field_10.s_0.field_2), var_s1);

    if (pool->slots[idx].field_B & 1)
    {
        pool->slots[idx].field_10.s_0.field_2 += sh_q12_mul(delta, (frame->field_C6 * pool->slots[idx].field_10.s_2.field_0) >> 8);
    }
    else
    {
        pool->slots[idx].field_10.s_0.field_2 -= sh_q12_mul(delta, (frame->field_C6 * pool->slots[idx].field_10.s_2.field_0) >> 8);
    }

    pool->slots[idx].field_C.s_0.field_0 += frame->field_CE;

    if (pool->slots[idx].field_C.s_2.field_0 > 4096)
    {
        boss_particle_spawn(pool, config, idx, 1);
        return false;
    }


position->vy = pool->slots[idx].vy_8;
return true;
}
