/* Generated from WorldGfx_SplitHeadMeshSwap / Bone_ModelIdxGet. */
static bool split_model_active(unsigned model, unsigned flags)
{
    switch (flags & (SH_CHARA_F_SPLIT_HEAD_1 | SH_CHARA_F_SPLIT_HEAD_2)) {
    case SH_CHARA_F_SPLIT_HEAD_1:
        switch (model) {
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
            return false;
        default: return true;
        }
    case SH_CHARA_F_SPLIT_HEAD_2:
        switch (model) {
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
            return false;
        default: return true;
        }
    default: return true;
    }
}
