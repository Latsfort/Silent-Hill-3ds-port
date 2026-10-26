"""Adapt the original grid walk, wall and prop ray intersections from collision/ray.c."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/combat_ray_world_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/collision/ray.c').read_text())
    def adapt(code):
        replacements = {
            'func_8006E490(': 'world_cell(', 'func_8006E78C(': 'world_edge(', 'func_8006EB8C(': 'world_prop(',
            's_IpdCollisionData_18': 'RayWorldProp', 's_IpdCollSubcell': 'RayWorldEdge',
            'SVECTOR3': 'RayWorldShort', 'SVECTOR': 'RayWorldShort', 'VECTOR': 'RayWorldVector', 'DVECTOR': 'RayWorldPair',
            'Q12_MULT(': 'WORLD_MULT(', 'Q12_FRACT(': 'WORLD_FRACT(',
            'FP_FROM(': 'WORLD_FROM(', 'Q12_SHIFT': '12', 'SQUARE(': 'WORLD_SQUARE(',
            'SquareRoot0(': 'sh_fixed_sqrt0(',
            'subcellRanges[0].field_0': 'world_read16(subcellRanges)',
            'subcellRanges[1].field_0': 'world_read16(subcellRanges + 4)',
            'collData->ptr_28': 'collData->ptr28', 'collData->subcellCount': 'collData->sub_n',
            'collData->splitVertices': 'collData->split_vertices',
            'collData->subcellCheckCount >= collData->subcellCheckIdxs[idx]': '!checked[idx]',
            'collData->subcellCheckIdxs[idx] = collData->subcellCheckCount + 1': 'checked[idx] = true',
            'curSubcell = &collData->subcells[idx];': 'RayWorldEdge decoded_edge = world_decode_edge(collData->subcells + idx * 10);\n                curSubcell = &decoded_edge;',
            'temp_a1_2 = &collData->ptr_18[idx - collData->sub_n];': 'RayWorldProp decoded_prop = world_decode_prop(collData->ptr18 + (idx - collData->sub_n) * 10);\n                temp_a1_2 = &decoded_prop;',
        }
        # Replace the original spelling before the subcellCount field is renamed.
        code = code.replace('temp_a1_2 = &collData->ptr_18[idx - collData->subcellCount];',
                            'temp_a1_2 = &collData->ptr_18[idx - collData->sub_n];')
        for old in sorted(replacements, key=len, reverse=True):
            code = code.replace(old, replacements[old])
        code = re.sub(r'(collData->surfaces|surfaces)\[([^]]+)\].groundType', r'world_ground(\1, \2)', code)
        code = re.sub(r'Q12\(([-\d.]+)f?\)', lambda m: str(fixed(m[1])), code)
        code = re.sub(r'Q8\(([-\d.]+)f?\)', lambda m: str(int(float(m[1]) * 256)), code)
        code = re.sub(r'\bQ12\(', 'WORLD_Q12(', code)
        code = re.sub(r'\b(?:OrientationFlags_\w+|GroundType_\w+)\b', lambda m: str(g.value(m[0])), code)
        for old, new in [('q19_12', 'int32_t'), ('q23_8', 'int32_t'), ('q7_8', 'int16_t'), ('s32', 'int32_t'), ('u32', 'uint32_t'), ('s16', 'int16_t'), ('u16', 'uint16_t')]:
            code = re.sub(r'\b' + old + r'\b', new, code)
        assert not re.search(r'\b(?:gte_|func_|Q12|Q8|s_Ipd|OrientationFlags_|GroundType_)\w*', code), code
        return code

    edge = function(source, 'func_8006E78C')
    edge = edge.replace('splitVert1 = &splitVerts[subcell->splitVertexIdx1];', 'RayWorldShort vertex1 = world_vertex(splitVerts, subcell->splitVertexIdx1);\n    splitVert1 = &vertex1;')
    edge = edge.replace('splitVert0 = &splitVerts[subcell->splitVertexIdx0];', 'RayWorldShort vertex0 = world_vertex(splitVerts, subcell->splitVertexIdx0);\n    splitVert0 = &vertex0;')
    begin = edge.index('unkXz = PACKED_XZ16')
    end = edge.index('if ((sp0.vy & 0x8000)', begin)
    edge = edge[:begin] + '''unkXz = world_packed(state);
        sp0 = world_project(unkXz, splitVert1->vx - state->field_6C.groundHeight, splitVert1->vz - state->field_6C.topHeight);
        sp8 = world_project(unkXz, splitVert0->vx - state->field_6C.groundHeight, splitVert0->vz - state->field_6C.topHeight);

        ''' + edge[end:]
    edge = re.sub(r'gte_ldsxy3\([^;]+;\s*gte_nclip\(\);', '', edge)
    edge = edge.replace('gte_stMAC0()', 'world_nclip(sp0, sp8)')
    begin = edge.index('gte_lddp(var_v1);')
    end = edge.index('maybeSplitDiff.vx +=', begin)
    edge = edge[:begin] + '''maybeSplitDiff = world_gpf(var_v1, splitVert0->vx - splitVert1->vx, splitVert0->vy - splitVert1->vy, splitVert0->vz - splitVert1->vz);

                    ''' + edge[end:]
    edge = edge.replace('(sp0.vy << 12)', '(sp0.vy * 4096)')
    prop = function(source, 'func_8006EB8C')
    begin = prop.index('temp_v1 = PACKED_XZ16')
    end = prop.index('if (-radiusOffset', begin)
    prop = prop[:begin] + '''temp_v1 = world_packed(state);
    sp10 = world_project(temp_v1, arg1->offset.vx - state->field_6C.groundHeight, arg1->offset.vz - state->field_6C.topHeight);

    ''' + prop[end:]
    begin = prop.index('gte_lddp(Q12(hitDist)')
    end = prop.index('if ((sp18.vy', begin)
    prop = prop[:begin] + '''sp18 = world_gpf(Q12(hitDist) / state->rayDistance, state->offset.vx, state->offset.vy, state->offset.vz);

            ''' + prop[end:]
    lines = ['/* Generated from retail collision/ray.c; explicit portable PS1 scratch/GTE adapters. */',
        'static void world_cell(RayWorldWalk *arg0, uint32_t flags, int32_t posX, int32_t posZ)', '{', adapt(function(source, 'func_8006E490')), '}',
        'static void world_walk(RayWorldWalk *arg0, RayWorldPair arg1, RayWorldPair arg2)', '{', adapt(function(source, 'func_8006E150')), '}',
        'static void world_edge(ShRayState *state, RayWorldEdge *subcell, const uint8_t *splitVerts, const uint8_t *surfaces, bool hasSurface)', '{', adapt(edge), '}',
        'static void world_prop(ShRayState *state, RayWorldProp *arg1)', '{', adapt(prop), '}',
        'static void world_range(ShRayState *state, const uint8_t *subcellRanges, const ShCollChunk *collData, bool checked[256])', '{', adapt(function(source, 'func_8006E53C')), '}', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    result = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == result
        print('PASS: retail grid traversal, candidate filtering, wall and prop intersection functions match source')
    else:
        OUTPUT.write_text(result, encoding='ascii', newline='\n')
