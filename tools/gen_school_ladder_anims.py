"""Read ladder animation mapping from the verified retail school overlay."""
import hashlib
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
DECOMP = ROOT / 'vendor/silent-hill-decomp'
OUTPUT = ROOT / 'source/game/school_ladder_anims.h'


def emit():
    config = (DECOMP / 'configs/USA/maps/map1_s00.yaml').read_text(encoding='utf-8')
    raw = (ROOT / 'dump/assets/VIN/MAP1_S00.BIN').read_bytes()
    assert hashlib.sha1(raw).hexdigest() == re.search(r'^sha1: (\w+)', config, re.M)[1]
    symbols = (DECOMP / 'configs/USA/maps/sym.map1_s00.txt').read_text(encoding='utf-8')
    base = int(re.search(r'    vram: (0x[\dA-Fa-f]+)', config)[1], 16)
    address = lambda name: int(re.search(r'^' + name + r'\s*=\s*(0x[\dA-Fa-f]+)', symbols, re.M)[1], 16)
    begin, end = address('g_MapHeaderTable_38'), address('LOADABLE_INVENTORY_ITEMS')
    rows = list(struct.iter_unpack('<4h', raw[begin - base:end - base]))
    shared = (DECOMP / 'include/maps/shared/sharedFunc_800CDAA8_0_s02.h').read_text(encoding='utf-8')
    info = (DECOMP / 'src/maps/map1_s00/map1_s00_anim_info.c').read_text(encoding='utf-8')
    clips = re.findall(r'\{ Anim_PlaybackLoop, ANIM_STATUS\((\d+), true\).*?\{ Q12\((\d+)\) \}, (\d+), (\d+) \}', info)
    lines = ['/* Generated from MAP1_S00 animation mapping and original playback-loop clips. */',
             'static const ShSchoolAnimation s_school_ladder_animations[] = {']
    for state in (87, 88):
        code = shared.split(f'case PlayerState_Unk{state}:', 1)[1].split('break;', 1)[0]
        virtual, active = re.search(r'ANIM_STATUS\((\d+), (true|false)\)', code).groups()
        status, _, first, last = next(row for row in rows if row[1] == (int(virtual) * 2 + (active == 'true')))
        clip, fps, start, stop = next(row for row in clips if int(row[0]) == ((status & 255) >> 1))
        assert (first, last) == (int(start), int(stop))
        assert first == int(re.search(r'#define KEYFRAME_PlayerState_Unk' + str(state) + r' (\d+)', shared)[1])
        lines.append(f'    {{{state}, {first}, {last}, {fps}.0f}},')
    lines += ['};', '']
    assert (ROOT / 'romfs/game/anim/HB_M1S00.ANM').read_bytes() == (ROOT / 'dump/assets/ANIM/HB_M1S00.ANM').read_bytes()
    return '\n'.join(lines)


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: both ladder loops and packaged ANM match original overlay/source/assets')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
