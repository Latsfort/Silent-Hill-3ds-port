"""Extract speed and yaw calculations used by scripted walk/turn states."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/path_locomotion.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/include/maps/shared/sharedFunc_800CDAA8_0_s02.h').read_text(encoding='utf-8'))
    lines = ['/* Generated from retail scripted walk/turn speed and yaw calculations. */',
             'void sh_path_locomotion(ShPath *path, ShPathPlayer *player, int keyframe, int32_t dt)', '{',
             '    int32_t yaw_step = 0, headingAngle = 0, speed = player->move_speed;', '    switch (player->state) {']
    for name in ('Reset', 'WalkForward', 'TurnRight', 'TurnLeft'):
        body = source.split('case PlayerState_' + name + ':', 1)[1].split('#endif', 1)[0]
        body = re.sub(r'Q12_ANGLE\(([-\d.]+)f\)', lambda m: str(fixed(m[1], True)), body)
        body = re.sub(r'Q12\(([-\d.]+)f\)', lambda m: str(fixed(m[1])), body)
        body = body.replace('playerProps.moveSpeed', 'player->move_speed')
        body = body.replace('player->model.anim.keyframeIdx', 'keyframe')
        body = body.replace('g_Player_MoveSpeed', 'speed').replace('sharedData_800E39D8_0_s00', 'yaw_step')
        body = body.replace('g_Player_FlexRotationY', 'path->flex_y').replace('g_Player_CutsceneState', 'player->cutscene_state')
        body = body.replace('playerProps.headingAngle', 'player->heading_angle')
        body = body.replace('&extra->model', '&player->extra_model').replace('extra->model.controlState', 'player->extra_model.control_state')
        body = body.replace('Model_AnimStatusSet', 'path_model_status')
        body = body.replace('Player_AnimStateReset(player, extra)', 'path_animation_reset(player)')
        body = re.sub(r'Player_ExtraStateSet\(player, extra, (\w+)\)', r'sh_path_state_set(player, \1)', body)
        body = re.sub(r'\b(PlayerState_\w+|PlayerCutsceneState_\w+|HarryAnim_\w+)\b', lambda m: str(g.value(m[0])), body)
        body = body.replace('g_DeltaTime', 'dt').replace('TIMESTEP_SCALE_30_FPS', 'PATH_TIMESTEP').replace('CLAMP', 'sh_q12_clamp')
        assert not re.search(r'\b(?:g_|playerProps|Model_)\w*', body)
        lines += [f'    case {g.value("PlayerState_" + name)}:', body]
    lines += ['    default: break;', '    }',
              '    player->move_speed = speed;',
              '    player->rotation.vy = path_angle(player->rotation.vy + (yaw_step >> 4) + 4096);',
              '    player->heading_angle = path_angle(player->rotation.vy + headingAngle);', '}', '']
    return '\n'.join(line.rstrip() for line in lines)


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: source walk acceleration, turn deceleration and signed yaw steps')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
