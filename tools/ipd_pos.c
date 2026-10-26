/* usage: ipd_pos <file.IPD>...  prints the collision chunk origin of each file */
#include "sh3ds/coll.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    static ShCollWorld world;
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f) {
            continue;
        }
        fseek(f, 0, SEEK_END);
        const size_t size = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t *data = malloc(size);
        const size_t got = fread(data, 1, size, f);
        fclose(f);
        sh_coll_world_reset(&world);
        if (got == size && sh_coll_world_add(&world, data, size)) {
            const ShCollChunk *c = &world.chunks[0];
            printf("%s %.0f %.0f n28=%u n2c=%u\n", argv[i], c->pos_x_q8 / 256.0f, c->pos_z_q8 / 256.0f, c->n28, c->n2c);
            if (getenv("CELLZ")) {
                const int iz = atoi(getenv("CELLZ"));
                for (int ix = 0; ix < c->nx; ix++) {
                    const uint8_t *r = c->ranges + (size_t)(iz * c->nx + ix) * 4u;
                    printf("  ix%d: 28=%d 2c=%d\n", ix, (int16_t)(r[0] | r[1] << 8), (int16_t)(r[2] | r[3] << 8));
                }
            }
        }
        free(data);
    }
    return 0;
}
