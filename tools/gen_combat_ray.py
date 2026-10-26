"""Retain retail ray setup, cylinder/weakpoint shapes, intersections and miss rules."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/combat_ray_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/collision/ray.c').read_text())
    def adapt(code):
        replacements = {
            'g_ActiveCollisionTriggers.flags': 'collision_flags',
            'Q12_TO_Q8(': 'RAY_TO_Q8(', 'Q8_TO_Q12(': 'RAY_TO_Q12(',
            'Q12_MULT(': 'RAY_MULT(', 'Math_Vector2MagCalc(': 'ray_magnitude(',
            '*(s32*)&state->field_58': '(uint32_t)state->field_58 | ((uint32_t)(uint16_t)state->field_5A << 16)',
            'func_8006C248(': 'ray_circle(', 'VECTOR3': 'ShPathPoint',
            'Q12_SHIFT': '12', 'NO_VALUE': '-1', 'GroundType_Default': str(g.value('GroundType_Default')),
            'Q8(0.0f)': '0', 'Q12(0.0f)': '0', 'Q12(1.0f)': '4096',
            'Q12(1.875f)': '7680', 'Q12_ANGLE(0.0f)': '0',
            'SQUARE(': 'RAY_SQUARE(', 'NULL': 'NULL',
        }
        for old in sorted(replacements, key=len, reverse=True):
            code = code.replace(old, replacements[old])
        code = re.sub(r'\bQ12\(', 'RAY_TO_Q12_SCALE(', code)
        code = re.sub(r'\(state->offset\.(v[xyz]) << 12\)', r'(state->offset.\1 * 4096)', code)
        for old, new in [('s32', 'int32_t'), ('q19_12', 'int32_t'), ('q23_8', 'int32_t'), ('q3_12', 'int16_t')]:
            code = re.sub(r'\b' + old + r'\b', new, code)
        assert not re.search(r'\b(?:g_|func_|Q12|Q8|Math_|Chara_)\w*', code), code
        return code
    lines = ['/* Generated from collision/ray.c: Ray_TraceSetup, 8006EE0C, 8006EEB8, Ray_MissSet. */',
        'bool sh_combat_ray_setup(ShRayState *state, bool useCylinder, int16_t radiusOffset, const ShPathPoint *from, const ShPathPoint *offset, int32_t arg5, int32_t arg6, ShRayCharacter **collCharas, int32_t collCharaCount, int16_t collision_flags)', '{',
        adapt(function(source, 'Ray_TraceSetup')), '}',
        'static void ray_shape(ShRayShape *arg0, bool useCylinder, const ShRayCharacter *chara)', '{',
        adapt(function(source, 'func_8006EE0C')), '}',
        'static void ray_character(ShRayState *state, ShRayCharacter *chara)', '{',
        adapt(function(source, 'func_8006EEB8')), '}',
        'static void ray_miss(ShRayTrace *trace, const ShPathPoint *from, const ShPathPoint *offset, int32_t dist)', '{',
        adapt(function(source, 'Ray_MissSet')), '}', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    result = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == result
        print('PASS: retail ray setup, both shape selections, intersection and miss functions match source')
    else:
        OUTPUT.write_text(result, encoding='ascii', newline='\n')
