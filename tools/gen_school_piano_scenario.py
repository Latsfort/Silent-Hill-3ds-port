"""Load the cumulative piano checkpoint without bypassing puzzle completion."""
import re
import gen_map_data as g
from gen_debug_checkpoints import emit, ROOT

labels = re.findall(r'^    \{"([^"]+)",', emit(), re.M)
index = labels.index('Ecole - piano')
steps = ['log 15', '60 NONE']
for _ in range(9 + index):
    steps += ['1 DOWN', '1 NONE']
steps += ['1 A', '600 NONE', 'quit']
path = ROOT / 'build/story-regressions/schoolpianoobjects.txt'
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text('\n'.join(steps) + '\n', encoding='ascii')
print(path)

from gen_school_piano import outputs
data = outputs()[ROOT / 'source/game/school_piano_data.h']
sequence = [int(v) for v in re.search(r's_piano_sequence\[\] = \{([^}]+)', data)[1].split(',')]
notes = re.findall(r'\{\d+, \d+, (-?\d+), (?:true|false)\}', data)
index = labels.index('Ecole - piano')
steps = ['log 15', '60 NONE']
for _ in range(9 + index):
    steps += ['1 DOWN', '1 NONE']
steps += ['1 A', '600 NONE', '1 A', '180 NONE', '1 A', '30 NONE', '1 A', '30 NONE', '1 A', '180 NONE']
x = 0
for key in sequence:
    target = int(notes[key]) / 4096
    frames = round(abs(target - x) / (45 * 4 / 75 * 30 / 60))
    steps += [f'{frames} ' + ('RIGHT' if target > x else 'LEFT'), '1 NONE', '1 A', '90 NONE']
    x = target
solution = list(steps)
pickup = next(e for e in g.load_events('map1_s01') if e['func'] == 'func_800D857C')
point = g.load_points('map1_s01')[pickup['point_idx']]
finish = ['900 NONE', f'warp {point["x"]:.4f} {-point["z"]:.4f} {-point["param0"] * 360 / 256:.4f}',
          '60 NONE', '1 A', '180 NONE', '1 A', '180 NONE', 'quit']
steps += finish
path = ROOT / 'build/story-regressions/schoolpiano.txt'
path.write_text('\n'.join(steps) + '\n', encoding='ascii')
print(path)

path = ROOT / 'build/story-regressions/schoolpianoskip.txt'
path.write_text('\n'.join(solution + ['120 NONE', '1 START'] + finish) + '\n', encoding='ascii')
print(path)
for cancel in (False, True):
    steps = ['log 15', '60 NONE']
    for _ in range(9 + index):
        steps += ['1 DOWN', '1 NONE']
    steps += ['1 A', '600 NONE', '1 A', '180 NONE', '1 A', '30 NONE', '1 A', '30 NONE']
    steps += ['1 A', '180 NONE'] if cancel else []
    steps += ['1 B', '300 NONE', 'quit']
    path = ROOT / ('build/story-regressions/schoolpiano' + ('cancel' if cancel else 'decline') + '.txt')
    path.write_text('\n'.join(steps) + '\n', encoding='ascii')
    print(path)

index = labels.index('Ecole - argent au sol')
steps = ['log 15', '60 NONE']
for _ in range(9 + index):
    steps += ['1 DOWN', '1 NONE']
steps += ['1 A', '600 NONE', '1 A', '180 NONE', '1 B', '180 NONE',
          '1 A', '180 NONE', '1 A', '180 NONE', 'quit']
path = ROOT / 'build/story-regressions/schoolsilver.txt'
path.write_text('\n'.join(steps) + '\n', encoding='ascii')
print(path)
