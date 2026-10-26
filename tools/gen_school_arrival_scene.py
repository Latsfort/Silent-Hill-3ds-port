"""Adapt both original MAP1_S02 arrival switches and verify ROM voice commands."""
from pathlib import Path
import re
import struct
import hashlib
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed
from gen_bathroom_motion import arguments
from gen_locker_body import adapt

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_arrival_scene.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s02/map1_s02.c').read_text())
    code = function(source, 'func_800DA384')
    config = (ROOT / 'vendor/silent-hill-decomp/configs/USA/maps/map1_s02.yaml').read_text()
    raw = (ROOT / 'dump/assets/VIN/MAP1_S02.BIN').read_bytes()
    assert hashlib.sha1(raw).hexdigest() == re.search(r'^sha1: (\w+)', config, re.M)[1]
    base = int(re.search(r'    vram: (0x[\dA-Fa-f]+)', config)[1], 16)
    voices = struct.unpack_from('<3H', raw, 0x800E1EDC - base)
    camera_stage = struct.unpack_from('<i', raw, 0x800E1EE4 - base)[0]
    assert camera_stage == 0
    vc = (ROOT / 'vendor/silent-hill-decomp/src/bodyprog/view/vc_main.c').read_text()
    defaults = {}
    for name in ('cam_mv_prm_user', 'deflt_watch_mv_prm'):
        text = vc.split(name + ' = {', 1)[1].split('};', 1)[0]
        values = [fixed(value) for value in re.findall(r'Q12\(([-\d.]+)f\)', text)]
        assert len(values) == 4
        defaults[name] = values

    camera_functions = []
    def cameras(m):
        pos, look = arguments(m[1]), arguments(m[2])
        assert pos[0] == look[0] == 'NULL' and pos[-1] == look[-1]
        def coordinate(text):
            return re.sub(r'Q12\(([-\d.]+)f\)', lambda n: str(fixed(n[1])), text)
        def params(args, name):
            result = []
            for index, text in enumerate(args[4:8]):
                if text == 'Q12(0.0f)': result.append(0)
                else:
                    assert re.fullmatch(r'DIV_FAST\(' + name + r'\.\w+, 8\)', text), text
                    result.append(defaults[name][index] // 8)
            return result
        number = len(camera_functions)
        camera_functions.append((list(map(coordinate, pos[1:4])), list(map(coordinate, look[1:4])),
            params(pos, 'cam_mv_prm_user'), params(look, 'deflt_watch_mv_prm'), pos[-1]))
        return f'arrival_camera(town, {number}, dt);'
    code = re.sub(r'Event_CameraPositionSet\(([^;]+)\);\s*Event_CameraLookAtSet\(([^;]+)\);', cameras, code)
    assert len(camera_functions) == 3
    code = code.replace('VECTOR3 viewPos;', 'ShPathPoint viewPos;')
    code = code.replace('D_800E1EE4', 'town->school.arrival_camera_stage')
    code = code.replace('D_800E1EE2', 'town->school.arrival_audio_index')
    code = code.replace('vwGetViewPosition(&viewPos)', 'arrival_view_position(town, &viewPos)')
    code = code.replace('ABS(', 'abs(')
    code = code.replace('(g_Controller0->buttonFlags.clicked & g_GameWorkPtr->config.controllerConfig.skip)', 'sh_input_down(input, KEY_START)')
    code = code.replace('SysWork_StateStepReset()', 'step_set(town, -1)')
    code = code.replace('Player_ControlFreeze()', 'phone_freeze(town)')
    code = code.replace('Player_ControlUnfreeze(true)', 'arrival_unfreeze(town, true)')
    code = code.replace('Player_ControlUnfreeze(false)', 'arrival_unfreeze(town, false)')
    code = code.replace('Game_TurnFlashlightOn()', 'sh_save_flashlight_set(save, true)')
    code = code.replace('Particle_EnvironmentSet(6, 127)', '(town->school.particle_mode = 6)')
    code = code.replace('Particle_SystemUpdate(0, g_SavegamePtr->mapIdx, 0)', '(town->school.particle_reset = true)')
    code = code.replace('Event_ScreenFadeCmd(ScreenFadeCmd_Auto, true, 2, Q12(0.0f), false)', 'locker_border_in(town)')
    code = code.replace('Event_ScreenFadeCmd(ScreenFadeCmd_Start, false, 2, Q12(0.0f), false)', '(town->border.state = SH_BORDER_FADEOUTSTART)')
    code = code.replace('Event_WaitPlayerStop()', 'phone_wait_stop(town, dt)')
    code = code.replace('Event_PathWaypointExecutePlayer(53, 1, false)', 'phone_path(town, dt)')
    waypoint = arguments(re.search(r'Event_PathWaypointSet\(([^;]+)\)', code)[1])
    assert waypoint[:3] == ['true', '0', '0']
    code = re.sub(r'Event_PathWaypointSet\([^;]+\)',
        'arrival_path_begin(town)', code)
    code = re.sub(r'Event_DisplayMapMsgWithAudio\((\d+), &town->school.arrival_audio_index, &D_800E1EDC\)',
        r'arrival_message(town, input, \1, dt)', code)
    code = code.replace('Event_CharaAnimPlayToEnd(&g_SysWork.playerWork.player, 74)', 'arrival_animation(town)')
    code = code.replace('SysWork_StateSetNext(SysState_Gameplay)', 'boss_finish(town)')
    code = code.replace('vcReturnPreAutoCamWork(true)', 'arrival_camera_release(town, true)')
    code = code.replace('vcReturnPreAutoCamWork(false)', 'arrival_camera_release(town, false)')
    code = code.replace('func_8003A16C()', 'arrival_autosave(town, save)')
    code = re.sub(r'Savegame_EventFlagClear\((\w+)\)', lambda m: f'sh_save_event_clear(save, {g.value(m[1])})', code)
    code = code.replace('playerChara.', 'town->school.card_player.position.')
    code = adapt(code)
    code = code.replace('arrival_path_begin(town);', 'arrival_path_begin(town);\n            card_warp(town);')
    assert not re.search(r'\b(?:g_|D_|Event_|Savegame_|Player_|Particle_)\w*', code), code
    data = ['/* Generated from func_800DA384 and SHA1-verified MAP1_S02 voice table. */',
        f'#define SH_ARRIVAL_CALLBACK {g.event_funcs("map1_s02").index("func_800DA384")}',
        'static const uint16_t s_arrival_voices[] = {' + ', '.join(map(str, voices)) + '};',
        'static void arrival_path_begin(ShTown *town)', '{',
        '    memset(&town->school.phone.motion, 0, sizeof(town->school.phone.motion));',
        f'    town->school.phone.motion.heading = {fixed(re.search(r"Q12_ANGLE\(([-\d.]+)f\)", waypoint[3])[1], True)};',
        '    town->school.phone.motion.waypoint = (ShPathPoint){' + str(fixed(re.search(r'Q12\(([-\d.]+)f\)', waypoint[4])[1])) +
        ', town->school.card_player.position.vy, ' + str(fixed(re.search(r'Q12\(([-\d.]+)f\)', waypoint[5])[1])) + '};', '}',
        'static void arrival_camera(ShTown *town, int index, float dt)', '{', '    switch (index) {']
    for i, (pos, look, camera, watch, warp) in enumerate(camera_functions):
        data += [f'    case {i}: {{',
            '        const float position[3] = {' + ', '.join(f'{int(v) / 4096:.12f}f' for v in pos) + '};',
            '        const float look[3] = {' + ', '.join(f'{int(v) / 4096:.12f}f' for v in look) + '};',
            '        const ShVcScriptParams params = {{' + ', '.join(f'{v / 4096:.12f}f' for v in camera) + '}, {' + ', '.join(f'{v / 4096:.12f}f' for v in watch) + '}};',
            f'        sh_vc_script_update(&town->vc, position, look, &params, {warp}, {warp}, dt, &town->cam);',
            '        town->cam_active = town->script_use_cam = true; break;', '    }']
    data += ['    }', '}', 'static void arrival_message(ShTown *town, const ShInputState *input, int index, float dt)', '{',
        '    school_timed_message(town, input, index, dt, true, s_arrival_voices, sizeof(s_arrival_voices) / sizeof(s_arrival_voices[0]));', '}',
        'static void arrival_scene(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)', '{', code, '}', '']
    return '\n'.join(line.rstrip() for line in '\n'.join(data).split('\n'))


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: both retail arrival switches, path/cameras, three ROM voices, messages, flags and skip branches')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
