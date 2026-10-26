"""Extract acid scene data and the retail border state/geometry."""
from pathlib import Path
import re
import struct
import sys

import gen_map_data as g
from gen_school_smoke import function

ROOT = Path(__file__).resolve().parents[1]
DECOMP = ROOT / 'vendor/silent-hill-decomp'


def outputs():
    source = (DECOMP / 'src/maps/map1_s01/map1_s01.c').read_text(encoding='utf-8')
    acid = function(source, 'func_800D7308')
    def q12(value):
        value = struct.unpack('<f', struct.pack('<f', float(value.rstrip('f'))))[0]
        return int(value * 4096) / 4096
    def position(name):
        call = re.search(name + r'\(NULL,(.*?)true\)', acid, re.S)[1]
        values = re.findall(r'Q12\(([^)]+)\)', call)[:3]
        assert len(values) == 3
        return [q12(v) for v in values]
    camera, look = position('Event_CameraPositionSet'), position('Event_CameraLookAtSet')
    warp = [q12(re.search(r'playerChara.position.v' + axis + r' = Q12\(([^)]+)\)', acid)[1]) for axis in ('x', 'z')]
    angle = float(re.search(r'playerChara.rotation.vy = Q12_ANGLE\(([-\d.]+)f\)', acid)[1])
    yaw = -int(angle / 360 * 4096) / 4096 * 6.283185307179586
    sound, point, volume = re.search(r'Sfx_WithFlagsPlay\((\w+), &Q12_VECTOR3\(([^)]+)\), Q8\(([^)]+)\)', acid).groups()
    sound_pos = [q12(v.strip()) for v in point.split(',')]
    def vector(values):
        return '{' + ', '.join(f'{v:.8f}f' for v in values) + '}'
    def converted(values):
        return [values[0], -values[1], -values[2]]
    waits = [g.value('Q12(' + v + ')') / 4096 for v in re.findall(r'Event_WaitTimer\(Q12\(([^)]+)\)', acid)]
    assert len(waits) == 2
    event = next(e for e in g.load_events('map1_s01') if e['func'] == 'func_800D7308')
    animation = int(re.search(r'CharaAnimCmd_SetState, &playerChara, (\d+)', acid)[1])
    flag = g.value(re.search(r'Savegame_EventFlagSet\((\w+)\)', acid)[1])
    assert flag == event['complete_flag'] and not event['transition_flags']
    data = ['/* Generated from map1_s01.c func_800D7308. Positions are 3DS metres. */',
            'static const ShSchoolAcid s_school_acid = {',
            f'    .callback = {event["event_param"]}, .item = {event["required_item"]}, .flag = {flag},',
            f'    .animation = {animation}, .sound = {g.value(sound)},',
            f'    .player = {vector([warp[0], -warp[1]])}, .yaw = {yaw:.8f}f,',
            f'    .camera = {vector(converted(camera))}, .look = {vector(converted(look))},',
            f'    .sound_pos = {vector(converted(sound_pos))}, .volume = {int(float(volume.rstrip("f")) * 256) / 256:.8f}f,',
            f'    .animation_wait = {waits[0]:.8f}f, .smoke_wait = {waits[1]:.8f}f,', '};', '']
    states = ('Reset', 'None', 'Active', 'FadeInStart', 'FadingIn', 'ForceShow', 'Shown', 'FadeOutStart', 'FadingOut')
    constants = ['/* Generated from cutscene_border.h. */', '#pragma once']
    for state in states:
        constants.append(f'#define SH_BORDER_{state.upper()} {g.value("CutsceneBorderState_" + state)}')
    border = (DECOMP / 'src/bodyprog/screen/cutscene_border.c').read_text(encoding='utf-8')
    records = g.records(g.strip_comments(border.split('static POLY_G4 D_800A8EB0[] = {', 1)[1].split('};', 1)[0]))
    rectangles = ['/* Generated: the two POLY_G4 added by Screen_CutsceneCameraStateUpdate. */',
                  'static const ShBorderRect s_border_rects[] = {']
    for index in (0, 2):
        values = [g.value(value) for value in records[index].split(',') if value.strip()]
        assert len(values) == 25
        x0, y0, x1, y1, x2, y2 = [values[i] for i in (5, 6, 11, 12, 17, 18)]
        assert y0 == y1 and x0 == x2
        rectangles.append(f'    {{{x0}, {y0}, {x1 - x0}, {y2 - y0}}},')
    rectangles += ['};', '']
    return {ROOT / 'source/game/school_acid_data.h': '\n'.join(data),
            ROOT / 'include/sh3ds/border_data.h': '\n'.join(constants + ['']),
            ROOT / 'source/game/border_rect_data.h': '\n'.join(rectangles)}


if __name__ == '__main__':
    for path, text in outputs().items():
        if '--check' in sys.argv: assert path.read_text(encoding='ascii') == text, path
        else: path.write_text(text, encoding='ascii', newline='\n')
    if '--check' in sys.argv: print('PASS: acid scene camera, warp, timing, item/flags/sound and border geometry match retail')
