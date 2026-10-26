#include "sh3ds/coll.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    FILE *file = fopen("romfs/game/bg/SCFEFE.IPD", "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    const size_t size = (size_t)ftell(file);
    rewind(file);
    uint8_t *data = malloc(size);
    assert(data && fread(data, 1, size, file) == size);
    fclose(file);
    ShCollWorld world;
    sh_coll_world_reset(&world);
    assert(sh_coll_world_add(&world, data, size));
    for (int side = 0; side < 2; side++) {
        float x = -60.0f, z = side ? 56.0f : 64.0f;
        const float start = z;
        sh_coll_nudge(&world, &x, &z, side ? -0.6f : 0.0f, SH_COLL_PLAYER_RADIUS);
        printf("Ladder %d: %.5f, %.5f\n", side, x, z);
        assert(x == -60.0f);
        assert(side ? z < start : z > start);
        assert(fabsf(z - start) <= SH_COLL_PLAYER_RADIUS + 0.03f);
        const float settled = z;
        for (int frame = 0; frame < 90; frame++) {
            sh_coll_nudge(&world, &x, &z, side ? -0.6f : 0.0f, SH_COLL_PLAYER_RADIUS);
        }
        assert(x == -60.0f && z == settled);
    }
    free(data);
    puts("PASS: both original ladder edges settle toward the valid surface without sideways drift");
    return 0;
}
