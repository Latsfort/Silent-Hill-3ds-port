#include "../source/game/chara.c"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char *names[] = {"DOG", "BIRD", "CAT", "SBL", "HB_BASE"};
    for (size_t file = 0; file < sizeof(names) / sizeof(names[0]); file++) {
        char path[128];
        snprintf(path, sizeof(path), "romfs/game/anim/%s.ANM", names[file]);
        FILE *fp = fopen(path, "rb");
        assert(fp);
        fseek(fp, 0, SEEK_END);
        const long size = ftell(fp);
        rewind(fp);
        uint8_t *data = malloc((size_t)size);
        assert(data && fread(data, 1, (size_t)size, fp) == (size_t)size);
        fclose(fp);
        const int frames = sh_chara_anm_keyframe_count(data, (size_t)size);
        const unsigned frame_size = data[4] | ((unsigned)data[5] << 8);
        const bool is_player = strcmp(names[file], "HB_BASE") == 0;
        for (int frame = 0; frame < frames; frame++) {
            int bones;
            assert(build_skeleton(&bones, data, (size_t)size, frame, NULL, 0, SH_CHARA_WEP_KF_BASE, is_player));
            const uint8_t *bind = data + 0x14 + 6;
            const int translation = (int8_t)bind[2];
            const uint8_t *kf = anm_keyframe_ptr(data, (size_t)size, frame_size, frame);
            for (int axis = 0; axis < 3; axis++) {
                int value = translation < 0 ? (int8_t)bind[3 + axis] : (int8_t)kf[translation * 3 + axis];
                value *= 1 << data[0x12];
                if (translation == 0 && axis == 1) value -= data[0x13];
                if (is_player) value = s_local[11].t[axis];
                if (s_local[1].t[axis] != value) {
                    printf("FAIL: %s frame %d root translation axis %d: got %d, retail ANM %d\n",
                           names[file], frame, axis, s_local[1].t[axis], value);
                    free(data);
                    return 1;
                }
            }
        }
        printf("PASS: %s, %d frames match %s root translation\n", names[file], frames, is_player ? "player" : "NPC");
        free(data);
    }
    return 0;
}
