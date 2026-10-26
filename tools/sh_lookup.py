#!/usr/bin/env python3
"""Silent Hill decomp lookup for porting work.

  sh_lookup.py func NAME [--map MAP]   function body, annotated (flags, map messages, Q12)
  sh_lookup.py flag NAME_OR_NUMBER     event flag name <-> number
  sh_lookup.py msg MAP IDX             MAP_MESSAGES[IDX] of a map (e.g. map2_s00 16)
  sh_lookup.py grep REGEX [--map MAP]  search decomp src/include
  sh_lookup.py map MAP                 files of a map + event functions it defines
  sh_lookup.py events MAP [REGEX]      MAP_EVENTS: function, flags, trigger, map point
  sh_lookup.py spawns MAP              enemy spawn table (chara_spawns.h)
  sh_lookup.py point MAP IDX           one map point (position, paper map, trigger params)
  sh_lookup.py coord X Y Z [YAW_DEG]   PS1 world -> 3DS port coordinates
  sh_lookup.py q12 VALUE               raw Q12 int <-> float (also 0x.. hex)
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DECOMP = os.path.join(ROOT, "vendor", "silent-hill-decomp")
SRC = os.path.join(DECOMP, "src")
INC = os.path.join(DECOMP, "include")

_flags = None
_msgs = {}


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def flags():
    global _flags
    if _flags is None:
        _flags = {}
        text = read(os.path.join(INC, "event_flags.h"))
        for name, val in re.findall(r"\b(EventFlag_\w+)\s*=\s*(\d+)", text):
            _flags[name] = int(val)
    return _flags


def src_files(sub=None):
    base = os.path.join(SRC, "maps", sub) if sub else SRC
    for d, _, files in os.walk(base):
        for f in files:
            if f.endswith((".c", ".h")):
                yield os.path.join(d, f)
    if not sub:
        for d, _, files in os.walk(INC):
            for f in files:
                if f.endswith(".h"):
                    yield os.path.join(d, f)


def map_messages(map_name):
    if map_name in _msgs:
        return _msgs[map_name]
    out = {}
    path = os.path.join(SRC, "maps", map_name, map_name + ".c")
    if os.path.exists(path):
        text = read(path)
        m = re.search(r"MAP_MESSAGES\[\]\s*=\s*\{(.*?)\n\};", text, re.S)
        if m:
            body = m.group(1)
            inc = re.search(r'#include\s+"(maps/shared/map_msg_common\.h)"', body)
            if inc:
                body = read(os.path.join(INC, inc.group(1))) + body
            # Keep the first (NTSC-U) branch of region #if blocks.
            body = re.sub(r"^\s*#(elif|else)\b.*?^\s*#endif.*?$", "", body, flags=re.S | re.M)
            for idx, s in re.findall(r'/\*\s*(\d+)\s*\*/\s*("(?:[^"\\]|\\.)*")', body):
                out[int(idx)] = clean_msg(s)
    _msgs[map_name] = out
    return out


def clean_msg(s):
    s = s[1:-1].replace("\\t", "").replace("\\n", " ").replace("_", " ")
    return re.sub(r"\s+", " ", re.sub(r"~[A-Z]\d*(\([^)]*\))?", "", s)).strip()


def find_function(name, map_name=None):
    pat = re.compile(r"^[\w \t\*]*\b" + re.escape(name) + r"\s*\([^;{]*\)\s*(//[^\n]*)?\s*\{", re.M)
    for path in src_files(map_name):
        text = read(path)
        m = pat.search(text)
        if not m:
            continue
        depth, i = 0, m.end() - 1
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        line = text.count("\n", 0, m.start()) + 1
        yield path, line, text[m.start():i + 1]


def annotate(code, map_name):
    fl = flags()
    msgs = map_messages(map_name) if map_name else {}
    out = []
    for ln in code.split("\n"):
        notes = []
        for f in dict.fromkeys(re.findall(r"\bEventFlag_\w+", ln)):
            if f in fl:
                notes.append(f"{f}={fl[f]}")
        m = re.search(r"Event_DisplayMapMsg\(\s*\w+\s*,\s*(\d+)", ln) or re.search(r"Map_MessageWithAudio\(\s*(\d+)", ln)
        if m and int(m.group(1)) in msgs and "//" not in ln:
            notes.append(f'msg {m.group(1)}: "{msgs[int(m.group(1))]}"')
        out.append(ln + (f"    /* {'; '.join(notes)} */" if notes else ""))
    return "\n".join(out)


def guess_map(path):
    m = re.search(r"[\\/]maps[\\/](map\d+_s\d+)[\\/]", path)
    return m.group(1) if m else None


def cmd_func(args):
    if not args:
        sys.exit("usage: func NAME [--map MAP]")
    map_name = args[args.index("--map") + 1] if "--map" in args else None
    found = False
    for path, line, code in find_function(args[0], map_name):
        found = True
        rel = os.path.relpath(path, ROOT)
        print(f"// {rel}:{line}")
        print(annotate(code, guess_map(path) or map_name))
        print()
    if not found:
        sys.exit(f"{args[0]}: not found (try: grep {args[0]})")


def cmd_flag(args):
    fl = flags()
    key = args[0]
    if key.isdigit():
        hits = [n for n, v in fl.items() if v == int(key)]
        print("\n".join(hits) or f"no name for {key}")
    else:
        rx = re.compile(key, re.I)
        for n, v in fl.items():
            if rx.search(n):
                print(f"{v:5d}  0x{v:03X}  {n}")


def cmd_msg(args):
    msgs = map_messages(args[0])
    if len(args) > 1:
        i = int(args[1])
        print(msgs.get(i, f"no message {i} in {args[0]}"))
    else:
        for i in sorted(msgs):
            print(f"{i:3d}  {msgs[i]}")


def cmd_grep(args):
    map_name = args[args.index("--map") + 1] if "--map" in args else None
    paths = [os.path.join(SRC, "maps", map_name)] if map_name else [SRC, INC]
    subprocess.call(["rg", "-n", "--no-heading", args[0], *paths])


def cmd_map(args):
    d = os.path.join(SRC, "maps", args[0])
    for f in sorted(os.listdir(d)):
        print(os.path.relpath(os.path.join(d, f), ROOT))
    print("\nevent functions:")
    for f in sorted(os.listdir(d)):
        if f.endswith(".c"):
            for name in re.findall(r"^void\s+(MapEvent_\w+)\s*\(", read(os.path.join(d, f)), re.M):
                print(f"  {name}  ({f})")


def cmd_coord(args):
    x, y, z = (float(a) for a in args[:3])
    print(f"3DS pos = ({x:g}, {-y:g}, {-z:g})")
    if len(args) > 3:
        print(f"3DS yaw = {-float(args[3]):g} deg  (forward = (-sin yaw, -cos yaw))")


def cmd_q12(args):
    v = args[0]
    if "." in v:
        print(f"Q12({v}) = {round(float(v) * 4096)}")
    else:
        n = int(v, 0)
        print(f"{n} = {n / 4096:g} (Q12)  |  angle {n / 4096 * 360:g} deg (Q12 turns)")


def cmd_events(args):
    import gen_map_data as g
    pts = g.load_points(args[0])
    rx = re.compile(args[1], re.I) if len(args) > 1 else None
    for i, e in enumerate(g.load_events(args[0])):
        n = e["names"]
        p = pts[e["point_idx"]] if e["point_idx"] < len(pts) else None
        where = f"pt {e['point_idx']} ({p['x']:g}, {p['z']:g})" if p and n.get("mapPointIdx") else "no point"
        what = e["func"] or f"{n.get('sysState', '?')} param {e['event_param']}"
        line = (f"[{i:3d}] {what:34s} req={n.get('requiredEventFlag', '-')} done={n.get('completeEventFlag', '-')} "
                f"{n.get('triggerType', '')} {n.get('activationType', '')} {where}")
        if not rx or rx.search(line):
            print(line)


def cmd_spawns(args):
    import gen_map_data as g
    for s in g.load_spawns(args[0]):
        print(f"{s['chara']:22s} ({s['x']:g}, {s['z']:g}) yaw {s['yaw_deg']:g}  flags {s['flags']}  "
              f"min {s['difficulty_name']}")


def cmd_point(args):
    import gen_map_data as g
    p = g.load_points(args[0])[int(args[1])]
    print(p, f"\n3DS pos = ({p['x']:g}, y, {-p['z']:g})  param0 as angle = {p['param0'] / 256 * 360:g} deg")


CMDS = {"events": cmd_events, "spawns": cmd_spawns, "point": cmd_point, "func": cmd_func, "flag": cmd_flag, "msg": cmd_msg, "grep": cmd_grep, "map": cmd_map,
        "coord": cmd_coord, "q12": cmd_q12}

if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if len(sys.argv) < 3 or sys.argv[1] not in CMDS:
        sys.exit(__doc__)
    CMDS[sys.argv[1]](sys.argv[2:])
