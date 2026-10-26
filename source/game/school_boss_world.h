/* Generated from MAP1_S02 boss-door world update. */
static bool boss_room(float x, float z)
{
    return in_chunk(SH_Q12(x), 1, 2, -1, 2) && in_chunk(SH_Q12(-z), 1, -2, -1, -2);
}
static uint32_t boss_prop_mask(float x, float z)
{
    return boss_room(x, z) ? (s_school_bathroom_props[2].mask | s_school_bathroom_props[3].mask | s_school_bathroom_props[4].mask | s_school_bathroom_props[5].mask | s_school_bathroom_props[6].mask) : 0;
}
static int boss_collision_flags(const ShBossDoors *doors)
{
    return 1 | 2 |
        ((doors->rotation[0] != 2048 ||
            doors->rotation[1] != doors->rotation[0]) ? 4 : 0);
}
static void boss_world_update(ShBossDoors *doors, ShSavegame *save, float x, float z)
{
    int collFlags = 0;
if (boss_room(x, z))
    {
        
        
        
        
        

        
        if (doors->rotation[0] != 2048 ||
            doors->rotation[1] != doors->rotation[0])
        {
            collFlags |= 4;
            sh_save_event_clear(save, 362);
        }
        else
        {
            sh_save_event_set(save, 362);
        }

        collFlags |= 2;
    }
    else
    {
        if (sh_save_event_get(save, 362))
        {
            doors->rotation[1] = 2048;
        }
        else
        {
            doors->rotation[1] = 1024;
        }

        doors->rotation[0] = 2048;
    }
    (void)collFlags;
}
