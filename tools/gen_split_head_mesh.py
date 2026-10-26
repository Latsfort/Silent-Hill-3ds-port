"""Extract both original Split Head mesh swaps through Bone_ModelIdxGet's packed ranges."""
from pathlib import Path
import re
import struct
import sys
from gen_school_smoke import function
import gen_map_data as g

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/split_head_mesh.h'
TWIST_OUTPUT = ROOT / 'source/game/split_head_twist.h'


def indices(value):
    data = struct.pack('<I', value)
    pointer, current = 0, 0
    result = []
    while True:
        code = struct.unpack_from('<b', data, pointer)[0]
        if code != -3:
            current = code
            pointer += 1
        else:
            current += 1
            if current >= data[pointer + 1] - 1: pointer += 1
        if current == -2: return result
        assert current >= 0 and current not in result
        result.append(current)


def emit():
    source = (ROOT / 'vendor/silent-hill-decomp/src/bodyprog/world/world_draw.c').read_text()
    body = function(g.strip_comments(source), 'WorldGfx_SplitHeadMeshSwap')
    packed = {name: int(value, 16) for name, value in re.findall(r'static s32 (D_\w+) = (0x[\dA-Fa-f]+);', body)}
    output = ['/* Generated from WorldGfx_SplitHeadMeshSwap / Bone_ModelIdxGet. */',
        'static bool split_model_active(unsigned model, unsigned flags)', '{', '    switch (flags & (SH_CHARA_F_SPLIT_HEAD_1 | SH_CHARA_F_SPLIT_HEAD_2)) {']
    for variant in (1, 2):
        block = body.split(f'case SplitHeadVariantMesh_{variant}:', 1)[1].split('break;', 1)[0]
        hidden, = re.findall(r'func_80045468\(skel, &(D_\w+), false\)', block)
        visible, = re.findall(r'func_80045468\(skel, &(D_\w+), true\)', block)
        assert not set(indices(packed[hidden])) & set(indices(packed[visible]))
        output += [f'    case SH_CHARA_F_SPLIT_HEAD_{variant}:', '        switch (model) {',
            ''.join(f'        case {i}:\n' for i in indices(packed[hidden])) + '            return false;',
            '        default: return true;', '        }']
    output += ['    default: return true;', '    }', '}', '']
    return '\n'.join(output)


def emit_twist():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/characters/split_head.c').read_text())
    body = function(source, 'SplitHead_AnimUpdate').split('if (splitHeadProps.field_EE != Q12(0.0f))', 1)[1]
    root = int(re.search(r'MulMatrix\(&sharedData_800D8610_1_s05\[(\d+)\]', body)[1])
    first, limit = map(int, re.search(r'for \(i = (\d+); i < (\d+); i\+\+\)', body).groups())
    shift = int(re.search(r'field_EE >> (\d+)', body)[1])
    return '\n'.join(['/* Generated from SplitHead_AnimUpdate procedural bone matrix calls. */',
        'static bool split_twist_bones(BoneXform *local, int count, int16_t angle)', '{',
        '    if (!angle) { return true; }', f'    if (count < {limit}) {{ return false; }}',
        f'    split_twist_y(&local[{root}], angle);',
        f'    for (int i = {first}; i < {limit}; i++) {{ split_twist_y(&local[i], angle >> {shift}); }}',
        '    return true;', '}', ''])


if __name__ == '__main__':
    data = emit()
    twist = emit_twist()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        assert TWIST_OUTPUT.read_text(encoding='ascii') == twist
        print('PASS: both original packed head-mesh ranges and procedural bone twist indices/angle shift')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
        TWIST_OUTPUT.write_text(twist, encoding='ascii', newline='\n')
