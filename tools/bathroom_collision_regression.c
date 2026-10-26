#include "sh3ds/coll.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void check_door(const char *path, int closed_flags, int open_flags,
    int min_x, int max_x, int min_z, int max_z, float floor_x, float floor_z)
{
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    const size_t size = (size_t)ftell(file);
    rewind(file);
    uint8_t *data = malloc(size);
    assert(data && fread(data, 1, size, file) == size);
    fclose(file);
    ShCollWorld world;
    sh_coll_world_reset(&world);
    assert(sh_coll_world_add(&world, data, size));
    unsigned released = 0;
    float start_x = 0, start_z = 0;
    for (int ix = min_x; ix <= max_x; ix++) {
        for (int iz = min_z; iz <= max_z; iz++) {
            const float x = ix * 0.25f, z = -iz * 0.25f;
            sh_coll_trigger_flags_set(&world, closed_flags);
            const float closed = sh_coll_ray(&world, x, z, x - 1.0f, z, 0, 0.3f);
            sh_coll_trigger_flags_set(&world, open_flags);
            const float opened = sh_coll_ray(&world, x, z, x - 1.0f, z, 0, 0.3f);
            assert(opened >= closed);
            if (opened > closed + 0.01f) {
                if (!released) { start_x = x; start_z = z; }
                released++;
            }
        }
    }
    printf("%s: %u sampled paths released by original collision flags\n", path, released);
    assert(released > 0);
    float end_x[2];
    for (int state = 0; state < 2; state++) {
        float x = start_x + 0.5f, y = 0, z = start_z;
        sh_coll_trigger_flags_set(&world, state ? open_flags : closed_flags);
        for (int frame = 0; frame < 15; frame++) {
            sh_coll_move(&world, &x, &y, &z, -0.1f, 0, SH_COLL_PLAYER_RADIUS, 1.6f);
        }
        end_x[state] = x;
    }
    printf("Door crossing from %.2f, %.2f: closed x=%.4f, opened x=%.4f\n", start_x, start_z, end_x[0], end_x[1]);
    fflush(stdout);
    assert(end_x[1] < end_x[0] - 0.1f);
    ShCollHit closed, opened;
    sh_coll_trigger_flags_set(&world, closed_flags);
    assert(sh_coll_ground(&world, floor_x, floor_z, &closed));
    sh_coll_trigger_flags_set(&world, open_flags);
    assert(sh_coll_ground(&world, floor_x, floor_z, &opened));
    assert(closed.height == opened.height && closed.ground_type == opened.ground_type);
    sh_coll_trigger_flags_set(&world, -1);
    assert(!world.trigger_filter);
    free(data);
}

int main(void)
{
    check_door("romfs/game/bg/SU0001.IPD", 3, 1, 64, 84, 232, 248, 20, -60);
    check_door("romfs/game/bg/SU01FE.IPD", 7, 3, 220, 244, -240, -240, 60, 60);
    puts("PASS: original bathroom and boss door collision releases, actual movement and retained map floors");
    return 0;
}
