"""Adapt the original two-door mechanism without replacing rotation with a timer."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/boss_door_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s02/map1_s02.c').read_text(encoding='utf-8'))
    body = function(source, 'func_800DC1E0')
    code = body[body.index('case 7:') + len('case 7:'):body.index('default:')]
    code = code[:code.rfind('break;')]
    replacements = {
        'D_800E1FDC.vx': 'doors->progress[0]', 'D_800E1FDC.vy': 'doors->progress[1]',
        'D_800E1FE0': 'doors->speed', 'D_800E1FE3': 'doors->reverse',
        'g_WorldObject_Door2Hi.rotation.vy': 'doors->rotation[0]',
        'g_WorldObject_Door1Hi.rotation.vy': 'doors->rotation[1]',
        'g_DeltaTime': 'delta', 'Savegame_EventFlagGet(EventFlag_118)': 'first_valve',
        'SD_Call(Sfx_Unk1453)': '(frame->start_sound = true)',
        'Sd_SfxStop(Sfx_Unk1453)': '(frame->stop_sound = true)',
        'SysWork_StateStepSet(0, 1)': '(frame->finished = true)',
        'Q12_MULT_PRECISE': 'sh_q12_mul', 'MIN(': 'door_min(', 'MAX(': 'door_max('
    }
    for old, new in replacements.items(): code = code.replace(old, new)
    code = re.sub(r'Q12(_ANGLE)?\(([-\d.]+)f?\)', lambda m: str(fixed(m[2], bool(m[1]))), code)
    code = re.sub(r'Sfx_WithFalloffAndPitchPlay\(Sfx_Unk1453, &D_800CB94C, ([^,]+), \d+, 0\)', r'(frame->volume = \1)', code)
    assert not re.search(r'\b(?:Event_|SysWork_|Savegame_|Sfx_|Sd_|SD_|g_|D_|Q12_)\w*', code), code
    return '\n'.join(['/* Generated from map1_s02 func_800DC1E0 case7. */',
        'void sh_boss_doors_step(ShBossDoors *doors, int32_t delta, bool first_valve, ShBossDoorFrame *frame)', '{',
        '    int32_t rotY0, rotY1;', '    memset(frame, 0, sizeof(*frame));', code.rstrip(), '}', ''])


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original boss door acceleration, four direction/valve branches, clamps and quarter-turn snap')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
