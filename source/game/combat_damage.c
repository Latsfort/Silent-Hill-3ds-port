#include "sh3ds/combat_damage.h"
#include "sh3ds/rng.h"
#include <stddef.h>
#include "fixed_gte_data.h"

#define COMBAT_MULT(a, b) ((int32_t)(((int64_t)(a) * (int64_t)(b)) >> 12))

/* Rng_RandQ12, player_control.c:80080514. */
static int32_t combat_random(void)
{
    int32_t value = sh_rng_rand16();
    return (((value * 2) ^ value) >> 3) & 4095;
}

/* func_80080540 followed by func_8008A058; each squared term truncates separately. */
static int32_t combat_magnitude(int32_t x, int32_t y, int32_t z)
{
    uint32_t value = (uint32_t)(((int64_t)x * x) >> 12) +
                     (uint32_t)(((int64_t)y * y) >> 12) +
                     (uint32_t)(((int64_t)z * z) >> 12);
    if (!value) {
        return 0;
    }
    unsigned leading = (value & 0x80000000u) ? 0 : (unsigned)__builtin_clz(value) & 30u;
    int shift = (int)leading - 24;
    uint32_t normalized = shift >= 0 ? value << shift : value >> -shift;
    uint32_t root = (uint16_t)s_fixed_sqrt[normalized - 64];
    shift = (shift + 6) >> 1;
    return (int32_t)(shift >= 0 ? root >> shift : root << -shift);
}

#include "combat_damage_steps.h"

const ShAttackInfo *sh_combat_attack_info(unsigned attack)
{
    return attack < 70 ? &s_attacks[attack] : NULL;
}

bool sh_combat_damage_apply(ShCombatActor *attacker, ShCombatActor *target,
                           const ShPathPoint *position, int32_t scale,
                           ShCombatDamageContext *ctx, int32_t *hit_mask)
{
    if (!attacker || !target || !position || !ctx || !hit_mask ||
        (uint8_t)attacker->attack >= 70 ||
        (!target->is_player && target->npc_index >= 32) ||
        !ctx->rumble || !ctx->motor_hit || !ctx->sound || !ctx->impact || !ctx->particle) {
        return false;
    }
    *hit_mask = combat_apply(attacker, target, position, scale, ctx);
    return true;
}
