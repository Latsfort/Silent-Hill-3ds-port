"""Adapt original500-slot emitter init/spawn, mode management and CPU state evolution."""
from pathlib import Path
import re
import struct
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed
from gen_fixed_gte import retail

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_boss_particles_steps.h'
TYPES = ROOT / 'include/sh3ds/school_boss_particle_slot.h'


def outputs():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/unk_draw.c').read_text())
    map_h = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/include/bodyprog/map/map.h').read_text())
    end = map_h.index('} s_MapHdr_field_4C;') + len('} s_MapHdr_field_4C;')
    # The type ends after nested unions; the nearest preceding top-level typedef is its start.
    start = map_h.rfind('typedef struct', 0, end)
    layout = map_h[start:end].replace('s_MapHdr_field_4C', 'ShSchoolBossParticleSlot')
    for old, new in [('q19_12', 'int32_t'), ('q20_12', 'uint32_t'), ('q3_12', 'int16_t'), ('q4_12', 'uint16_t'), ('u8', 'uint8_t'), ('u16', 'uint16_t'), ('s16', 'int16_t'), ('s32', 'int32_t')]:
        layout = re.sub(r'\b' + old + r'\b', new, layout)
    types = '#pragma once\n#include <stdint.h>\n/* Generated from bodyprog/map/map.h. */\n' + layout + '\n'
    math = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/bodyprog_math_8005BF38.c').read_text())
    curve = re.search(r'D_800AE520\[34\]\s*=\s*\{([^}]+)', math)[1]
    curve = re.sub(r'Q12_ANGLE\(([-\d.]+)f?\)', lambda m: str(fixed(m[1], True)), curve)
    values = [int(v.strip(), 0) for v in curve.split(',') if v.strip()]
    raw, base, symbols = retail()
    assert len(values) == 34 and tuple(values) == struct.unpack_from('<34h', raw, 0x800AE520 - base)
    def adapt(code):
        replacements = {
            'sharedData_800D8568_1_s05.': 'config->',
            'sharedData_800DFB7C_0_s00': 'pool->slots',
            'D_800C4414': 'pool->effect_flags',
            'sharedFunc_800CABF8_1_s05(': 'boss_particle_spawn(pool, config, ',
            'g_DeltaTime': 'delta', 'Rng_GenerateUInt(': 'boss_particle_random(',
            'Rng_GenerateInt(': 'boss_particle_random(', 'Rng_Rand16()': 'sh_rng_rand16()',
            'Math_Cos(': 'sh_fixed_cos(', 'Math_Sin(': 'sh_fixed_sin(',
            'Q12_MULT_PRECISE(': 'sh_q12_mul(', 'Q12_MULT(': 'BOSS_PARTICLE_MULT(',
            'FP_FROM(': 'BOSS_PARTICLE_FROM(', 'Q12_SHIFT': '12',
            'ARRAY_SIZE(pool->slots)': '500', 'MIN(': 'BOSS_PARTICLE_MIN(',
            'func_8005C7B0(': 'boss_particle_curve(',
        }
        for old in sorted(replacements, key=len, reverse=True): code = code.replace(old, replacements[old])
        code = code.replace('ARRAY_SIZE(pool->slots)', '500')
        code = re.sub(r'Q12(_ANGLE)?\(([-\d.]+)f?\)', lambda m: str(fixed(m[2], bool(m[1]))), code)
        code = code.replace('Q12_CLAMPED(1.0f)', '4095')
        for old, new in [('q19_12', 'int32_t'), ('q3_12', 'int16_t'), ('q4_12', 'uint16_t'), ('s32', 'int32_t'), ('s16', 'int16_t'), ('u16', 'uint16_t'), ('u8', 'uint8_t')]: code = re.sub(r'\b' + old + r'\b', new, code)
        assert not re.search(r'\b(?:g_|sharedData_|sharedFunc_|Rng_|Math_|Q12|func_)\w*', code), code
        return code
    init = function(source, 'sharedFunc_800CAAD0_1_s05')
    spawn = function(source, 'sharedFunc_800CABF8_1_s05')
    effects = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/gfx/bodyprog_effects_8005E0DC.c').read_text())
    setup = effects[effects.index('ptr->field_C4 ='):effects.index('    else if (D_800C4414 & 2)')]
    setup = setup[:setup.rfind('}')]
    setup = setup.replace('g_MapOverlayHdr.field_5C->', 'config->').replace('g_MapOverlayHdr.unkTable1Count_50', '500').replace('g_MapOverlayHdr.unkTable1_4C', 'pool->slots')
    setup = setup.replace('g_MapOverlayHdr.func_60(', 'boss_particle_spawn(pool, config, ')
    setup = setup.replace('CLAMP_LOW(', 'BOSS_PARTICLE_MAX(')
    # Projection-only scratch setup is deferred to the rendering adapter.
    setup = re.sub(r'\s*ptr->field_D8 = [^;]+;', '', setup)
    lines = ['/* Generated original500-slot emitter CPU logic; GPU projection/draw remains separate. */',
        'static const int16_t s_boss_particle_curve[34] = {' + ', '.join(map(str, values)) + '};',
        'static int32_t boss_particle_curve(int32_t angle) { return s_boss_particle_curve[angle >> 5]; }',
        'static void boss_particle_spawn(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config, int32_t idx, bool arg1)', '{', adapt(spawn), '}',
        'static void boss_particle_init(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config)', '{', adapt(init), '}',
        'static void boss_particle_setup(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config, int32_t delta, ShBossParticleFrame *ptr)', '{', 'int32_t i;', adapt(setup), '}', '']
    for name, function_name in [('rise', 'sharedFunc_800CB040_1_s05'), ('fall', 'sharedFunc_800CB884_1_s05')]:
        code = function(source, function_name)
        begin = code.index('sharedData_800DFB7C_0_s00[idx].vy_8 =')
        end = code.index('    gte_ldv0(', begin)
        code = code[begin:end]
        code = re.sub(r'\s*Math_SetSVectorFastSum\(&ptr->field_12C,[\s\S]*?;', '', code)
        code = code.replace('ptr->field_0.', 'frame->')
        code = code.replace('ptr->field_134', 'position->vx').replace('ptr->field_138', 'position->vz')
        lines += [f'static bool boss_particle_{name}(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config, int32_t idx, int32_t delta, const ShBossParticleFrame *frame, ShPathPoint *position)', '{', 'int32_t var_s1;', adapt(code),
            'position->vy = pool->slots[idx].vy_8;', 'return true;', '}', '']
    return {OUTPUT: '\n'.join(lines), TYPES: types}


if __name__ == '__main__':
    for path, text in outputs().items():
        if '--check' in sys.argv: assert path.read_text(encoding='ascii') == text
        else: path.write_text(text, encoding='ascii', newline='\n')
    if '--check' in sys.argv: print('PASS: particle init/spawn/modes/CPU lifetime,20-byte union layout and34 ROM curve entries match retail')
