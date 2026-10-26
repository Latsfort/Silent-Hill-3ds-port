"""Extract the two retail Anim_BoneUpdate fixed-point interpolation expressions."""
from pathlib import Path
import re
import sys
from gen_school_smoke import function
import gen_map_data as g

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/bone_interpolation.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/world/bodyprog_anim_800445A4.c').read_text())
    body = function(source, 'Anim_BoneUpdate')
    translation = re.search(r'curBoneCoord->coord.t\[i\]\s*=\s*([^;]+);', body)[1]
    rotation = re.search(r'curBoneCoord->coord.m\[i\]\[j\]\s*=\s*([^;]+);', body)[1]
    def adapt(expr):
        expr = expr.replace('*frame0TranslationData', 'a').replace('*frame1TranslationData', 'b')
        expr = expr.replace('*frame0RotationData', 'a').replace('*frame1RotationData', 'b')
        expr = expr.replace('Q12_SHIFT', '12').replace('scaleLog2', 'scale')
        expr = expr.replace('(a << scale)', '(a * (1 << scale))').replace('(a << 5)', '(a * 32)')
        return ' '.join(expr.split())
    return '\n'.join(['/* Generated from Anim_BoneUpdate; scaled translations and matrix entries, no renormalization. */',
        'static int32_t bone_translation_lerp(int32_t a, int32_t b, int32_t alpha, unsigned scale)', '{',
        '    return ' + adapt(translation) + ';', '}',
        'static int32_t bone_rotation_lerp(int32_t a, int32_t b, int32_t alpha)', '{',
        '    return ' + adapt(rotation) + ';', '}', ''])


if __name__ == '__main__':
    result = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == result
        print('PASS: scaled translation and rotation matrix interpolation expressions match Anim_BoneUpdate')
    else: OUTPUT.write_text(result, encoding='ascii', newline='\n')
