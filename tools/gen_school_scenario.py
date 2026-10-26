"""Emit an emulator input scenario using source school door and inspection points."""
from pathlib import Path
import sys

import gen_map_data as g

ROOT = Path(__file__).resolve().parents[1]
events = g.load_events("map2_s00")
entry_index, entry = next((i, row) for i, row in enumerate(events)
                         if row["sys_state"] == g.value("SysState_LoadOverlay")
                         and row["map_idx"] == g.value("MapIdx_MAP1_S00")
                         and row["required_flag"] == 0)
inspection = next(row for row in g.load_events("map1_s00") if row["func"] == "func_800D853C")
point = g.load_points("map1_s00")[inspection["point_idx"]]
steps = ["log 15", "60 NONE"]
for _ in range(5):
    steps += ["1 DOWN", "1 NONE"]
steps += ["1 A", "240 NONE", f"event {entry_index}", "300 NONE",
          f'warp {point["x"]:.4f} {-point["z"]:.4f} {-point["param0"] * 360 / 256:.4f}',
          "30 NONE", "1 A", "180 NONE", "1 A", "60 NONE", "1 A", "180 NONE"]
clock = next(row for row in g.load_events("map1_s00") if row["func"] == "MapEvent_ClockTowerInspect")
point = g.load_points("map1_s00")[clock["point_idx"]]
steps += [f'warp {point["x"]:.4f} {-point["z"]:.4f} {-point["param0"] * 360 / 256:.4f}',
          "30 NONE", "1 A", "90 NONE", "1 A", "180 NONE", "1 A", "180 NONE"]
boiler = next(row for row in g.load_events("map1_s00") if row["func"] == "MapEvent_Boiler1")
point = g.load_points("map1_s00")[boiler["point_idx"]]
steps += [f'warp {point["x"]:.4f} {-point["z"]:.4f} {-point["param0"] * 360 / 256:.4f}',
          "120 NONE", "1 A", "90 NONE", "1 B", "60 NONE", "1 A", "90 NONE", "1 A", "90 NONE", "1 A", "90 NONE"]
pickup = next(row for row in g.load_events("map1_s00") if row["func"] == "MapEvent_TownMapTake")
point = g.load_points("map1_s00")[pickup["point_idx"]]
steps += [f'warp {point["x"]:.4f} {-point["z"]:.4f} {-point["param0"] * 360 / 256:.4f}',
          "120 NONE", "1 A", "180 NONE", "1 B", "120 NONE", "1 A", "180 NONE", "1 A", "120 NONE", "quit"]
output = ROOT / "build/story-regressions/school.txt"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text("\n".join(steps) + "\n", encoding="ascii")
print(output)
