"""Adapt the bathroom door cutscene around its original generated motion."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_locker_body import adapt

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_bathroom_scene.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s02/map1_s02.c').read_text(encoding='utf-8'))
    name = 'func_800DBB7C'
    body = function(source, name)
    code = body[body.index('switch ('):]
    begin, end = code.index('case 3:'), code.index('default:')
    code = code[:begin] + '''case 3:
            {
                ShBathroomFrame frame;
                sh_bathroom_motion_step(&town->school.bathroom, SH_Q12(dt), &frame);
                if (frame.camera) { bathroom_camera_frame(town, &frame, dt); }
                if (frame.finished) { step_set(town, town->step + 1); }
            }
            break;

        ''' + code[end:]
    code = re.sub(r'Event_TweenReset\((\d+)\)', r'(town->school.bathroom.tween[\1] = 0)', code)
    code = code.replace('Event_WaitPlayerStop()', 'step_set(town, town->step + 1)')
    code = code.replace('Model_AnimFlagsClear(&g_SysWork.playerWork.player.model, 2)', '(town->school.player_hidden = true)')
    code = code.replace('Model_AnimFlagsSet(&g_SysWork.playerWork.player.model, 2)', '(town->school.player_hidden = false)')
    code = code.replace('Event_ScreenFadeCmd(ScreenFadeCmd_Start, true, 2, Q12(0.0f), false)', '(town->border.state = SH_BORDER_FADEINSTART)')
    code = code.replace('Event_ScreenFadeCmd(ScreenFadeCmd_Start, false, 2, Q12(0.0f), false)', '(town->border.state = SH_BORDER_FADEOUTSTART)')
    code = code.replace('vcReturnPreAutoCamWork(true)', '(town->cam_active = town->script_use_cam = false)')
    code = re.sub(r'SD_Call\((\w+)\)', lambda m: f'sh_snd_call({g.value(m[1])})', code)
    code = adapt(code)
    return '\n'.join(['/* Generated from map1_s02 func_800DBB7C. */',
        f'#define SH_BATHROOM_CALLBACK {g.event_funcs("map1_s02").index(name)}',
        'static void bathroom_camera_frame(ShTown *town, const ShBathroomFrame *frame, float dt);',
        'static void bathroom_scene(ShTown *town, ShSavegame *save, float dt)', '{', code, '}', ''])


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original bathroom cutscene cases, sound, camera, visibility, borders and flags')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
