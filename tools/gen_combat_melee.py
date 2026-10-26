"""Adapt complete retail func_8008BF84, including candidate order, clipping and occlusion."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/combat_melee_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/bodyprog_combat_8008A058.c').read_text())
    code = function(source, 'func_8008BF84')
    code = re.sub(r'\s*static VECTOR3\s+D_800C4788\[4\];', '', code)
    code = re.sub(r'\s*D_800C4788\[\d\]\.v[xyz]\s*=\s*[^;]+;', '', code)
    code = re.sub(r'\s*static s32\s+__pad_\w+(?:\[\d+\])?;', '', code)
    code = re.sub(r'\s*s_SubCharacter\*\s+chara0;', '', code)
    code = re.sub(r'\s*chara0 = [^;]+;', '', code)
    code = code.replace('chara1 = chara0;', '')
    code = code.replace('for (i = var_v0; i > 0; i--, chara1++, sp18 *= 2)\n    {',
        'for (i = var_v0; i > 0; i--, sp18 *= 2)\n    {\n        chara1 = chara->is_player ? ctx->npcs[6 - i] : ctx->player;\n        if (!chara1) { continue; }')
    for field, high in [('6', 'unk_7'), ('8', 'chara_id'), ('A', 'field_b')]:
        low = {'6': 'field_6', '8': 'field_8', 'A': 'field_a'}[field]
        code = code.replace('*(u16*)&arg2->field_' + field, f'((uint16_t)(uint8_t)arg2->{low} | ((uint16_t)(uint8_t)arg2->{high} << 8))')
    replacements = {
        'chara == &g_SysWork.playerWork.player': 'chara->is_player',
        'g_SysWork.sysState == SysState_GameOver': 'ctx->game_over',
        'g_SysWork.playerWork.player.health': 'ctx->player->health',
        'chara1->model.charaId': 'chara1->chara_id',
        'chara1->collision.state': 'chara1->collision_state',
        'chara->collision.box.top': 'chara->shape->collision.box.top',
        'chara1->collision.': 'chara1->shape->collision.',
        'chara->field_44.field_24': 'chara->previous',
        'chara->field_44.field_48': 'chara->current',
        'chara->field_44.field_8': 'chara->hit_mask',
        'func_8008B714(chara, chara1, &D_800C47B8, arg3)': 'melee_damage(chara, chara1, &D_800C47B8, arg3, ctx)',
        'Ray_TraceQuery(&D_800C47F8, &D_800C47C8[0], &D_800C47C8[1])': 'melee_world_trace(&D_800C47F8, &D_800C47C8[0], &D_800C47C8[1], ctx)',
        'Math_Distance2dGet(': 'melee_distance(',
        'func_8008A058(func_80080540(unkX, Q12(0.0f), unkZ))': 'melee_length(unkX, unkZ)',
        'Math_Sin(': 'sh_fixed_sin(', 'Math_Cos(': 'sh_fixed_cos(', 'ratan2(': 'sh_fixed_atan2(',
        'Q12_MULT_PRECISE(': 'sh_q12_mul(',
        's_SubCharacter*': 'ShCombatActor*', 's_RayTrace': 'ShRayTrace', 'VECTOR3': 'ShPathPoint', 'static ': '',
    }
    for old in sorted(replacements, key=len, reverse=True):
        code = code.replace(old, replacements[old])
    code = re.sub(r'Q12\(([-\d.]+)f?\)', lambda m: str(fixed(m[1])), code)
    code = re.sub(r'\bQ12\(', 'MELEE_Q12(', code)
    code = re.sub(r'\b(?:Chara_\w+|CharaCollisionState_\w+)\b', lambda m: str(g.value(m[0])), code)
    for old, new in [('q19_12', 'int32_t'), ('s32', 'int32_t'), ('u16', 'uint16_t')]:
        code = re.sub(r'\b' + old + r'\b', new, code)
    assert not re.search(r'\b(?:g_|func_|Chara_|Ray_|Math_|Q12|chara0)\w*', code), code
    return '\n'.join(['/* Generated from complete func_8008BF84; do not edit. */',
        'static int32_t melee_apply(ShCombatActor *chara, int32_t angle, const ShAttackInfo *arg2, int32_t arg3, ShMeleeContext *ctx)', '{', code, '}', ''])


if __name__ == '__main__':
    result = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == result
        print('PASS: complete retail melee candidate/shape/occlusion/contact/damage routine matches source')
    else:
        OUTPUT.write_text(result, encoding='ascii', newline='\n')
