/* Generated from sharedFunc_800CBDA8_1_s02 movement before projection. */
bool sh_water_particle_step(ShWaterParticle *particle, const ShWaterConfig *config, int32_t dt, int32_t gravity)
{
    const int idx0 = particle->field_C.s_1.field_0 >> 2;
    int32_t temp_s4;
if (config->field_30[idx0] == 0) {
temp_s4 = sh_q12_mul(dt, particle->field_10.s_3.field_2);

        if ((particle->field_10.s_3.field_0 + temp_s4) < config->field_28[idx0])
        {
            particle->field_0.vx_0 += sh_q12_mul(temp_s4, sh_fixed_sin(config->field_34[idx0]));
            particle->field_4.vz_4 += sh_q12_mul(temp_s4, sh_fixed_cos(config->field_34[idx0]));
        }
        else if ((particle->field_10.s_3.field_0 + temp_s4) >= (config->field_28[idx0] + config->field_64[idx0]))
        {
            sh_water_particle_spawn(particle, config, 3, idx0 + 1);
            return false;
        }
        else if (particle->field_10.s_3.field_0 < config->field_28[idx0])
        {
            particle->field_0.vx_0 += sh_q12_mul((config->field_28[idx0] -
                                                                        particle->field_10.s_3.field_0),
                                                                       sh_fixed_sin(config->field_34[idx0]));
            particle->field_4.vz_4 += sh_q12_mul((config->field_28[idx0] -
                                                                        particle->field_10.s_3.field_0),
                                                                       sh_fixed_cos(config->field_34[idx0]));
        }

        particle->field_10.s_0.field_0 += temp_s4;
        
} else {
particle->field_C.s_0.field_2 = water_min(particle->field_C.s_2.field_2 + ((gravity * particle->field_10.s_3.field_2) >> 7),
                                                                 24576);

        temp_s4 = sh_q12_mul(dt, particle->field_C.s_2.field_2);

        if (particle->field_10.s_3.field_0 + temp_s4 >= (config->field_28[idx0] + config->field_64[idx0]))
        {
            sh_water_particle_spawn(particle, config, 3, idx0 + 1);
            return false;
        }

        particle->field_10.s_3.field_0 = particle->field_10.s_3.field_0 + temp_s4;
        particle->vy_8                 = particle->vy_8 + temp_s4;

        
}
    return true;
}
