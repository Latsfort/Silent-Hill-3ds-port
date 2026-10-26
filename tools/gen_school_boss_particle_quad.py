"""Retain original particle texture/color recipes and post-projection primitive geometry."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_boss_particle_quad_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/unk_draw.c').read_text())
    game_h = (ROOT / 'vendor/silent-hill-decomp/include/game.h').read_text()
    buckets = int(re.search(r'#define ORDERING_TABLE_SIZE\s+(\d+)', game_h)[1])
    def adapt(code):
        code = re.sub(r'sharedData_800DFB7C_0_s00\[idx\]\.', 'slot->', code)
        code = re.sub(r'POLY_FT4\* poly2\s*=\s*\*poly;', '', code)
        code = re.sub(r'\*\((?:s32|u32|s16|u16)\*\)&\(\*poly\)->u([0-3])', r'quad->uv\1', code)
        code = code.replace('*(s32*)&(poly2->r0)', 'quad->color')
        code = code.replace('ptr->field_144', 'quad->scale')
        code = re.sub(r'setUV([01])And(?:Clut|TPage)Sum\(\*poly, ([^,]+), ([^,]+), ([^)]+)\)', r'(quad->uv\1 = (\2) + ((\3) << 8) + ((\4) << 16))', code)
        code = re.sub(r'setUV([23])Sum\(\*poly, ([^,]+), ([^)]+)\)', r'(quad->uv\1 = (\2) + ((\3) << 8))', code)
        code = code.replace('setRGBC0(*poly,', 'boss_quad_color(quad,')
        code = code.replace('ptr->field_0.field_2C', 'projection_scale').replace('ptr->field_0.field_D8', '(base_size * (2 * projection_scale))')
        code = code.replace('*(s32*)&ptr->field_140', 'screen_word')
        code = code.replace('ptr->field_140.vx', 'screen_x').replace('ptr->field_140.vy', 'screen_y').replace('ptr->field_13C', 'depth')
        code = re.sub(r'\*\(s32\*\)&\(\*poly\)->x([0-3])', r'quad->xy[\1]', code)
        code = re.sub(r'setXY([0-3])Fast\(\*poly, ([^,]+), ([^)]+)\)', r'(quad->xy[\1] = BOSS_QUAD_XY(\2, \3))', code)
        code = re.sub(r'addPrimFast\([^;]+;', 'quad->depth_bucket = (unsigned)depth >> 3;', code)
        code = code.replace('*poly += 1;', '')
        code = code.replace('ORDERING_TABLE_SIZE', str(buckets))
        code = code.replace('MAX(', 'BOSS_QUAD_MAX(').replace('CLAMP_LOW(', 'BOSS_QUAD_MAX(').replace('ABS(', 'abs(')
        code = re.sub(r'\b(?:PRIM_\w+|RECT_\w+)\b', lambda m: str(g.value(m[0])), code)
        for old, new in [('s32', 'int32_t'), ('u16', 'uint16_t')]: code = re.sub(r'\b' + old + r'\b', new, code)
        code = code.replace('(halfSizeCeil << 16)', '((uint32_t)halfSizeCeil << 16)').replace('(halfSizeFloor << 16)', '((uint32_t)halfSizeFloor << 16)')
        assert not re.search(r'\b(?:gte_|sharedData_|g_|POLY_|ptr|poly|poly2)\w*', code), code
        return code
    lines = ['/* Generated original particle style, clipping and screen geometry; projection is supplied by caller. */']
    for name, func in [('rise', 'sharedFunc_800CB040_1_s05'), ('fall', 'sharedFunc_800CB884_1_s05')]:
        code = function(source, func)
        begin = code.index('if (sharedData_800DFB7C_0_s00[idx].field_A')
        end = code.index('sharedData_800DFB7C_0_s00[idx].vy_8 =', begin)
        style = code[begin:end]
        begin = code.index('if (ptr->field_13C <=')
        geometry = code[begin:]
        declarations = 'int32_t var_s1;' + ('int32_t halfSizeCeil, halfSizeFloor;\nuint32_t screen_word = BOSS_QUAD_XY(screen_x, screen_y);' if name == 'rise' else '\n(void)base_size;')
        lines += [f'static void boss_quad_{name}_style(const ShSchoolBossParticleSlot *slot, ShBossParticleQuad *quad)', '{',
            ('int32_t var_s1;' if name == 'fall' else ''), adapt(style), '}',
            f'static bool boss_quad_{name}_geometry(const ShSchoolBossParticleSlot *slot, ShBossParticleQuad *quad, int16_t screen_x, int16_t screen_y, int32_t depth, int32_t projection_scale, uint8_t base_size)', '{',
            declarations, ('(void)slot;' if name == 'fall' else ''), adapt(geometry), '}', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    result = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == result
        print('PASS: particle packed UV/CLUT/tpage/colors, clipping, depth bucket and quad dimensions match source')
    else: OUTPUT.write_text(result, encoding='ascii', newline='\n')
