"""Exercise both original ladder entrance/exit triggers from school progression."""
import re
import gen_map_data as g
from gen_debug_checkpoints import ROOT, emit

labels = re.findall(r'^    \{"([^"]+)",', emit(), re.M)
index = labels.index('Ecole - arrivee')
events = g.load_events('map1_s00')
points = g.load_points('map1_s00')
steps = ['log 15', '60 NONE']
for _ in range(9 + index):
    steps += ['1 DOWN', '1 NONE']
steps += ['1 A', '600 NONE']
for origin in (16, 17):
    event = next(e for e in events if e['func'] == 'func_800D8948' and e['point_idx'] == origin)
    point = points[event['point_idx']]
    steps += [f'warp {point["x"]:.4f} {-point["z"]:.4f} {-point["param0"] * 360 / 256:.4f}',
              '90 NONE', '1 A', '600 NONE']
    room_event = next(e for e in events if e['required_flag'] == (79 if origin == 16 else 80) and e['sys_state'] == g.value('SysState_LoadRoom'))
    exit_point = points[room_event['event_param']]
    steps += [f'warp {exit_point["x"]:.4f} {-exit_point["z"]:.4f} {-exit_point["param0"] * 360 / 256:.4f}',
              '90 NONE', '1 A', '600 NONE']
steps += ['quit']
path = ROOT / 'build/story-regressions/schoolladders.txt'
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text('\n'.join(steps) + '\n', encoding='ascii')
print(path)
