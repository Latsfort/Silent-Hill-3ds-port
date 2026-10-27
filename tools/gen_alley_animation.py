"""Extract the original MAP0_S00 torch animation through the verified overlay mapping."""
from pathlib import Path
import sys
from gen_roof_animation import emit

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/alley_animation_data.h'

if __name__ == '__main__':
    data = emit('map0_s00', (84,), 'alley')
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original torch state, animation mapping, blend/playback and packaged ANM')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
