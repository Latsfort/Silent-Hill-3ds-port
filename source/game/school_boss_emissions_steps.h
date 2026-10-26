/* Generated source allocator8005E7E0 and factories800CBE7C/800CCE2C. */
static int32_t boss_effect_allocate(ShSchoolBossParticles *pool, int32_t arg0)
{

    int32_t idx;
    int32_t i;

    idx = pool->next_slot;

    for (i = 0; i < 500; i++, idx++)
    {
        if (idx >= 500)
        {
            idx = 0;
        }

        if (pool->slots[idx].field_A == 0)
        {
            break;
        }
    }

    if (i == 500)
    {
        return -1;
    }

    pool->slots[idx].field_A = arg0;

    pool->next_slot = idx + 1;
    return idx;

}
void sh_school_boss_shed(ShSchoolBossParticles *pool, int32_t arg0, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4)
{

    int32_t i;

    i = boss_effect_allocate(pool, 7);
    if (i != -1)
    {
        pool->slots[i].field_0.vx_0         = BOSS_EMISSION_ADD(arg0, -32, 31) * 16;
        pool->slots[i].vy_8                 = BOSS_EMISSION_ADD(arg1, 0, 63) * 16;
        pool->slots[i].field_4.vz_4         = BOSS_EMISSION_ADD(arg2, -32, 31) * 16;
        pool->slots[i].field_B              = 0;
        pool->slots[i].field_C.s_0.field_0  = 0;
        pool->slots[i].field_C.s_0.field_2  = arg3;
        pool->slots[i].field_10.s_1.field_0 = arg4;
        pool->slots[i].field_10.s_1.field_2 = boss_emission_random(16, 47);
        pool->slots[i].field_10.s_1.field_3 = boss_emission_random(0, 15);
    }

}
void sh_school_boss_strand(ShSchoolBossParticles *pool, int32_t arg0, int32_t arg1, int32_t posX0, int32_t posY0, int32_t posZ0, int32_t posX1, int32_t posY1, int32_t posZ1)
{

    int32_t idx0;
    int32_t idx1;

    idx0 = boss_effect_allocate(pool, 5);
    if (idx0 == -1)
    {
        return;
    }

    idx1 = boss_effect_allocate(pool, 6);
    if (idx1 == -1)
    {
        pool->slots[idx0].field_A = 0;
        return;
    }

    pool->slots[idx0].field_B             = arg0;
    pool->slots[idx0].field_C.s_0.field_0 = idx1;
    pool->slots[idx0].field_C.s_1.field_2 = arg1;
    pool->slots[idx0].field_0.vx_0        = posX0;
    pool->slots[idx0].vy_8                = posY0;
    pool->slots[idx0].field_4.vz_4        = posZ0;

    pool->slots[idx1].field_0.vx_0 = posX1;
    pool->slots[idx1].vy_8         = posY1;
    pool->slots[idx1].field_4.vz_4 = posZ1;

}
