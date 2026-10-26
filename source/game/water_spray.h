/* Generated from original roof spray configuration and functions. */
void sh_water_spray_configure_roof(ShWaterSprayConfig *config, bool ball)
{

            
            
            
            
            
            
            
            
            
            

            
            
            
            
            
            
            
            
            
            
            
            

            config[0].vx_0     = -41779;
            config[0].field_8  = 491;
            config[0].field_C  = 327;
            config[0].vz_4     = -54681;
            config[0].idx_14   = 2;
            config[0].field_12 = 40;
            config[0].field_E  = 1024;
            config[0].field_10 = 1228;

            config[1].vx_0     = -41984;
            config[1].vz_4     = -72581;
            config[1].field_C  = 204;
            config[1].field_12 = 40;

            if (ball)
            {
                
                
                
                
                
                
                
                
                
                
                
                
                
                

                config[1].field_8  = 901;
                config[1].idx_14   = 4;
                config[1].field_A  = 0;
                config[1].field_E  = 819;
                config[1].field_10 = 1433;
                config[2].field_C  = 245;
                config[2].vx_0     = -69058;
                config[2].vz_4     = -72704;
                config[2].field_8  = 819;
                config[2].idx_14   = 3;
                config[2].field_12 = 60;
                config[2].field_E  = 819;
                config[2].field_10 = 819;
            }
            else
            {
                
                
                config[1].field_8              = 819;
                config[1].idx_14               = 3;
                config[1].field_E              = 614;
                config[1].field_10             = 819;
            }
}
void sh_water_spray_spawn(ShWaterParticle *particle, const ShWaterSprayConfig *config, int arg1, int arg2)
{

    int16_t tmp0;
    int16_t offsetX; 
    int16_t offsetZ; 
    int32_t zOff2;
    int32_t i;

    switch (config[arg1].idx_14)
    {
        case 0:
            particle->field_0.vx_0 = config[arg1].vx_0;
            particle->field_4.vz_4 = config[arg1].vz_4;
            break;

        case 1:
            
            tmp0 = (sh_rng_rand16() % config[arg1].field_C) - (config[arg1].field_C >> 1);
            particle->field_0.vx_0 = config[arg1].vx_0 + WATER_FROM((tmp0 * sh_fixed_sin(config[arg1].field_A)), 12);
            zOff2 = WATER_FROM((tmp0 * sh_fixed_cos(config[arg1].field_A)), 12);
            particle->field_4.vz_4 = config[arg1].vz_4 + zOff2;
            break;

        case 2:
            for (i = 0; i < 8; i++)
            {
                offsetX = water_random_range(-config[arg1].field_C, config[arg1].field_C - 1);
                offsetZ = water_random_range(-config[arg1].field_C, config[arg1].field_C - 1);
                if ((WATER_SQUARE(offsetX) + WATER_SQUARE(offsetZ)) < (config[arg1].field_C * config[arg1].field_C))
                {
                    break;
                }
            }

            if (i == 8)
            {
                offsetZ = 0;
                offsetX = 0;
            }

            particle->field_0.vx_0 = config[arg1].vx_0 + offsetX;
            particle->field_4.vz_4 = config[arg1].vz_4 + offsetZ;
            break;

        case 3:
            offsetX = water_random_range(0, 4096 - 1);
            particle->field_0.vx_0 = config[arg1].vx_0 + WATER_FROM((config[arg1].field_C * sh_fixed_sin(offsetX)), 12);
            zOff2 = WATER_FROM((config[arg1].field_C * sh_fixed_cos(offsetX)), 12);
            particle->field_4.vz_4 = config[arg1].vz_4 + zOff2;
            break;

        case 4:
            offsetX = water_random_add(config[arg1].field_A, -1024, 1024 - 1);
            particle->field_0.vx_0 = config[arg1].vx_0 + WATER_FROM((config[arg1].field_C * sh_fixed_sin(offsetX)), 12);
            zOff2 = WATER_FROM((config[arg1].field_C * sh_fixed_cos(offsetX)), 12);
            particle->field_4.vz_4 = config[arg1].vz_4 + zOff2;
            break;
    }

    particle->vy_8 = config[arg1].field_8;

    if (arg2 == 0)
    {
        particle->field_C.s_0.field_0 = water_random_range(0, config[arg1].field_10 - 1);
    }
    else
    {
        particle->field_C.s_0.field_0 = particle->field_C.field_0 - config[arg1].field_10;
    }

    particle->field_C.s_1.field_2  = water_random_range(0, 3);
    particle->field_C.s_1.field_3  = water_random_range(96, 159);
    particle->field_10.s_2.field_0 = arg1;
}
void sh_water_spray_begin(ShWaterPool *pool, const ShWaterSprayConfig *config, int arg0)
{

    int32_t idx;
    int32_t i;

    for (i = 0; i < config[arg0].field_12; i++)
    {
        idx = water_slot_alloc(pool, 28);
        if (idx == -1)
        {
            break;
        }

        sh_water_spray_spawn(&pool->particles[idx], config, arg0, 0);
    }
}
bool sh_water_spray_step(ShWaterParticle *particle, const ShWaterSprayConfig *config, int32_t dt)
{
    int32_t idx2;
particle->field_C.s_0.field_0 += dt;

    idx2 = particle->field_10.s_2.field_0;
    if ((uint16_t)particle->field_C.s_0.field_0 > config[idx2].field_10)
    {
        sh_water_spray_spawn(particle, config, idx2, 1);
        return false;
    }

    
    return true;
}
void sh_water_spray_quad(const ShWaterParticle *particle, const ShWaterSprayConfig *config,
                          int screen_x, int screen_y, int depth, int projection,
                          int pitch, int illumination, ShWaterSprayQuad *quad)
{
    const int idx2 = particle->field_10.s_2.field_0;
    int32_t var_lo, var_a2, var_a1, rgbColor;
var_lo = WATER_TO((uint16_t)particle->field_C.s_0.field_0, 12) / config[idx2].field_10;

    

    var_a2 = (WATER_FROM(sh_q12_mul(config[idx2].field_E * particle->field_C.s_1.field_3, sh_fixed_sin(var_lo >> 2)), 12) * projection) / depth;
    var_a1 = (var_a2 * (0x800 - abs(pitch))) >> 10;

    quad->x[0] = (uint16_t)screen_x - var_a2;
    quad->y[0] = screen_y - var_a1;
    quad->x[1] = (uint16_t)screen_x + var_a2;
    quad->y[1] = screen_y - var_a1;
    quad->x[2] = (uint16_t)screen_x - var_a2;
    quad->y[2] = screen_y;
    quad->x[3] = (uint16_t)screen_x + var_a2;
    quad->y[3] = screen_y;

    const uint32_t packed0 = ((particle->field_C.s_1.field_2 << 13) + 0xE0000);
    const uint32_t packed1 = ((particle->field_C.s_1.field_2 << 13) + 0x2D003F);
    const uint16_t packed2 = ((particle->field_C.s_1.field_2 << 13) | 0x1F00);
    const uint16_t packed3 = ((particle->field_C.s_1.field_2 << 13) | 0x1F3F);

    

    if (var_lo < 0x800)
    {
        rgbColor = sh_q12_mul((var_lo * 2), 0x80);
    }
    else
    {
        rgbColor = 0x80 - sh_q12_mul(((var_lo - 0x800) * 2), 0x80);
    }

    rgbColor = (rgbColor * illumination) >> 7;

    quad->shade = rgbColor;
    const uint32_t packed[4] = {packed0, packed1, packed2, packed3};
    for (int i = 0; i < 4; i++) { quad->u[i] = packed[i] & 255; quad->v[i] = (packed[i] >> 8) & 255; }
    quad->clut = packed0 >> 16;
    quad->page = packed1 >> 16;
}
