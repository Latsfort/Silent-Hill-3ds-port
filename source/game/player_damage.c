#include "sh3ds/player_damage.h"
#include <stddef.h>

#define PLAYER_DAMAGE_DIV(a, b) ((a) >> 2)
#define PLAYER_DAMAGE_ANGLE(v) ((v) & 4095)

/* player.h Player_ExtraStateSet. */
static void player_damage_state(ShPlayerDamage *player, int state)
{
    player->state = state;
    player->model_step = player->model_control = 0;
    player->extra_step = player->extra_control = 0;
    player->upper_state = player->lower_state = 0;
}

/* chara.h Chara_DamageClear. */
static void player_damage_clear(ShPlayerDamage *player)
{
    player->actor.damage_amount = 0;
    player->actor.damage_position = (ShPathPoint){0};
}

#include "player_damage_steps.h"

bool sh_player_receive_damage(ShPlayerDamage *player, ShPlayerDamageContext *ctx)
{
    if (!player || !ctx || !ctx->save || !ctx->npc_yaws || !ctx->npc_count ||
        !ctx->sound || !ctx->motor || !ctx->collision_reset || !ctx->grab_feedback || !ctx->damage_feedback ||
        player->actor.attack_received < -1 || player->actor.attack_received >= 70 ||
        (player->actor.attack_received > 0 && !ctx->disable_control && !ctx->disable_damage &&
         (unsigned)player->actor.last_attacker >= ctx->npc_count)) {
        return false;
    }
    player_receive(player, ctx);
    return true;
}
