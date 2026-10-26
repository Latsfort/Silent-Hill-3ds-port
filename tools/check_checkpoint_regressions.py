"""Compare emulator menu arrivals against the source-generated checkpoint states."""
from pathlib import Path
import re

import gen_debug_checkpoints as checkpoints
import gen_map_data as g

root = Path(__file__).resolve().parents[1]
data = checkpoints.emit()
radius = float(re.search(r'#define SH_COLL_PLAYER_RADIUS ([\d.]+)f', (root / 'include/sh3ds/coll.h').read_text())[1])
rows = re.findall(r'^    \{"[^"]+", (\d+), ([-\d.]+)f, ([-\d.]+)f, [-\d.]+f, (\d+)u,', data, re.M)
for index, (map_id, x, z, maps) in enumerate(rows):
    flags = set(map(int, re.search(rf's_debug_flags_{index}\[\] = \{{([^}}]+)', data)[1].split(', ')))
    items = dict((int(item), int(count)) for item, count in re.findall(r'\{(\d+), (\d+), 0, 0\}', re.search(rf's_debug_items_{index}\[\] = \{{(.*?)\n\}};', data, re.S)[1]))
    log = (root / f'build/checkpoint-regressions/{index}.log').read_text(encoding='utf-8')
    assert 'AUTOTEST END' in log
    state = re.findall(r'^checkpoint=.*$', log, re.M)[-1]
    def count(name):
        return items.get(g.value('InvItemId_' + name), 0)
    keys = ','.join(str(count('KeyOf' + name)) for name in ('Lion', 'Woodman', 'Scarecrow'))
    clock = ','.join(str(int(g.value('EventFlag_' + name) in flags)) for name in ('71', '72'))
    assert f'keys={keys} chemical={count("Chemical")} gold={count("GoldMedallion")} silver={count("SilverMedallion")}' in state, state
    assert f'clock={clock} boiler={int(g.value("EventFlag_73") in flags)} eclipse={int(g.value("EventFlag_159") in flags)}' in state, state
    frame = re.findall(r'^f=.*$', log, re.M)[-1]
    assert f'phase=3 map={map_id} stage=2 scr=0/' in frame, frame
    assert 'ground=1' in frame, frame
    px, _, pz = map(float, re.search(r'pos=([-\d.]+),([-\d.]+),([-\d.]+)', frame).groups())
    # The normal collision snap can move a trigger-centred arrival out of a wall.
    assert abs(px - float(x)) <= radius * 2 and abs(pz - float(z)) <= radius * 2, frame
    if index == 6:
        gold_log = (root / 'build/checkpoint-regressions/4.log').read_text(encoding='utf-8')
        gold_frame = re.findall(r'^f=.*$', gold_log, re.M)[-1]
        gold_height = float(re.search(r'gy=([-\d.]+)', gold_frame)[1])
        assert abs(float(re.search(r'gy=([-\d.]+)', frame)[1]) - gold_height) < .02, frame
    if map_id in (str(g.value('MapIdx_MAP1_S00')), str(g.value('MapIdx_MAP1_S01'))):
        assert f'paper_flags={maps} ' in log
    print(f'PASS: checkpoint {index} emulator inventory, puzzle flags, map, source position and ground')
