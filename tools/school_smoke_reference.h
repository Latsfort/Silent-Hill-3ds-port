/* Generated from retail smoke functions; projection is excluded from the physics oracle. */
static uint32_t ref_rand32(void) {

    u32 nextSeed = ref_seed;

    nextSeed  = (nextSeed * 1664525) + 1013904223;
    ref_seed = nextSeed;

    return nextSeed;
}
static int32_t ref_rand16(void) {

    return ref_rand32() >> 17;
}
static void ref_begin(void) {

    s32 count;
    s32 i;

    count = ref_config.count;
    for (i = 0; i < ARRAY_SIZE(ref_particles); i++)
    {
        if (ref_particles[i].kind != 0)
        {
            continue;
        }

        ref_particles[i].kind         = 13;
        ref_particles[i].age = ((TO_FIXED(ref_config.ceiling - ref_config.base, Q12_SHIFT) / ref_config.speed) * count) / ref_config.count;

        count--;
        if (count == 0)
        {
            break;
        }
    }

    ref_config.phase = 0;
    ref_flags                        |= 1 << 1;
}
static void ref_spawn(s32 idx) {

    
    q19_12 angleZ;
    s16    rngX;

    if (ref_config.disable_spawn == 0)
    {
        ref_particles[idx].kind = 14;
    }

    if (ref_config.shape == 0)
    {
        rngX                                = (ref_rand16() % ref_config.spread);
        angleZ                              = Rng_GenerateUInt(0, 4095);
        ref_particles[idx].x = (s32)(rngX * sh_fixed_cos(angleZ)) >> Q12_SHIFT;
        ref_particles[idx].z = (s32)(rngX * sh_fixed_sin(angleZ)) >> Q12_SHIFT;
    }
    else
    {
        ref_particles[idx].x = (ref_rand16() % (ref_config.spread * 2)) - ref_config.spread;
        ref_particles[idx].z = (ref_rand16() % (ref_config.spread * 2)) - ref_config.spread;
    }

    ref_particles[idx].y            = ref_config.base;
    ref_particles[idx].variant         = ref_rand16() % 3;
    ref_particles[idx].age = 0;
}
static bool ref_delay(s32 idx) {

    ref_particles[idx].age += Q12_MULT_PRECISE(ref_delta, ((ref_rand16() % Q12_ANGLE(144.0f)) + Q12_ANGLE(288.0f)));

    if (Q12_DIV(ref_config.ceiling - ref_config.base, ref_config.speed) < ref_particles[idx].age)
    {
        ref_spawn(idx);
        return true;
    }

    return false;
}
static bool ref_update(s32 idx) {
s32 temp_v1_2, temp_v0_2, temp_v0_3, temp_v0_4, temp_s1, temp_v1_3, temp_v0_5, var_a1;
temp_v1_2 = 0x40 - (ref_particles[idx].age >> 6);
    if (temp_v1_2 >= 0)
    {
        var_a1 = Q12_MULT_PRECISE(temp_v1_2, (0x400 - MIN(ref_config.phase * 2, 0x400)) * 4);
    }
    else
    {
        var_a1 = 0;
    }

    ref_particles[idx].shade = var_a1;

    temp_v0_2 = ref_particles[idx].x >> 6;
    temp_v0_3 = ref_particles[idx].z >> 6;

    ref_particles[idx].y += Q12_MULT_PRECISE(ref_delta, ref_config.speed);

    temp_v0_4 = sh_fixed_sqrt0(SQUARE(temp_v0_2) + SQUARE(temp_v0_3)) << 10;
    temp_s1   = Q12_MULT_PRECISE(temp_v0_4, ref_config.spread);

    temp_v1_3 = ref_remainder(ref_rand16(), temp_s1) - (temp_s1 >> 2);

    temp_s1   = Q12_MULT_PRECISE(ref_delta, temp_v1_3) >> 1;
    temp_v0_5 = sh_fixed_atan2(ref_particles[idx].x, ref_particles[idx].z);

    ref_particles[idx].x    += Q12_MULT(temp_s1, sh_fixed_cos(temp_v0_5));
    ref_particles[idx].z    += Q12_MULT(temp_s1, sh_fixed_sin(temp_v0_5));
    ref_particles[idx].age += MAX(temp_s1, 0);

    

    ref_particles[idx].age += CLAMP_LOW(TO_FIXED(Q12_MULT_PRECISE(ref_delta, ref_config.speed), Q12_SHIFT) /
                                                                (Q12_MULT(sh_fixed_cos(ref_config.phase), ref_config.ceiling) -
                                                                 ref_config.base),
                                                                0);

    if (ref_particles[idx].age > Q12(1.0f) || ref_config.phase == Q12(0.25f))
    {
        if (ref_config.emission_time == 0 && sh_fixed_cos(ref_config.phase) < Rng_GenerateUInt(0, Q12_ANGLE(360.0f) - 1))
        {
            ref_particles[idx].kind = 0;
        }
        else
        {
            ref_spawn(idx);
        }

        return false;
    }
return true;
}
static void ref_global_tick(void) {

    {
        if (ref_config.emission_time > 0)
        {
            ref_config.emission_time -= ref_delta;
            ref_config.emission_time  = MAX(ref_config.emission_time, 0);
        }
        else
        {
            ref_config.phase = Q12_MULT_PRECISE(ref_delta, ref_config.fade_rate) +
                                                    ref_config.phase;
            ref_config.phase = MIN(ref_config.phase, Q12(0.25f));
        }
    }
}
