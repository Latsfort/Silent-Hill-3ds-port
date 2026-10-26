/* Generated from MAP1_S02 phone world visibility. */
static uint32_t phone_prop_mask(const ShSavegame *save, float x, float z)
{
    uint32_t mask = 0;
if (in_chunk(SH_Q12(x), 0, 0, -1, 1) && in_chunk(SH_Q12(-z), 1, 4, -1, 4))
    {
        if (!sh_save_event_get(save, 99))
        {
            mask |= s_school_bathroom_props[7].mask;
            mask |= s_school_bathroom_props[8].mask;
        }
    }
    return mask;
}
