"""Adapt the entire retail animation-driven attack dispatcher and vector history helpers."""
from pathlib import Path
import re
import sys
import struct
import gen_map_data as g
from gen_fixed_gte import retail
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/combat_attack_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/bodyprog_combat_8008A058.c').read_text())
    table = re.search(r'D_800AFD1C\[\]\s*=\s*\{([^}]+)', source)[1]
    entries = []
    for entry in table.split(','):
        entry = entry.strip()
        entries.append(fixed(re.search(r'\(([^)]+)\)', entry)[1].rstrip('f'), True) if entry.startswith('Q12_ANGLE') else int(entry, 0))
    assert len(entries) == 16
    raw, base, symbols = retail()
    angle_address = int(re.search(r'q3_12 D_([0-9A-F]+)\[\]', source)[1], 16)
    assert tuple(entries) == struct.unpack_from('<16h', raw, angle_address - base)
    # SquareRoot12's exponent and table indexing are verified against its retail instructions.
    root = struct.unpack_from('<35I', raw, symbols['SquareRoot12'] - base)
    assert root[:4] == (0x4884f000, 0, 0, 0x4802f800)
    assert root[8:16] == (0x2401fffe, 0x00415024, 0x24090013, 0x012a4822,
                          0x00094843, 0x214bffe8, 0x05600003, 0)
    def adapt(code):
        code = re.sub(r'\s*s_ModelAnim\*\s+var_s0;', '', code)
        code = re.sub(r'\s*s_AnimInfo\*\s+anim;', '', code)
        begin = code.find('if (chara == &g_SysWork.playerWork.player)')
        if begin >= 0:
            end = code.index('var_s0_2 = func_8008A270', begin)
            code = code[:begin] + 'sp28 = ctx->animation_time - ctx->animation_start * 4096;\n\n    ' + code[end:]
        code = re.sub(r'\*\(u16\*\)&sp1C->field_6', '((uint16_t)(uint8_t)sp1C->field_6 | ((uint16_t)(uint8_t)sp1C->unk_7 << 8))', code)
        replacements = {
            'chara->model.charaId': 'chara->chara_id',
            'temp_s1_2->model.charaId': 'temp_s1_2->chara_id',
            'chara->field_44': '*chara',
            '&*chara': 'chara',
            'D_800AFD1C': 's_attack_angles',
            'g_DeltaTime': 'ctx->delta', 'func_8007FD2C()': 'ctx->player_accuracy',
            'g_SysWork.field_2388.field_154.effectsInfo.flags.field_00[0] & SpecialEnvEventFlags_DarkEnvironment': 'ctx->dark',
            'g_SysWork.targetNpcIdx': 'ctx->target_index',
            '&g_SysWork.npcs[ctx->target_index]': 'ctx->npcs[ctx->target_index]',
            'func_8008A270(sp14)': 'attack_start(ctx, sp14)', 'func_8008A2E0(sp14)': 'attack_duration(ctx, sp14)',
            'g_SysWork.field_275C': 'ctx->damage->charge',
            '&D_800AD4C8[sp14]': 'sh_combat_attack_info((unsigned)sp14)',
            '&D_800AD4C8[idx46]': 'sh_combat_attack_info((unsigned)idx46)',
            'Chara_Flag8Set(chara)': 'chara->flags |= CharaFlag_Unk8',
            'func_8008B15C(chara)': 'attack_begin(chara)',
            'func_8008B1DC(': 'attack_vector(',
            'func_8008BF84(chara,': 'ctx->melee(ctx->user, chara,',
            'func_800892A4(': 'ctx->damage->rumble(ctx->damage->user, ',
            'func_8009151C(': 'ctx->feedback(ctx->user, ',
            'Ray_CharaTraceQuery(&D_800C4728, &chara->field_44.field_18, &chara->field_44.field_48[0], chara)': 'ctx->trace(ctx->user, &chara->attack_origin, &chara->current[0], chara, &D_800C4728)',
            'func_8008B714(chara, ptr, &D_800C4728.target, 0)': 'attack_damage(chara, ptr, &D_800C4728.target, ctx)',
            'Rng_RandQ12()': 'attack_random()',
            'SquareRoot12(': 'sh_fixed_sqrt12(', 'Math_Vector2MagCalc(': 'attack_magnitude(',
            'Math_Sin(': 'sh_fixed_sin(', 'Math_Cos(': 'sh_fixed_cos(', 'ratan2(': 'sh_fixed_atan2(',
            'Q12_MULT_PRECISE(': 'sh_q12_mul(', 'Q12_MULT(': 'ATTACK_MULT(',
            's_800AD4C8*': 'const ShAttackInfo*', 's_SubCharacter_44*': 'ShCombatActor*',
            's_SubCharacter*': 'ShCombatActor*', 's_RayTrace': 'ShAttackTrace',
            'DVECTOR': 'ShAttackAngles', 'VECTOR3': 'ShPathPoint',
            'NO_VALUE': '-1', 'static ': '',
        }
        # Dependent global target and field rewrites are performed explicitly first.
        code = code.replace('&g_SysWork.npcs[g_SysWork.targetNpcIdx]', 'ctx->npcs[ctx->target_index]')
        for old in sorted(replacements, key=len, reverse=True):
            code = code.replace(old, replacements[old])
        code = code.replace('&*chara', 'chara')
        fields = {'0': 'attack_state', '2': 'attack', '3': 'shot_budget', '4': 'shot_count', '8': 'hit_mask',
                  'C': 'angle_c', 'E': 'angle_e', '10': 'angle_10', '12': 'angle_12', '14': 'attack_frame',
                  '18': 'attack_origin', '24': 'previous', '48': 'current'}
        code = re.sub(r'\*chara\.field_([\dA-F]+)', lambda m: 'chara->' + fields[m[1]], code)
        code = re.sub(r'(base|temp_v0_12)->field_([\dA-F]+)', lambda m: m[1] + '->' + fields[m[2]], code)
        for old, new in [('field_A', 'field_a'), ('field_B', 'field_b'), ('charaId_9', 'chara_id')]:
            code = code.replace('sp1C->' + old, 'sp1C->' + new)
        code = re.sub(r'D_800AD4C8\[(\w+)\].field_([EF])', lambda m: 'sh_combat_attack_info((unsigned)' + m[1] + ')->field_' + m[2].lower(), code)
        code = re.sub(r'Q12(_ANGLE)?\(([-\d.]+)f?\)', lambda m: str(fixed(m[2], bool(m[1]))), code)
        code = re.sub(r'\bQ12\(', 'ATTACK_Q12(', code)
        code = code.replace('CHARA_LAST_ENEMY_ID', 'Chara_MonsterCybil')
        code = re.sub(r'\b(?:Chara_\w+|CharaFlag_\w+)\b', lambda m: str(g.value(m[0])), code)
        for old, new in [('q19_12', 'int32_t'), ('s32', 'int32_t'), ('u32', 'uint32_t'), ('u16', 'uint16_t'), ('u8', 'uint8_t')]:
            code = re.sub(r'\b' + old + r'\b', new, code)
        assert not re.search(r'\b(?:g_|func_|Chara_|Ray_|Math_|Q12|D_800AD4C8)\w*', code), code
        return code
    start = function(source, 'func_8008A270')
    start = re.sub(r'#if VERSION_EQUAL_OR_NEWER\(USA\)(.*?)#else.*?#endif', r'\1', start, flags=re.S)
    lines = ['/* Generated from complete func_8008A3E0/8008B15C/8008B1DC and attack timing helpers; do not edit. */',
             'static const int16_t s_attack_angles[16] = {' + ', '.join(map(str, entries)) + '};',
             'static uint32_t attack_start(ShAttackContext *ctx, int32_t idx)', '{', adapt(start), '}',
             'static uint32_t attack_duration(ShAttackContext *ctx, int32_t arg0)', '{', adapt(function(source, 'func_8008A2E0')), '}',
             'static void attack_vector(ShCombatActor *chara, int32_t angle0, int32_t angle1)', '{', adapt(function(source, 'func_8008B1DC')), '}',
             'static void attack_begin(ShCombatActor *chara)', '{', adapt(function(source, 'func_8008B15C')), '}',
             'static int32_t attack_update(ShCombatActor *chara, ShAttackContext *ctx)', '{', adapt(function(source, 'func_8008A3E0')), '}', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    result = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == result
        print('PASS: complete attack dispatcher, vector/history helpers and 16 source angles match retail')
    else:
        OUTPUT.write_text(result, encoding='ascii', newline='\n')
