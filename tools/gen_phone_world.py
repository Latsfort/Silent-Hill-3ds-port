"""Adapt the two original phone models' world visibility branch."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_bathroom_props import BOSS_OBJECTS, PHONE_OBJECTS

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_phone_world.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s02/map1_s02.c').read_text(encoding='utf-8'))
    body = function(source, 'Map_WorldObjectsUpdate')
    begin = body.index('if (PLAYER_IN_MAP_CHUNK(vx, 0, 0, -1, 1) && PLAYER_IN_MAP_CHUNK(vz, 1, 4, -1, 4))')
    end = body.index('if (PLAYER_IN_MAP_CHUNK(vx, 1, 2, -1, 2)', begin)
    code = body[begin:end]
    calls = re.findall(r'WorldObjects_Add\(&g_WorldObject_(\w+).object, [^;]+\);', code)
    assert tuple(calls) == PHONE_OBJECTS
    code = re.sub(r'WorldObjects_Add\(&g_WorldObject_(\w+).object, [^;]+\);',
        lambda m: f'mask |= s_school_bathroom_props[{2 + len(BOSS_OBJECTS) + PHONE_OBJECTS.index(m[1])}].mask;', code)
    code = re.sub(r'Savegame_EventFlagGet\((\w+)\)', lambda m: f'sh_save_event_get(save, {g.value(m[1])})', code)
    code = code.replace('PLAYER_IN_MAP_CHUNK(vx,', 'in_chunk(SH_Q12(x),').replace('PLAYER_IN_MAP_CHUNK(vz,', 'in_chunk(SH_Q12(-z),')
    assert not re.search(r'\b(?:g_|Savegame_|WorldObjects_|PLAYER_)\w*', code), code
    return '\n'.join(['/* Generated from MAP1_S02 phone world visibility. */',
        'static uint32_t phone_prop_mask(const ShSavegame *save, float x, float z)', '{',
        '    uint32_t mask = 0;', code.rstrip(), '    return mask;', '}', ''])


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original phone world models, room gate and flag99 visibility')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
