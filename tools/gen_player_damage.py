"""Adapt complete Player_ReceiveDamage, its original state macro and shortest-angle helper."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/player_damage_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/player_control.c').read_text())
    code = function(source, 'Player_ReceiveDamage')
    code = re.sub(r'\s*#define attacker[^\n]*', '', code)
    code = code.replace('#undef attacker', '')
    replacements = {
        'g_SysWork.playerWork.player.collision.cylinder.field_2': 'player->collision_radius',
        'player->collision.cylinder.field_2': 'player->collision_radius',
        'player->damage.amount': 'player->actor.damage_amount',
        'player->damage.position': 'player->actor.damage_position',
        'player->attackReceived': 'player->actor.attack_received',
        'player->health': 'player->actor.health', 'player->field_40': 'player->actor.last_attacker',
        'player->field_44.field_0': 'player->actor.attack_state',
        'player->rotation.vy': 'player->yaw',
        'player->properties.player.afkTimer': 'player->afk_timer',
        'player->properties.player.field_F4': 'player->field_f4',
        'player->properties.player.field_10C': 'player->field_10c',
        'playerProps.flags': 'player->flags', 'playerProps.moveSpeed': 'player->move_speed',
        'playerExtra.state': 'player->state',
        'playerCombat.weaponAttack': 'player->weapon_attack', 'playerCombat.isAiming': 'player->aiming',
        'g_SavegamePtr->mapIdx': 'ctx->save->map_idx', 'g_SavegamePtr->gameDifficulty': 'ctx->difficulty',
        'g_SavegamePtr->equippedWeapon': 'ctx->save->equipped_weapon',
        'g_SavegamePtr->healthSaturation': 'ctx->save->health_saturation',
        'g_Player_DisableControl': 'ctx->disable_control', 'g_Player_DisableDamage': 'ctx->disable_damage',
        'g_Player_IsInWalkToRunTransition': 'ctx->walk_to_run', 'g_Player_PrevAttackReceived': 'ctx->previous_attack',
        'g_Player_IsDead': 'ctx->dead', 'g_Player_HeadingAngle': 'player->heading_angle',
        'g_SysWork.targetNpcIdx': 'ctx->target_npc', 'g_SysWork.npcIdxs': 'ctx->npc_indices',
        'g_SysWork.npcs[player->field_40].rotation.vy': 'ctx->npc_yaws[player->actor.last_attacker]',
        'attacker.rotation.vy': 'ctx->npc_yaws[0]',
        'D_800AD4C8[player->attackReceived].field_11': 'sh_combat_attack_info((unsigned)player->actor.attack_received)->field_11',
        'Sfx_WithFlagsPlay(': 'ctx->sound(ctx->user, ',
        'func_8004C564(': 'ctx->motor(ctx->user, ', 'func_80089494()': 'ctx->grab_feedback(ctx->user)',
        'func_800893D0(': 'ctx->damage_feedback(ctx->user, ',
        'Player_CollisionReset()': 'ctx->collision_reset(ctx->user)',
        'Player_ExtraStateSet(player, extra,': 'player_damage_state(player,',
        'Math_ShortestAngleGet(': 'player_damage_angle(',
        'Chara_DamageClear(player)': 'player_damage_clear(player)',
        'Q8(1.0f / 8.0f)': '32', 'NO_VALUE': '-1',
        'DIV_FAST(': 'PLAYER_DAMAGE_DIV(',
        'Q12_ANGLE_NORM_U(': 'PLAYER_DAMAGE_ANGLE(',
    }
    for old in sorted(replacements, key=len, reverse=True):
        code = code.replace(old, replacements[old])
    code = re.sub(r'WEAPON_ATTACK\((\w+),\s*(\w+)\)', lambda m: str(g.value(m[1]) + g.value(m[2]) * 10), code)
    code = re.sub(r'Q12(_ANGLE)?\(([-\d.]+)f?\)', lambda m: str(fixed(m[2], bool(m[1]))), code)
    code = re.sub(r'\b(?:PlayerState_\w+|PlayerFlag_\w+|GameDifficulty_\w+|MapIdx_\w+|InvItemId_\w+|Sfx_\w+|SfxFlag_\w+)\b', lambda m: str(g.value(m[0])), code)
    for old, new in [('q3_12', 'int16_t'), ('q4_12', 'uint16_t'), ('s32', 'int32_t'), ('u32', 'uint32_t'), ('s16', 'int16_t')]:
        code = re.sub(r'\b' + old + r'\b', new, code)
    code = code.replace('\n                case 68:', '\n                    /* fall through */\n                case 68:')
    code = code.replace('\n                case 40:', '\n                    /* fall through */\n                case 40:')
    assert not re.search(r'\b(?:g_|func_|Player_|playerProps|playerExtra|playerCombat|Q12|WEAPON_|Sfx_)\w*', code), code
    angle = function(source, 'Math_ShortestAngleGet')
    angle = re.sub(r'Q12_ANGLE\(([-\d.]+)f?\)', lambda m: str(fixed(m[1], True)), angle).replace('q3_12', 'int16_t')
    return '\n'.join(['/* Generated from Player_ReceiveDamage, Math_ShortestAngleGet; do not edit. */',
        'static void player_damage_angle(int16_t angleFrom, int16_t angleTo, int16_t *shortestAngle)', '{', angle, '}',
        'static void player_receive(ShPlayerDamage *player, ShPlayerDamageContext *ctx)', '{', code, '}', ''])


if __name__ == '__main__':
    result = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == result
        print('PASS: complete Player_ReceiveDamage and shortest-angle helper match retail source')
    else:
        OUTPUT.write_text(result, encoding='ascii', newline='\n')
