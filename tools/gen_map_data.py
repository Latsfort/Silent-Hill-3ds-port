#!/usr/bin/env python3
"""Generate a C header with a map's retail points, character spawns and events.

  gen_map_data.py MAP [OUT]     e.g. gen_map_data.py map2_s00 source/game/mapdata_map2_s00.h

Sources (vendor/silent-hill-decomp/src/maps/MAP/): map_points.h, chara_spawns.h,
MAP_events_data.c, MAP_header.c (charaGroupIds, g_MapEventFuncs). Coordinates stay in PS1
space (meters, Y down); convert with x, -y, -z when placing on 3DS.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DECOMP = os.path.join(ROOT, "vendor", "silent-hill-decomp")
MAPS = os.path.join(DECOMP, "src", "maps")
INC = os.path.join(DECOMP, "include")

_enums = None


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def strip_comments(text):
    return re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)


def enums():
    """All `enum { A = x, B, ... }` constants from the decomp headers."""
    global _enums
    if _enums is not None:
        return _enums
    _enums = {}
    texts = []
    for d, _, files in os.walk(INC):
        for f in files:
            if f.endswith(".h"):
                texts.append(strip_comments(read(os.path.join(d, f))))
    for text in texts:
        for name, expr in re.findall(r"^\s*#define\s+(\w+)[ \t]+([^\n\\]+)$", text, re.M):
            v = evaluate(expr)
            if v is not None:
                _enums.setdefault(name, v)
    for text in texts:
        text = re.sub(r"^\s*#.*$", "", text, flags=re.M)
        if True:
            for body in re.findall(r"\benum\b[^{;]*\{(.*?)\}", text, re.S):
                nxt = 0
                for entry in body.split(","):
                    entry = entry.strip()
                    if not entry:
                        continue
                    m = re.match(r"(\w+)\s*(?:=\s*(.+))?$", entry, re.S)
                    if not m:
                        continue
                    name, expr = m.group(1), m.group(2)
                    if expr is not None:
                        v = evaluate(expr)
                        if v is None:
                            continue
                        nxt = v
                    _enums.setdefault(name, nxt)
                    nxt += 1
    return _enums


def evaluate(expr):
    e = expr.strip()
    e = re.sub(r"\bQ12\(\s*([-\d.]+)f?\s*\)", lambda m: str(round(float(m.group(1)) * 4096)), e)
    e = re.sub(r"\bQ8_ANGLE\(\s*([-\d.]+)f?\s*\)", lambda m: str(round(float(m.group(1)) / 360 * 256)), e)
    e = re.sub(r"\b(true)\b", "1", e)
    e = re.sub(r"\b(false|NULL)\b", "0", e)
    table = _enums or {}

    def sub(m):
        name = m.group(0)
        if name in table:
            return str(table[name])
        raise KeyError(name)

    try:
        e = re.sub(r"\b[A-Za-z_]\w*\b", sub, e)
    except KeyError:
        return None
    if not re.fullmatch(r"[\d\s()+\-*/|&<>~x0-9a-fA-F]*", e):
        return None
    try:
        return int(eval(e.replace("/", "//"), {"__builtins__": {}}))
    except Exception:
        return None


def value(expr):
    enums()
    v = evaluate(expr)
    if v is None:
        raise ValueError(f"cannot resolve '{expr.strip()}'")
    return v


def records(text):
    """Top-level `{ ... }` blocks of an initializer list."""
    out, depth, start = [], 0, None
    for i, c in enumerate(text):
        if c == "{":
            if depth == 0:
                start = i
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                out.append(text[start + 1:i])
    return out


def fields(rec):
    return {k: v.strip() for k, v in re.findall(r"\.(\w+)\s*=\s*([^,\n]+)", rec)}


def map_dir(map_name):
    return os.path.join(MAPS, map_name)


def load_points(map_name):
    text = strip_comments(read(os.path.join(map_dir(map_name), "map_points.h")))
    pts = []
    for rec in records(text):
        f = fields(rec)
        pts.append({
            "x": value(f.get("positionX", "0")) / 4096,
            "z": value(f.get("positionZ", "0")) / 4096,
            "paper_map": value(f.get("paperMapIdx", "0")),
            "field_4_5": value(f.get("field_4_5", "0")),
            "loading_screen": value(f.get("loadingScreenId", "0")),
            "param0": value(f.get("triggerParam0", "0")) & 0xFF,
            "param1": value(f.get("triggerParam1", "0")),
        })
    return pts


def header_text(map_name):
    return read(os.path.join(map_dir(map_name), map_name + "_header.c"))


def chara_groups(map_name):
    m = re.search(r"\.charaGroupIds\s*=\s*\{([^}]*)\}", header_text(map_name))
    return [g.strip() for g in m.group(1).split(",") if g.strip()] if m else []


def event_funcs(map_name):
    m = re.search(r"g_MapEventFuncs\[\][^{]*\{(.*?)\};", strip_comments(header_text(map_name)), re.S)
    return [f.strip() for f in m.group(1).split(",") if f.strip()] if m else []


def load_spawns(map_name):
    text = strip_comments(read(os.path.join(map_dir(map_name), "chara_spawns.h")))
    groups = chara_groups(map_name)
    out = []
    for gi, block in enumerate(records(text)):
        for rec in records(block):
            parts = [p.strip() for p in rec.split(",")]
            if len(parts) < 6:
                continue
            chara = parts[1] if parts[1] != "Chara_None" else (groups[gi] if gi < len(groups) else "Chara_None")
            out.append({
                "group": gi,
                "chara": chara,
                "chara_id": value(chara),
                "x": value(parts[0]) / 4096,
                "yaw_deg": value(parts[2]) / 256 * 360,
                "flags": value(parts[3]),
                "difficulty_min": value(parts[4]),
                "difficulty_name": parts[4],
                "z": value(parts[5]) / 4096,
            })
    return out


def load_events(map_name):
    path = os.path.join(map_dir(map_name), map_name + "_events_data.c")
    text = strip_comments(read(path))
    m = re.search(r"MAP_EVENTS\[\d*\]\s*=\s*\{(.*)\};", text, re.S)
    funcs = event_funcs(map_name)
    out = []
    for rec in records(m.group(1)) if m else []:
        f = fields(rec)
        if f.get("triggerType") == "TriggerType_EndOfArray":
            break
        ev = {
            "required_flag": value(f.get("requiredEventFlag", "0")),
            "complete_flag": value(f.get("completeEventFlag", "0")),
            "trigger_type": value(f.get("triggerType", "0")),
            "activation_type": value(f.get("activationType", "0")),
            "point_idx": value(f.get("mapPointIdx", "0")),
            "required_item": value(f.get("requiredItemId", "0")),
            "sys_state": value(f.get("sysState", "0")),
            "event_param": value(f.get("eventParam", "0")),
            "transition_flags": value(f.get("transitionFlags", "0")),
            "sfx_pair": value(f.get("sfxPairIdx_8_19", "0")),
            "on_rail": value(f.get("field_8_24", "0")),
            "map_idx": value(f.get("mapIdx", "0")),
            "names": f,
            "func": None,
        }
        if f.get("sysState") == "SysState_EventCallback" and ev["event_param"] < len(funcs):
            ev["func"] = funcs[ev["event_param"]]
        out.append(ev)
    return out


def emit(map_name, precision=None):
    sym = map_name
    position_precision = 4 if precision is None else precision
    angle_precision = 3 if precision is None else precision
    pts, spawns, events = load_points(map_name), load_spawns(map_name), load_events(map_name)
    L = [f"/* Generated by tools/gen_map_data.py {map_name} - do not edit. PS1 space (Y down). */",
         "#pragma once", '#include "sh3ds/mapdata.h"', ""]
    L.append(f"static const ShMapPoint s_map_points_{sym}[] = {{")
    for i, p in enumerate(pts):
        L.append(f"    /* {i:3d} */ {{{p['x']:.{position_precision}f}f, {p['z']:.{position_precision}f}f, {p['paper_map']}, {p['field_4_5']}, "
                 f"{p['loading_screen']}, {p['param0']}, {p['param1']}}},")
    L += ["};", ""]
    L.append(f"static const ShMapSpawn s_map_spawns_{sym}[] = {{")
    for s in spawns:
        L.append(f"    {{{s['x']:.{position_precision}f}f, {s['z']:.{position_precision}f}f, {s['yaw_deg']:.{angle_precision}f}f, {s['chara_id']}, {s['group']}, "
                 f"{s['flags']}, {s['difficulty_min']}}}, /* {s['chara']} {s['difficulty_name']} */")
    if not spawns:
        L.append("    {0},")
    L += ["};", f"#define MAP_SPAWN_COUNT_{sym.upper()} {len(spawns)}", ""]
    L.append(f"static const ShMapEvent s_map_events_{sym}[] = {{")
    for i, e in enumerate(events):
        n = e["names"]
        note = ", ".join(x for x in (n.get("requiredEventFlag"), n.get("completeEventFlag"), n.get("triggerType"),
                                     e["func"] or n.get("sysState")) if x)
        L.append(f"    /* {i:3d} */ {{{e['required_flag']}, {e['complete_flag']}, {e['trigger_type']}, "
                 f"{e['activation_type']}, {e['point_idx']}, {e['required_item']}, {e['sys_state']}, "
                 f"{e['event_param']}, {e['transition_flags']}, {e['sfx_pair']}, {e['on_rail']}, "
                 f"{e['map_idx']}}}, /* {note} */")
    L += ["};", ""]
    return "\n".join(L)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = emit(sys.argv[1])
    if len(sys.argv) > 2:
        with open(sys.argv[2], "w", encoding="ascii", newline="\n") as f:
            f.write(out)
    else:
        sys.stdout.write(out)
