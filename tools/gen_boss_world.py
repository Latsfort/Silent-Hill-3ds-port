"""Adapt original boss-door room visibility, world flags and outside-room reset."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed
from gen_bathroom_props import BOSS_OBJECTS

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_boss_world.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s02/map1_s02.c').read_text(encoding='utf-8'))
    body = function(source, 'Map_WorldObjectsUpdate')
    begin = body.index('if (PLAYER_IN_MAP_CHUNK(vx, 1, 2, -1, 2) && PLAYER_IN_MAP_CHUNK(vz, 1, -2, -1, -2))')
    end = body.index('if (PLAYER_IN_MAP_CHUNK(vx, 1, 4, -1, 4)', begin)
    code = body[begin:end]
    room = re.search(r'if \(([^\n]+)\)', code)[1]
    room = room.replace('PLAYER_IN_MAP_CHUNK(vx,', 'in_chunk(SH_Q12(x),').replace('PLAYER_IN_MAP_CHUNK(vz,', 'in_chunk(SH_Q12(-z),')
    objects = re.findall(r'WorldObjects_Add\(&g_WorldObject_(\w+).object, [^;]+\);', code)
    assert tuple(objects) == BOSS_OBJECTS
    code = re.sub(r'WorldObjects_Add\([^;]+\);', '', code)
    code = code.replace(re.search(r'if \(([^\n]+)\)', code)[1], 'boss_room(x, z)', 1)
    # Capture the original two-line condition using its following block.
    condition = code[code.index('if (g_WorldObject_Door2Hi'):code.index('\n        {', code.index('if (g_WorldObject_Door2Hi'))]
    condition = condition[len('if ('):-1]
    code = re.sub(r'Q12_ANGLE\(([-\d.]+)f\)', lambda m: str(fixed(m[1], True)), code)
    condition = re.sub(r'Q12_ANGLE\(([-\d.]+)f\)', lambda m: str(fixed(m[1], True)), condition)
    for old, new in [('g_WorldObject_Door2Hi.rotation.vy', 'doors->rotation[0]'), ('g_WorldObject_Door1Hi.rotation.vy', 'doors->rotation[1]')]:
        code = code.replace(old, new); condition = condition.replace(old, new)
    code = re.sub(r'Savegame_EventFlag(Set|Clear|Get)\((\w+)\)',
        lambda m: f'sh_save_event_{m[1].lower()}(save, {g.value(m[2])})', code)
    code = re.sub(r'CollisionTriggerFlag_\w+', lambda m: str(g.value(m[0])), code)
    assert not re.search(r'\b(?:g_|WorldObjects_|Savegame_|PLAYER_|Q12_)\w*', code), code
    mask = ' | '.join(f's_school_bathroom_props[{i + 2}].mask' for i in range(len(objects)))
    return '\n'.join(['/* Generated from MAP1_S02 boss-door world update. */',
        'static bool boss_room(float x, float z)', '{', f'    return {room};', '}',
        'static uint32_t boss_prop_mask(float x, float z)', '{', f'    return boss_room(x, z) ? ({mask}) : 0;', '}',
        'static int boss_collision_flags(const ShBossDoors *doors)', '{',
        f'    return {g.value("CollisionTriggerFlag_Map")} | {g.value("CollisionTriggerFlag_Objects")} |',
        f'        (({condition}) ? {g.value("CollisionTriggerFlag_2")} : 0);', '}',
        'static void boss_world_update(ShBossDoors *doors, ShSavegame *save, float x, float z)', '{',
        '    int collFlags = 0;', code.rstrip(), '    (void)collFlags;', '}', ''])


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original boss props, room gate, door reset, flag362 and collision decision')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
