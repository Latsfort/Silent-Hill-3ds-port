/* Generated original particle style, clipping and screen geometry; projection is supplied by caller. */
static void boss_quad_rise_style(const ShSchoolBossParticleSlot *slot, ShBossParticleQuad *quad)
{

if (slot->field_A == 8)
    {
        quad->uv0 = ((slot->field_B >= 2) << 6) + ((slot->field_B >= 3) ? 0x020EC000 : 0x020E8000);
        quad->uv1 = ((slot->field_B >= 2) ? 0x7F : 0x3F) + (slot->field_B >= 3 ? 0x6DC000 : 0x6D8000);
        quad->uv2 = ((slot->field_B >= 2) << 6) | ((slot->field_B >= 3) ? 0xFF00 : 0xBF00);
        quad->uv3 = ((slot->field_B >= 2) ? 0x7F : 0x3F) | ((slot->field_B >= 3) ? 0xFF00 : 0xBF00);

        
        if ((uint16_t)slot->field_C.s_0.field_0 < 0xE00)
        {
            
            int32_t       var_v1_3  = 0xB4 + ((0xC4 - ((uint16_t)slot->field_C.s_0.field_0 >> 5)) << 8) + -(((uint16_t)slot->field_C.s_0.field_0 >> 7));
            int32_t       var_v0_6  = var_v1_3 + (BOSS_QUAD_MAX(0, 0xC4 - ((uint16_t)slot->field_C.s_0.field_0 >> 4)) << 0x10) + 0x2E000000;
            quad->color = var_v0_6;
        }
        else
        {
            
            int32_t       var_a0    = BOSS_QUAD_MAX(0, 0x418 - ((uint16_t)slot->field_C.s_0.field_0 >> 2));
            int32_t       var_v0_7  = var_a0 + (BOSS_QUAD_MAX(0, 0x3D4 - ((uint16_t)slot->field_C.s_0.field_0 >> 2)) << 8) + 0x2E000000;
            quad->color = var_v0_7;
        }
    }
    else
    {
        (quad->uv0 = (0) + ((120) << 8) + ((590) << 16));
        (quad->uv1 = (7) + ((120) << 8) + ((77) << 16));
        (quad->uv2 = (0) + ((127) << 8));
        (quad->uv3 = (7) + ((127) << 8));
        boss_quad_color(quad, 120, 128, 128, 32 | 2 | 4 | 8);
    }

    
}
static bool boss_quad_rise_geometry(const ShSchoolBossParticleSlot *slot, ShBossParticleQuad *quad, int16_t screen_x, int16_t screen_y, int32_t depth, int32_t projection_scale, uint8_t base_size)
{
int32_t var_s1;int32_t halfSizeCeil, halfSizeFloor;
uint32_t screen_word = BOSS_QUAD_XY(screen_x, screen_y);

if (depth <= 0 || (depth >> 3) >= 2048 ||
        abs(screen_x) > 200 ||
        abs(screen_y) > 160)
    {
        return false;
    }

    if (slot->field_A != 8)
    {
        var_s1 = (projection_scale * 4) / depth;
    }
    else
    {
        var_s1 = (base_size * (2 * projection_scale)) / depth;
    }

    var_s1 = BOSS_QUAD_MAX(var_s1, 2);

    
    
    
    halfSizeCeil  = (var_s1 + 1) >> 1;
    halfSizeFloor = -(var_s1 >> 1);

    quad->xy[0] = screen_word + halfSizeCeil + ((uint32_t)halfSizeCeil << 16);
    quad->xy[1] = screen_word + halfSizeFloor + ((uint32_t)halfSizeCeil << 16);
    quad->xy[2] = screen_word + halfSizeCeil + ((uint32_t)halfSizeFloor << 16);
    quad->xy[3] = screen_word + halfSizeFloor + ((uint32_t)halfSizeFloor << 16);

    quad->depth_bucket = (unsigned)depth >> 3;
    

    return true;

}

static void boss_quad_fall_style(const ShSchoolBossParticleSlot *slot, ShBossParticleQuad *quad)
{
int32_t var_s1;
if (slot->field_A == 9)
    {
        quad->uv0 = 0x024EC000;
        quad->uv1 = 0x4DC03F;
        quad->uv2 = 0xFF00;
        quad->uv3 = 0xFF3F;

        quad->scale = (slot->field_C.s_2.field_0 >> 6) + 0x24;

        var_s1 = BOSS_QUAD_MAX(0x40 - (slot->field_C.s_2.field_0 >> 6), 0);

        boss_quad_color(quad, var_s1, var_s1, var_s1, 32 | 2 | 4 | 8);
    }
    else
    {
        quad->uv0 = 0x020E7800;
        quad->uv1 = 0xD7807;
        quad->uv2 = 0x7F00;
        quad->uv3 = 0x7F07;

        quad->scale = 2;

        boss_quad_color(quad, 0xFF, BOSS_QUAD_MAX(0xC0 - (slot->field_C.s_2.field_0 >> 6), 0), 0x80, 32 | 2 | 4 | 8);
    }

    
}
static bool boss_quad_fall_geometry(const ShSchoolBossParticleSlot *slot, ShBossParticleQuad *quad, int16_t screen_x, int16_t screen_y, int32_t depth, int32_t projection_scale, uint8_t base_size)
{
int32_t var_s1;
(void)base_size;
(void)slot;
if (depth <= 0 || (depth >> 3) >= 2048)
    {
        return false;
    }

    if (abs(screen_x) > 200)
    {
        return false;
    }

    if (abs(screen_y) > 160)
    {
        return false;
    }

    var_s1 = (quad->scale * projection_scale) / depth;
    if (var_s1 != 0)
    {
        (quad->xy[0] = BOSS_QUAD_XY((uint16_t)screen_x - var_s1, screen_y - var_s1));
        (quad->xy[1] = BOSS_QUAD_XY((uint16_t)screen_x + var_s1, screen_y - var_s1));
        (quad->xy[2] = BOSS_QUAD_XY((uint16_t)screen_x - var_s1, screen_y + var_s1));
        (quad->xy[3] = BOSS_QUAD_XY((uint16_t)screen_x + var_s1, screen_y + var_s1));
    }
    else
    {
        (quad->xy[0] = BOSS_QUAD_XY(screen_x, screen_y));
        (quad->xy[1] = BOSS_QUAD_XY((uint16_t)screen_x + 1, screen_y));
        (quad->xy[2] = BOSS_QUAD_XY(screen_x, screen_y + 1));
        (quad->xy[3] = BOSS_QUAD_XY((uint16_t)screen_x + 1, screen_y + 1));
    }

    quad->depth_bucket = (unsigned)depth >> 3;
    

    return true;

}
