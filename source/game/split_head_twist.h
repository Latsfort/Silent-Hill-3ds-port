/* Generated from SplitHead_AnimUpdate procedural bone matrix calls. */
static bool split_twist_bones(BoneXform *local, int count, int16_t angle)
{
    if (!angle) { return true; }
    if (count < 25) { return false; }
    split_twist_y(&local[1], angle);
    for (int i = 18; i < 25; i++) { split_twist_y(&local[i], angle >> 3); }
    return true;
}
