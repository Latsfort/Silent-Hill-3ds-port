"""Check Azahar journals produced by run_menu_regressions.ps1."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1] / "build/menu-regressions"


def read(name):
    text = (ROOT / f"{name}.log").read_text()
    assert "AUTOTEST END" in text, name
    records, state = [], {}
    for line in text.splitlines():
        if line.startswith(("timing ", "status ")):
            state.update(re.findall(r"(\w+)=([^ ]+)", line))
        if line.startswith("f="):
            record = dict(state)
            record.update(re.findall(r"(\w+)=([^ ]+)", line))
            records.append(record)
    return records


town = [r for r in read("town_light") if r["phase"] == "3"]
light_changes = []
for r in town:
    if not light_changes or r["light"] != light_changes[-1]:
        light_changes.append(r["light"])
assert light_changes == ["1", "0", "1"], light_changes
assert all(r["light"] == "1" for r in town if r["map_open"] == "1")
assert any(r["inv"] == "1" for r in town)
assert town[-1]["inv"] == "0"
print("PASS: single flashlight toggle, map input isolation, inventory close")

cafe = read("cafe_map")
paused = [r for r in cafe if r["map_open"] == "1"]
assert len(paused) >= 4
for key in ("bird_anim", "bird_pos", "hp", "play_time"):
    assert len({r[key] for r in paused}) == 1, key
assert cafe[-1]["map_open"] == "0"
assert cafe[-1]["bird_pos"] != paused[-1]["bird_pos"]
print("PASS: cafe map freezes enemy position, animation, damage and gameplay time; resumes")

menu = read("cafe_menu")
cutscene = [r for r in menu if r["cafe"] == "1"]
assert cutscene and all(r["inv"] == "0" for r in cutscene)
assert any(r["cafe"] == "2" and r["inv"] == "1" for r in menu)
assert menu[-1]["inv"] == "0"
print("PASS: inventory locked during cafe cutscene and available after skip")

if (ROOT / "dpad.log").exists():
    dpad = [r for r in read("dpad") if r["phase"] == "3"]
    # libctru hid.h: D-pad Right/Left/Up/Down = bits 4/5/6/7.
    both_lr = [r for r in dpad if int(r["keys"], 16) == (1 << 4 | 1 << 5)]
    both_ud = [r for r in dpad if int(r["keys"], 16) == (1 << 6 | 1 << 7)]
    for cancelled in (both_lr, both_ud):
        assert len(cancelled) >= 3
        assert len({(r["pos"], r["yaw"]) for r in cancelled}) == 1
    right = [r for r in dpad if int(r["keys"], 16) == 1 << 4]
    # The last Right segment also holds the stick forward: stick takes priority.
    assert len(right) >= 6
    assert float(right[0]["yaw"]) != float(right[2]["yaw"])
    assert len({r["yaw"] for r in right[-3:]}) == 1
    assert len({r["pos"] for r in right[-3:]}) > 1
    for key in (1 << 6, 1 << 7):
        moving = [r for r in dpad if int(r["keys"], 16) == key]
        assert len(moving) >= 3 and len({r["pos"] for r in moving}) > 1
    assert {r["weapon"] for r in dpad} == {"160"}
    print("PASS: D-pad turn/walk/backstep, opposite cancellation, stick priority, weapon unchanged")
