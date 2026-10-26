/* Generated from Anim_BoneUpdate; scaled translations and matrix entries, no renormalization. */
static int32_t bone_translation_lerp(int32_t a, int32_t b, int32_t alpha, unsigned scale)
{
    return (a * (1 << scale)) + (((b - a) * alpha) >> (12 - scale));
}
static int32_t bone_rotation_lerp(int32_t a, int32_t b, int32_t alpha)
{
    return (a * 32) + (((b - a) * alpha) >> 7);
}
