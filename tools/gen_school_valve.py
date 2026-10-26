"""Adapt the original roof valve switch and its hand/sound update blocks."""
from pathlib import Path
import re
import sys
import hashlib
import struct
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_roof_valve.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s03/map1_s03.c').read_text(encoding='utf-8'))
    body = function(source, 'MapEvent_DrainageValveInteract')
    code = body[body.index('switch ('):body.index('if (g_SysWork.sysStateSteps[0] == 7 ||')].rstrip()
    first = code.index('sharedData_800E30C8_1_s02.field_0[0]')
    last = code.index('g_SysWork.playerWork.player.position.vx', first)
    code = code[:first] + 'sh_roof_water_prepare(&s_roof_water, sh_save_event_get(save, 111));\n\n            ' + code[last:]
    first, last = code.index('case 9:'), code.index('case 10:')
    code = code[:first] + '''case 9:
            town->school.drain_ticks += SH_Q12(dt);
            if (!sh_drain_camera_sample(town->school.drain_ticks, sh_save_event_get(save, 111), &position, &look)) {
                step_set(town, town->step + 1);
            } else {
                ladder_camera(town, false, position.vx, position.vy, position.vz);
                ladder_camera(town, true, look.vx, look.vy, look.vz);
                sh_drain_camera_light(position, look, &town->school.drain_light);
                town->school.drain_light_override = true;
            }
            break;

        ''' + code[last:]
    code = re.sub(r'Q12(_ANGLE)?\(([-\d.]+)f?\)', lambda m: str(fixed(m[2], bool(m[1]))), code)
    code = re.sub(r'\bEventFlag_\w+\b', lambda m: str(g.value(m[0])), code)
    code = re.sub(r'Savegame_EventFlagSet\((\d+)\)', r'sh_save_event_set(save, \1)', code)
    code = re.sub(r'Savegame_EventFlagGet\((\d+)\)', r'sh_save_event_get(save, \1)', code)
    for old, new in [('g_SysWork.sysStateSteps[0]', 'town->step'),
        ('D_800E20FC', 'town->school.valve.opening'), ('D_800E2100', 'town->school.valve.running'),
        ('D_800E2101', 'town->school.valve.sound_phase'), ('D_800E20FE', 'town->school.valve_animation_calls'),
        ('D_800E20FA', 'town->school.drain_ticks'),
        ('g_SysWork.playerWork.player.position.', 'town->school.card_player.position.'),
        ('g_SysWork.playerWork.player.rotation.vy', 'town->school.card_player.rotation.vy')]: code = code.replace(old, new)
    subs = {'Player_ControlFreeze()': '(town->controls_locked = true)',
        'Player_ControlUnfreeze(false)': 'ladder_unfreeze(town)',
        'SysWork_StateSetNext(SysState_Gameplay)': 'finish(town)',
        'SysWork_StateStepIncrement(0)': 'step_set(town, town->step + 1)',
        'Event_WaitPlayerStop()': 'step_set(town, town->step + 1)',
        'func_8003D03C()': '(town->school.empty_hand = true)',
        'Player_EmptyWeaponHandSet()': '(town->school.empty_hand = true)',
        'Player_WeaponAttackRestore()': '(town->school.empty_hand = false)',
        'vcReturnPreAutoCamWork(true)': '(town->cam_active = town->script_use_cam = false)',
        'func_8008D438()': '(town->school.drain_flare_suppressed = true)',
        'func_8008D448()': '(town->school.drain_flare_suppressed = false)',
        'Game_FlashlightAttributesFix()': '(town->school.drain_light_override = false)',
        'Model_AnimFlagsClear(&g_SysWork.playerWork.player.model, AnimFlag_Visible)': '(town->school.player_hidden = true)',
        'Model_AnimFlagsSet(&g_SysWork.playerWork.player.model, AnimFlag_Visible)': '(town->school.player_hidden = false)',
        'sharedFunc_800CBC98_1_s02()': 'sh_water_pool_begin(&s_roof_water.pool, &s_roof_water.config)',
        'sharedFunc_800CBD58_1_s02()': 'sh_roof_water_stop(&s_roof_water)',
        'Event_ScreenFadeCmd(ScreenFadeCmd_Start, true, 2, 0, false)': '(town->border.state = SH_BORDER_FADEINSTART)',
        'Event_ScreenFadeCmd(ScreenFadeCmd_Start, false, 2, 0, false)': '(town->border.state = SH_BORDER_FADEOUTSTART)'}
    for old, new in subs.items(): code = code.replace(old, new)
    code = re.sub(r'func_800CE0CC\((\d+)\)', r'sh_water_spray_begin(&s_roof_water.pool, s_roof_water.spray, \1)', code)
    code = re.sub(r'Event_WaitTimer\((\d+), false\)', r'ladder_wait(town, dt, \1)', code)
    code = re.sub(r'Event_CharaAnimPlayToEnd\(&g_SysWork.playerWork.player, (\d+)\)', r'card_animation(town, \1, dt)', code)
    code = code.replace('Event_DisplayMapMsg(true, 40, 4, 3, 0, false)', 'valve_prompt(town, input, 40, 4, 3)')
    code = code.replace('Event_DisplayMapMsg(false, 41, false, false, 0, false)', 'card_message(town, input, 41)')
    for axis in ('vx', 'vy', 'vz'):
        code = code.replace(f'D_800E1F88[0].{axis}', f'initial_position.{axis}').replace(f'D_800E1FC4[0].{axis}', f'initial_look.{axis}')
    def camera(match):
        args = [arg.strip() for arg in match[2].split(',')]
        assert args[0] == 'NULL' and args[4:8] == ['0'] * 4 and args[8] == 'true'
        return f'ladder_camera(town, {str(match[1] == "LookAt").lower()}, ' + ', '.join(args[1:4]) + ');'
    code = re.sub(r'Event_Camera(Position|LookAt)Set\(([^;]+)\);', camera, code)
    code = re.sub(r'(town->school.card_player.rotation.vy = -?\d+;)', r'\1\n            card_warp(town);\n            town->school.valve_active = true;', code)
    code = re.sub(r'(step_set\(town, town->step \+ 1\);)(\s+case \d+:)', r'\1\n        /* fall through */\2', code)
    assert not re.search(r'\b(?:Event_|Player_|SysWork_|Model_|sharedFunc_|func_|g_|D_)\w*', code), code
    config = (ROOT / 'vendor/silent-hill-decomp/configs/USA/maps/map1_s03.yaml').read_text(encoding='utf-8')
    raw = (ROOT / 'dump/assets/VIN/MAP1_S03.BIN').read_bytes()
    assert hashlib.sha1(raw).hexdigest() == re.search(r'^sha1: (\w+)', config, re.M)[1]
    base = int(re.search(r'    vram: (0x[\dA-Fa-f]+)', config)[1], 16)
    sound_point = re.search(r'Sfx_WithFlagsPlay\(Sfx_Unk1463, &(D_[\dA-F]+)', body)[1]
    point = struct.unpack_from('<3i', raw, int(sound_point[2:], 16) - base)
    native_point = (point[0] / 4096, -point[1] / 4096, -point[2] / 4096)
    balance_point = re.search(r'Vc_StereoBalanceGet\(&(D_[\dA-F]+)\)', body)[1]
    point2 = struct.unpack_from('<3i', raw, int(balance_point[2:], 16) - base)
    native_balance_point = (point2[0] / 4096, -point2[1] / 4096, -point2[2] / 4096)
    audio = ['static void roof_valve_hand_update(ShTown *town)', '{',
        '    if ((town->step != 7 && town->step != 12) || !town->school.hand_world_valid) { return; }',
        '    int first;',
        '    if (!sh_path_animation_first(&town->school.card_player, &first)) { return; }',
        '    const int frame = town->school.valve_animation_calls > 2 ? (town->school.card_player.model.time >> 12) - first : 0;',
        '    const int angle = sh_drain_valve_hand_angle(&town->school.hand_world);',
        '    const unsigned sounds = sh_drain_valve_hand(&town->school.valve, town->step, angle, frame);',
        '    static const float position[3] = {' + ', '.join(f'{v:.12f}f' for v in native_point) + '};',
        f'    if (sounds & SH_DRAIN_VALVE_SQUEAK) {{ sh_snd_at({g.value("Sfx_Unk1463")}, position, 0.5f); }}',
        f'    if (sounds & SH_DRAIN_VALVE_CLICK) {{ sh_snd_at({g.value("Sfx_Unk1462")}, position, 0.5f); }}',
        f'    if (sounds & SH_DRAIN_WATER_STOP) {{ sh_snd_stop({g.value("Sfx_Unk1465")}); }}',
        f'    if (sounds & SH_DRAIN_WATER_START) {{ sh_snd_call({g.value("Sfx_Unk1465")}); }}', '}', '']
    audio += ['static void roof_valve_loop_update(ShTown *town)', '{',
        '    if (!town->school.valve.running) { return; }',
        '    const ShPathPoint camera = {SH_Q12(town->cam.pos[0]), -SH_Q12(town->cam.pos[1]), -SH_Q12(town->cam.pos[2])};',
        '    const int32_t distance = sh_drain_water_distance(town->school.player_position, camera);',
        '    static const float position[3] = {' + ', '.join(f'{v:.12f}f' for v in native_balance_point) + '};',
        '    const int balance = sh_snd_camera_balance(&town->cam, position) >> 1;',
        f'    sh_snd_update({g.value("Sfx_Unk1465")}, balance, sh_drain_water_attenuation(distance, town->school.valve.opening), 0);',
        '}', '']
    return '\n'.join(['/* Generated by tools/gen_school_valve.py from the original roof valve event. */',
        f'#define SH_SCHOOL_ROOF_VALVE_CALLBACK {g.event_funcs("map1_s03").index("MapEvent_DrainageValveInteract")}',
        'static void roof_valve_steps(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)', '{',
        '    ShPathPoint position, look, initial_position, initial_look;',
        '    sh_drain_camera_sample(0, false, &initial_position, &initial_look);', code, '}', ''] + audio)


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original valve switch cases, branches, messages, cameras and flags')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
