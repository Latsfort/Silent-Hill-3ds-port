"""Adapt the original bathroom door motion and camera tweens, in call order."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/bathroom_motion_steps.h'


def arguments(text):
    depth, begin, result = 0, 0, []
    for i, char in enumerate(text):
        if char == '(': depth += 1
        elif char == ')': depth -= 1
        elif char == ',' and depth == 0:
            result.append(text[begin:i].strip())
            begin = i + 1
    result.append(text[begin:].strip())
    assert depth == 0
    return result


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s02/map1_s02.c').read_text(encoding='utf-8'))
    body = function(source, 'func_800DBB7C')
    code = body[body.index('case 3:') + len('case 3:'):body.index('default:')]
    code = code[:code.rfind('break;')]
    code = re.sub(r'Q12(_ANGLE)?\(([-\d.]+)f?\)', lambda m: str(fixed(m[2], bool(m[1]))), code)
    code = code.replace('D_800E1EE8', 'motion->elapsed').replace('g_DeltaTime', 'delta')
    code = code.replace('g_WorldObject_DoorHid.rotation.vy', 'motion->rotation')
    code = code.replace('Q12_MULT_PRECISE', 'sh_q12_mul')
    code = code.replace('SysWork_StateStepIncrement(0)', '(frame->finished = true)')
    code = re.sub(r'Event_TweenLinear\(([-\d]+), (\d+), (\d+)\)', r'bathroom_tween(motion, \1, \2, \3, delta)', code)
    def camera(m):
        args = arguments(m[2])
        assert len(args) == 9 and args[0] == 'NULL' and args[-1] == 'false'
        look = m[1] == 'LookAt'
        point = 'look' if look else 'position'
        values = ', '.join(args[1:4])
        params = 'watch' if look else 'camera'
        result = f'frame->{point} = (ShPathPoint){{{values}}};\n'
        result += '\n'.join(f'                frame->params.{params}[{i}] = {value} / 4096.0f;' for i, value in enumerate(args[4:8]))
        return result + '\n                frame->camera = true;'
    code = re.sub(r'Event_Camera(Position|LookAt)Set\(([^;]+)\);', camera, code)
    assert not re.search(r'\b(?:Event_|SysWork_|g_|D_|Q12_)\w*', code), code
    return '\n'.join(['/* Generated from map1_s02 func_800DBB7C case3. */',
        'void sh_bathroom_motion_step(ShBathroomMotion *motion, int32_t delta, ShBathroomFrame *frame)', '{',
        '    int16_t angleAdd;', '    memset(frame, 0, sizeof(*frame));', code.rstrip(), '}', ''])


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original bathroom door motion, camera targets, parameters and tween call order')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
