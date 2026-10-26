"""Exercise the real stairs, statue inspection and chemical pickup with source points."""
from pathlib import Path
import gen_map_data as g

ROOT = Path(__file__).resolve().parents[1]
town = g.load_events("map2_s00")
entry = next(i for i, e in enumerate(town) if e['sys_state'] == g.value('SysState_LoadOverlay') and
             e['map_idx'] == g.value('MapIdx_MAP1_S00') and not e['required_flag'])
lower = g.load_events("map1_s00")
stairs = next(e for e in lower if e['sys_state'] == g.value('SysState_LoadOverlay') and e['map_idx'] == g.value('MapIdx_MAP1_S01'))
upper = g.load_events("map1_s01")


def warp(map_name, point):
    p = g.load_points(map_name)[point]
    return f'warp {p["x"]:.4f} {-p["z"]:.4f} {-p["param0"] * 360 / 256:.4f}'


steps = ['log 15', '60 NONE']
for _ in range(5):
    steps += ['1 DOWN', '1 NONE']
steps += ['1 A', '240 NONE', f'event {entry}', '300 NONE', warp('map1_s00', stairs['point_idx']), '360 NONE']
statue = next(e for e in upper if e['func'] == 'MapEvent_HandStatueInspect')
steps += [warp('map1_s01', statue['point_idx']), '120 NONE', '1 A', '180 NONE', '1 A', '30 NONE', '1 A', '120 NONE']
chemical = next(e for e in upper if e['func'] == 'MapEvent_ChemicalItemTake')
steps += [warp('map1_s01', chemical['point_idx']), '120 NONE', '1 A', '180 NONE', '1 B', '60 NONE',
          '1 A', '180 NONE', '1 A', '120 NONE', 'quit']
output = ROOT / 'build/story-regressions/schoolupper.txt'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('\n'.join(steps) + '\n', encoding='ascii')
print(output)
