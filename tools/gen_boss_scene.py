"""Adapt the boss-door callback around its original motion and ROM sound points."""
from pathlib import Path
import hashlib
import re
import struct
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_locker_body import adapt

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_boss_scene.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s02/map1_s02.c').read_text(encoding='utf-8'))
    name = 'func_800DC1E0'
    body = function(source, name)
    config = (ROOT / 'vendor/silent-hill-decomp/configs/USA/maps/map1_s02.yaml').read_text(encoding='utf-8')
    raw = (ROOT / 'dump/assets/VIN/MAP1_S02.BIN').read_bytes()
    assert hashlib.sha1(raw).hexdigest() == re.search(r'^sha1: (\w+)', config, re.M)[1]
    base = int(re.search(r'    vram: (0x[\dA-Fa-f]+)', config)[1], 16)
    symbols = list(dict.fromkeys(re.findall(r'Sfx_With(?:Flags|FalloffAndPitch)Play\(\w+, &(D_\w+)', body)))
    assert len(symbols) == 3
    lines = ['/* Generated from MAP1_S02 func_800DC1E0 and verified original ROM. */',
        f'#define SH_BOSS_CALLBACK {g.event_funcs("map1_s02").index(name)}',
        'static const float s_boss_sound_positions[][3] = {']
    for symbol in symbols:
        x, y, z = struct.unpack_from('<3i', raw, int(symbol[2:], 16) - base)
        lines.append('    {' + ', '.join(f'{v / 4096:.12f}f' for v in (x, -y, -z)) + '},')
    lines += ['};']
    code = body[body.index('switch ('):]
    begin, end = code.index('case 7:'), code.index('default:')
    code = code[:begin] + '''case 7:
            {
                ShBossDoorFrame frame;
                sh_boss_doors_step(&town->school.boss_doors, SH_Q12(dt), sh_save_event_get(save, 118), &frame);
                if (frame.start_sound) { sh_snd_call(SH_BOSS_LOOP_SOUND); }
                sh_snd_falloff(SH_BOSS_LOOP_SOUND, s_boss_sound_positions[2], frame.volume, SH_BOSS_LOOP_DISTANCE, 0);
                if (frame.stop_sound) { sh_snd_stop(SH_BOSS_LOOP_SOUND); }
                if (frame.finished) { step_set(town, 1); }
            }
            break;

        ''' + code[end:]
    code = code.replace('D_800E1FE2', 'town->school.boss_camera_started')
    code = code.replace('D_800E1FE3', 'town->school.boss_doors.reverse')
    code = code.replace('D_800E1FE0', 'town->school.boss_doors.speed')
    code = code.replace('D_800E1FDC.vx', 'town->school.boss_doors.progress[0]').replace('D_800E1FDC.vy', 'town->school.boss_doors.progress[1]')
    code = code.replace('Event_DisplayMapMsg(2, 41, 2, 4, NO_VALUE, false)', 'boss_choice(town, input, dt)')
    code = code.replace('SysWork_StateSetNext(SysState_Gameplay)', 'boss_finish(town)')
    code = code.replace('vcReturnPreAutoCamWork(false)', 'sh_vc_camera_release(&town->vc, false)')
    code = re.sub(r'SysWork_StateStepSet\(0, (\d+)\)', r'step_set(town, \1)', code)
    code = re.sub(r'Savegame_EventFlagClear\((\w+)\)', lambda m: f'sh_save_event_clear(save, {g.value(m[1])})', code)
    code = re.sub(r'Savegame_EventFlagGet\((\w+)\)', lambda m: f'sh_save_event_get(save, {g.value(m[1])})', code)
    code = re.sub(r'Sfx_WithFlagsPlay\((\w+), &(D_\w+), Q8\(0.5f\), SfxFlag_None\)',
        lambda m: f'sh_snd_at({g.value(m[1])}, s_boss_sound_positions[{symbols.index(m[2])}], 0.5f)', code)
    def camera(m):
        args = [s.strip() for s in m[1].split(',')]
        assert args[0] == 'NULL' and args[-1] == 'false'
        return 'boss_camera_position(town, ' + ', '.join(args[1:8]) + ')'
    code = re.sub(r'Event_CameraPositionSet\(([^;]+)\)', camera, code)
    code = adapt(code)
    code = code.replace('town->school.boss_doors.reverse = 1;\n\n        case 4:', 'town->school.boss_doors.reverse = 1;\n            /* fall through */\n\n        case 4:')
    assert 'Savegame_' not in code
    loop_distance = re.search(r'Sfx_WithFalloffAndPitchPlay\([^;]+Q12\(([-\d.]+)f\)', body)[1]
    lines += [f'#define SH_BOSS_LOOP_SOUND {g.value("Sfx_Unk1453")}', f'#define SH_BOSS_LOOP_DISTANCE {loop_distance}f',
        'static void boss_scene(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)', '{', code, '}', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original boss callback cases, ROM sound points, camera parameters, waits and flags')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
