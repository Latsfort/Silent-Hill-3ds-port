#include "sh3ds/school_boss_particles.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    ShSchoolBossParticles pool = {0};
    pool.next_slot = 499;
    sh_school_boss_strand(&pool, 2, 196, 10, 20, 30, 40, 50, 60);
    assert(pool.slots[499].field_A == 5 && pool.slots[0].field_A == 6 && pool.next_slot == 1);
    assert(pool.slots[499].field_C.s_0.field_0 == 0 && pool.slots[499].field_C.s_1.field_2 == 196);
    assert(pool.slots[499].field_0.vx_0 == 10 && pool.slots[499].vy_8 == 20 && pool.slots[0].field_4.vz_4 == 60);
    for (unsigned i = 0; i < 500; ++i) { pool.slots[i].field_A = 7; }
    sh_rng_set_seed(1234);
    sh_school_boss_shed(&pool, 100, 200, 300, 500, 600);
    assert(sh_rng_get_seed() == 1234);
    pool.slots[499].field_A = 0; pool.next_slot = 499;
    sh_school_boss_strand(&pool, 1, 128, 10, 20, 30, 40, 50, 60);
    assert(pool.slots[499].field_A == 0 && pool.next_slot == 500);
    assert(sh_rng_get_seed() == 1234);
    sh_school_boss_shed(&pool, 100, 200, 300, 500, 600);
    const ShSchoolBossParticleSlot *p = &pool.slots[499];
    assert(p->field_A == 7 && pool.next_slot == 500 && sh_rng_get_seed() != 1234);
    assert(p->field_0.vx_0 >= (100 - 32) * 16 && p->field_0.vx_0 <= (100 + 31) * 16);
    assert(p->vy_8 >= 200 * 16 && p->vy_8 <= (200 + 63) * 16);
    assert(p->field_C.s_0.field_2 == 500 && p->field_10.s_1.field_0 == 600);
    assert(p->field_10.s_1.field_2 >= 16 && p->field_10.s_1.field_2 <= 47 && p->field_10.s_1.field_3 <= 15);
    puts("PASS: circular allocation/wrap, paired-strand rollback, full-pool RNG gating and original shed ranges/fields");
    return 0;
}
