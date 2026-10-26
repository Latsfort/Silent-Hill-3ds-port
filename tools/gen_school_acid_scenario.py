"""Exercise the acid checkpoint through inventory Use and the retail gold trigger."""
from pathlib import Path
import re

import gen_map_data as g
import gen_debug_checkpoints as checkpoints

ROOT = Path(__file__).resolve().parents[1]
data = checkpoints.emit()
labels = re.findall(r'^    \{"([^"]+)",', data, re.M)
index = labels.index('Ecole - acide en poche')
items = [int(item) for item in re.findall(r'\{(\d+), \d+, 0, 0\}',
          re.search(rf's_debug_items_{index}\[\] = \{{(.*?)\n\}};', data, re.S)[1])]
chemical = g.value('InvItemId_Chemical')
gold = next(e for e in g.load_events('map1_s01') if e['func'] == 'MapEvent_GoldMedallionItemPickup')
point = g.load_points('map1_s01')[gold['point_idx']]
for skip in (False, True):
    steps = ['log 15', '60 NONE']
    for _ in range(9 + index): steps += ['1 DOWN', '1 NONE']
    steps += ['1 A', '300 NONE', '1 SELECT', '30 NONE']
    for _ in range(items.index(chemical)): steps += ['1 RIGHT', '1 NONE']
    steps += ['1 A', '15 NONE', '1 A']
    steps += ['300 NONE', '1 START', '300 NONE'] if skip else ['900 NONE']
    steps += [f'warp {point["x"]:.4f} {-point["z"]:.4f} {-point["param0"] * 360 / 256:.4f}',
              '60 NONE', '1 A', '180 NONE', '1 A', '120 NONE', 'quit']
    name = 'schoolacidskip' if skip else 'schoolacid'
    path = ROOT / f'build/story-regressions/{name}.txt'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('\n'.join(steps) + '\n', encoding='ascii')
    print(path)
