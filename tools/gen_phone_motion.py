"""Extract original phone path and every camera command, including live tweens."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed
from gen_bathroom_motion import arguments

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/phone_motion_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s02/map1_s02.c').read_text(encoding='utf-8'))
    body = function(source, 'MapEvent_CutscenePhoneCall')
    waypoint = re.search(r'Event_PathWaypointSet\(([^;]+)\);', body)[1]
    args = arguments(waypoint)
    assert args[:3] == ['true', '0', '0']
    angle = fixed(re.search(r'Q12_ANGLE\(([-\d.]+)f\)', args[3])[1], True)
    point = [fixed(re.search(r'Q12\(([-\d.]+)f\)', v)[1]) for v in args[4:]]
    execution = set(re.findall(r'Event_PathWaypointExecutePlayer\((\d+), (\d+), false\)', body))
    assert len(execution) == 1
    state, count = next(iter(execution))
    lines = ['/* Generated from MAP1_S02 MapEvent_CutscenePhoneCall path/camera calls. */',
        'void sh_phone_motion_begin(ShPhoneMotion *motion, const ShPathPlayer *player)', '{',
        '    memset(motion, 0, sizeof(*motion));',
        f'    motion->waypoint = (ShPathPoint){{{point[0]}, player->position.vy, {point[1]}}};',
        f'    motion->heading = {angle};', '}',
        'bool sh_phone_path_step(ShPhoneMotion *motion, ShPathPlayer *player, int32_t delta)', '{',
        f'    return sh_path_player_step(&motion->path, player, {state}, &motion->waypoint, motion->heading, {count}, delta);', '}',
        'bool sh_phone_camera_view(ShPhoneMotion *motion, const ShPathPlayer *player, int step, int32_t delta, ShPhoneView *view)', '{',
        '    switch (step) {']
    for step in (5, 9, 15, 16, 28):
        block = body[body.index(f'case {step}:'):]
        block = block[:re.search(r'\n\s*(?:case \d+|default):', block[len(f'case {step}:'):]).start() + len(f'case {step}:')]
        calls = re.findall(r'Event_Camera(Position|LookAt)Set\(([^;]+)\);', block)
        assert [v[0] for v in calls] == ['Position', 'LookAt']
        lines.append(f'    case {step}:')
        for kind, command in calls:
            camera = arguments(command)
            assert len(camera) == 9 and camera[0] == 'NULL' and all(re.fullmatch(r'Q12\(0.0f\)', v) for v in camera[4:8])
            position = ', '.join(camera[1:4])
            position = re.sub(r'Q12\(([-\d.]+)f\)', lambda m: str(fixed(m[1])), position)
            position = position.replace('playerChara.', 'player->position.')
            position = re.sub(r'Event_TweenLinear\(([-\d]+), (\d+), ([03])\)',
                lambda m: f'phone_tween(motion, {0 if m[3] == "0" else 1}, {m[1]}, {m[2]}, delta)', position)
            member = 'position' if kind == 'Position' else 'look'
            warp = 'warp_position' if kind == 'Position' else 'warp_look'
            lines += [f'        view->{member} = (ShPathPoint){{{position}}};', f'        view->{warp} = {camera[8]};']
        lines.append('        return true;')
    lines += ['    default: return false;', '    }', '}', '']
    output = '\n'.join(lines)
    assert not re.search(r'\bEvent_\w+', output) and 'playerChara' not in output
    return output


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original phone waypoint/heading and all five camera command pairs with source tween order')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
