"""Adapt retail func_8008B714 and all 70 attack records without hand-copying stats."""
from pathlib import Path
import re
import struct
import sys
import gen_map_data as g
from gen_fixed_gte import retail
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/combat_damage_steps.h'


def emit():
    raw, base, symbols = retail()
    records = raw[symbols['D_800AD4C8'] - base:symbols['D_800AD4C8'] - base + 70 * 24]
    assert len(records) == 70 * 24
    lines = ['/* Generated from func_8008B714 and SHA1-verified BODYPROG attack records. */',
             'static const ShAttackInfo s_attacks[70] = {']
    for record in struct.iter_unpack('<HhHbbBBBBHBBBBBbI', records):
        values = record[:15]
        lines.append('    {' + ', '.join(map(str, values)) + '},')
    lines += ['};']
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/bodyprog_combat_8008A058.c').read_text())
    code = function(source, 'func_8008B714')
    replacements = {
        'target == &g_SysWork.playerWork.player': 'target->is_player',
        'target != &g_SysWork.playerWork.player': '!target->is_player',
        'attacker->field_44.field_2': 'attacker->attack',
        'attacker->field_44.field_8': 'attacker->hit_mask',
        'attacker->field_44.field_14': 'attacker->attack_frame',
        'attacker->field_44.field_24': 'attacker->previous',
        'attacker->field_44.field_48': 'attacker->current',
        'target->field_40': 'target->last_attacker',
        'target->model.charaId': 'target->chara_id',
        'target->damage.amount': 'target->damage_amount',
        'target->damage.position': 'target->damage_position',
        'target->attackReceived': 'target->attack_received',
        'g_SysWork.field_275C': 'ctx->charge',
        'Game_HyperBlasterBeamColorGet()': 'ctx->beam_color',
        '&D_800AD4C8[weaponAttack]': '&s_attacks[weaponAttack]',
        's_800AD4C8*': 'const ShAttackInfo*',
        'temp_fp->field_C': 'temp_fp->field_c',
        'Rng_RandQ12()': 'combat_random()',
        'func_8008B3E4(': 'ctx->motor_hit(ctx->user, ',
        'func_800892A4(': 'ctx->rumble(ctx->user, ',
        'Sfx_WithFlagsPlay(': 'ctx->sound(ctx->user, ',
        'func_8008B664(': 'ctx->impact(ctx->user, ',
        'func_8005F6B0(': 'ctx->particle(ctx->user, ',
        'func_8008A35C(temp_fp, attacker->field_44.field_14)': '(attacker->attack_frame >= temp_fp->field_e && attacker->attack_frame < temp_fp->field_e + temp_fp->field_f)',
        'func_8008A058(func_80080540(offsetX, offsetY, offsetZ))': 'combat_magnitude(offsetX, offsetY, offsetZ)',
        'Q12_MULT_PRECISE(': 'COMBAT_MULT(',
        'NO_VALUE': '-1',
    }
    for old in sorted(replacements, key=len, reverse=True):
        code = code.replace(old, replacements[old])
    code = re.sub(r'\bs32\s+var_v0;', '', code)
    old = '(((s32)((u32)((u8*)attacker - sizeof(s_PlayerWork)) - (u32)target) * -0x6EB3E453) >> 3)'
    assert old in code
    code = code.replace(old, '(int32_t)attacker->npc_index')
    old = '1 << (((s32)((u32)((u8*)target - sizeof(s_PlayerWork)) - (u32)&g_SysWork.playerWork) * -0x6EB3E453) >> 3)'
    assert old in code
    code = code.replace(old, '(int32_t)(1u << target->npc_index)')
    code = re.sub(r'WEAPON_ATTACK\((\w+),\s*(\w+)\)', lambda m: str(g.value(m[1]) + g.value(m[2]) * 10), code)
    code = re.sub(r'Q12\(([-\d.]+)f?\)', lambda m: str(fixed(m[1])), code)
    code = code.replace('Q12(temp_fp->field_4)', '((uint32_t)temp_fp->field_4 * 4096u)')
    code = re.sub(r'Q8\(([-\d.]+)f?\)', lambda m: str(int(float(m[1]) * 256)), code)
    code = re.sub(r'\b(?:Chara_\w+|CharaFlag_\w+|Sfx_\w+|SfxFlag_\w+)\b', lambda m: str(g.value(m[0])), code)
    for old, new in [('q19_12', 'int32_t'), ('q20_12', 'int32_t'), ('s64', 'int64_t'), ('s32', 'int32_t'), ('u32', 'uint32_t'), ('u8', 'uint8_t')]:
        code = re.sub(r'\b' + old + r'\b', new, code)
    code = code.replace('\n                default:', '\n                    /* fall through */\n                default:')
    assert not re.search(r'\b(?:g_|func_|WEAPON_|Q12|Rng_|Chara_|Sfx_)\w*', code), code
    lines += ['static int32_t combat_apply(ShCombatActor *attacker, ShCombatActor *target, const ShPathPoint *attackPos, int32_t arg3, ShCombatDamageContext *ctx)', '{', code, '}', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    result = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == result
        # Decode every emitted field independently and compare against the ROM layout.
        raw, base, symbols = retail()
        rows = re.findall(r'^    \{([^{}]+)\},$', result, re.M)
        assert len(rows) == 70
        for index, row in enumerate(rows):
            emitted = tuple(map(int, row.split(',')))
            original = struct.unpack_from('<HhHbbBBBBHBBBBBbI', raw, symbols['D_800AD4C8'] - base + index * 24)
            assert emitted == original[:15]
        print('PASS: complete damage routine and all 70 attack records (1050 fields) match source/ROM')
    else:
        OUTPUT.write_text(result, encoding='ascii', newline='\n')
