#include "sh3ds/npc_load.h"
#include "sh3ds/gfx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned uploads, texture_uploads;
static uint32_t mesh_hash;
static bool read_file_into(const char *path, void *bytes, size_t capacity, size_t *size)
{
    char native[256];
    unsigned j = 0;
    for (unsigned i = 0; path[i]; ++i) { if (path[i] != ':') { native[j++] = path[i]; } }
    native[j] = 0;
    FILE *file = fopen(native, "rb");
    if (!file || fseek(file, 0, SEEK_END)) { if (file) { fclose(file); } return false; }
    long length = ftell(file);
    rewind(file);
    bool ok = length > 0 && (size_t)length <= capacity && fread(bytes, 1, length, file) == (size_t)length;
    fclose(file);
    if (ok) { *size = (size_t)length; }
    return ok;
}

bool sh_gfx_actor_set_mesh(int slot, const ShMapVertex *vertices, size_t count)
{
    assert(slot >= 0 && slot < SH_GFX_MAX_ACTORS && count > 0 && count <= SH_CHARA_MAX_VERTS);
    mesh_hash = 2166136261u;
    const unsigned char *bytes = (const unsigned char *)vertices;
    for (size_t i = 0; i < count * sizeof(*vertices); ++i) { mesh_hash = (mesh_hash ^ bytes[i]) * 16777619u; }
    uploads++;
    return true;
}

bool sh_gfx_actor_set_tim(int slot, const ShTim *tim, const unsigned *cluts, int count)
{
    assert(slot >= 0 && slot < SH_GFX_MAX_ACTORS && tim && cluts && count > 0);
    texture_uploads++;
    return true;
}

#include "../build/npc_pose_under_test.h"

int main(void)
{
    assert(sh_npc_load_split_head(0, 100, 1, 0));
    const ShCharaBone *bones;
    unsigned count;
    assert(sh_npc_pose_split_head(0, 100, 1, 0, &bones, &count));
    assert(count > 23 && count <= SH_CHARA_MAX_BONES && uploads == 1 && texture_uploads == 1);
    ShCharaBone saved[SH_CHARA_MAX_BONES];
    memcpy(saved, bones, count * sizeof(*bones));
    uint32_t neutral_hash = mesh_hash;
    assert(load_npc(1, "romfs:/game/chara/DOG.ILM", "romfs:/game/anim/DOG.ANM", "romfs:/game/chara/DOG.TIM"));
    assert(bake_npc_frame(1, 0));
    unsigned previous_uploads = uploads;
    assert(sh_npc_pose_split_head(0, 100, 1, 0, &bones, &count));
    assert(uploads == previous_uploads && !memcmp(saved, bones, count * sizeof(*bones)));
    assert(sh_npc_pose_split_head(0, 100, 1, 192, &bones, &count));
    assert(uploads == previous_uploads + 1 && mesh_hash != neutral_hash);
    assert(memcmp(saved[1].rotation, bones[1].rotation, sizeof(saved[1].rotation)));
    uint32_t twist_hash = mesh_hash;
    assert(sh_npc_pose_split_head(0, 100, 2, 192, &bones, &count));
    assert(uploads == previous_uploads + 2 && mesh_hash != twist_hash);
    ShAnimPoseTiming blend = {100, 101, 2048};
    assert(sh_npc_pose_split_head_blend(0, &blend, 2, 192, &bones, &count));
    unsigned blended_uploads = uploads;
    uint32_t blended_hash = mesh_hash;
    assert(sh_npc_pose_split_head_blend(0, &blend, 2, 192, &bones, &count) && uploads == blended_uploads);
    blend.alpha = 1024;
    assert(sh_npc_pose_split_head_blend(0, &blend, 2, 192, &bones, &count));
    assert(uploads == blended_uploads + 1 && mesh_hash != blended_hash);
    blend.second = 102;
    assert(sh_npc_pose_split_head_blend(0, &blend, 2, 192, &bones, &count) && uploads == blended_uploads + 2);
    blend.alpha = 4097;
    assert(!sh_npc_pose_split_head_blend(0, &blend, 2, 192, &bones, &count));
    assert(texture_uploads == 2);
    assert(!sh_npc_load_split_head(-1, 0, 1, 0));
    assert(!sh_npc_pose_split_head(SH_GFX_MAX_ACTORS, 0, 1, 0, &bones, &count));
    assert(!sh_npc_pose_split_head(1, 0, 1, 0, &bones, &count));
    assert(!sh_npc_load_split_head(0, 0, 3, 0));
    puts("PASS: actual game.c boss asset path, variant/twist-aware cache, isolated bones after another NPC pose, legacy load and upload counts");
    return 0;
}
