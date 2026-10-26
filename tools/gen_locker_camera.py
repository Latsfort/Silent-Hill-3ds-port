"""Extract locker room camera selection without replacing smooth calls with cuts."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_locker_camera.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s03/map1_s03.c').read_text(encoding='utf-8'))
    body = function(source, 'Map_WorldObjectsUpdate')
    begin = body.index('if (g_SysWork.playerWork.player.position.vx <')
    end = body.index('if (!Savegame_EventFlagGet(EventFlag_53)')
    code = 'if (roof_locker_in_room(x, z)) {\n' + body[begin:end]
    code = re.sub(r'Q12\(([-\d.]+)f?\)', lambda m: str(fixed(m[1])), code)
    code = code.replace('g_SysWork.playerWork.player.position.vx', 'SH_Q12(x)')
    code = code.replace('g_SysWork.playerWork.player.position.vz', 'SH_Q12(-z)')
    code = code.replace('D_800E62D0', 'town->school.locker_camera_mode')
    code = re.sub(r'Savegame_EventFlagGet\((\w+)\)', lambda m: f'sh_save_event_get(save, {g.value(m[1])})', code)
    def camera(m):
        args = [s.strip() for s in m[2].split(',')]
        assert args[0] == 'NULL' and args[4:8] == ['0'] * 4
        return f'locker_camera_target(&target, {str(m[1] == "LookAt").lower()}, ' + ', '.join(args[1:4] + [args[8]]) + ');'
    code = re.sub(r'Event_Camera(Position|LookAt)Set\(([^;]+)\);', camera, code)
    code = code.replace('vcReturnPreAutoCamWork(town->school.locker_camera_mode)', 'locker_camera_return(town, town->school.locker_camera_mode)')
    assert not re.search(r'\b(?:Event_|Savegame_|vcReturn|g_|D_)\w*', code), code
    return '\n'.join(['/* Generated from map1_s03 Map_WorldObjectsUpdate locker camera branches. */',
        'static void locker_room_camera(ShTown *town, const ShSavegame *save, float x, float z, float dt)', '{',
        '    ShLockerCameraTarget target = {0};', code,
        '    if (target.set) { locker_camera_apply(town, &target, NULL, dt); }', '}', ''])


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original locker camera conditions, positions, flags and warp choices')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
