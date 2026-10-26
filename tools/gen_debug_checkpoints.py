"""Generate cumulative debug route states from retail event outcomes and map points."""
from pathlib import Path
import json
import re
import sys

import gen_map_data as g

ROOT = Path(__file__).resolve().parents[1]
DECOMP = ROOT / 'vendor/silent-hill-decomp'
OUTPUT = ROOT / 'source/game/debug_checkpoints_data.h'


def body(map_name, name):
    for path in (DECOMP / 'src/maps' / map_name).glob('*.c'):
        text = path.read_text(encoding='utf-8')
        match = re.search(r'void\s+' + name + r'\([^)]*\)[^{]*\{', text)
        if not match:
            continue
        start = match.end()
        depth = 1
        for i in range(start, len(text)):
            depth += (text[i] == '{') - (text[i] == '}')
            if not depth:
                return text[start:i]
    raise ValueError(name)


def emit():
    flags, items, checkpoints = set(), {}, []
    paper_flags = 0
    picked_count = 0

    def set_flag(name):
        flags.add(g.value(name))

    def set_outcomes(map_name, name):
        for flag in re.findall(r'Savegame_EventFlagSet\((\w+)\)', body(map_name, name)):
            set_flag(flag)

    def add(item, count=1):
        nonlocal picked_count
        item = g.value(item)
        if not items.get(item, 0):
            picked_count += 1
        items[item] = items.get(item, 0) + count

    def remove(item):
        item = g.value(item)
        assert items[item] > 0
        items[item] -= 1

    def pickup(map_name, name):
        code = body(map_name, name)
        call = re.search(r'Event_ItemTake\((\w+), (\w+), (\w+),', code)
        if call:
            item, count, flag = call.groups()
            add(item, g.value(count))
            set_flag(flag)
        else:
            item, count = re.search(r'Event_InvItemCmd\(InvItemCmd_AddItem, (\w+), (\w+),', code).groups()
            add(item, g.value(count))
        set_outcomes(map_name, name)
        for item in re.findall(r'Player_ItemRemove\((\w+), 1\)', code):
            remove(item)

    def event(map_name, name):
        return next(e for e in g.load_events(map_name) if e['func'] == name)

    def snapshot(label, map_name, point, point_map=None):
        p = g.load_points(point_map or map_name)[point]
        map_id = g.value('MapIdx_' + map_name.upper())
        checkpoints.append((label, map_id, p, sorted(flags), dict(items), paper_flags, picked_count))

    cafe = 'map0_s01'
    for name in ('MapEvent_KitchenKnifeItemTake', 'MapEvent_FlashlightItemTake', 'MapEvent_PocketRadioItemTake'):
        pickup(cafe, name)
    # Cafe item-take route; the fight is completed without spending reserve ammo.
    handgun = body(cafe, 'MapEvent_CafeCutscene')
    ammo = re.search(r'InvItemCmd_AddItem, InvItemId_Handgun, (\w+)', handgun)[1]
    add('InvItemId_Handgun')
    add('InvItemId_HandgunBullets', g.value(ammo))
    for name in ('EventFlag_M0S01_PickupMap', 'EventFlag_M0S01_PickedUpKnifeAndMap',
                 'EventFlag_M0S01_AirScreamerIntroCutsceneStarted', 'EventFlag_46', 'EventFlag_47',
                 'EventFlag_M0S01_AirScreamerDied', 'EventFlag_M0S01_AirScreamerDeathPlayed'):
        assert name in (DECOMP / 'src/maps/map0_s01/map0_s01_events.c').read_text(encoding='utf-8')
        set_flag(name)
    for e in g.load_events(cafe):
        if e['sys_state'] == g.value('SysState_Fmv') or e['func'] == 'MapEvent_CafeCutscene':
            flags.add(e['complete_flag'])
    map_code = body(cafe, 'MapEvent_MapItemTake')
    paper_flags |= 1 << int(re.search(r'paperMapFlags \|= 1 << (\d+)', map_code)[1])
    town = 'map2_s00'
    set_outcomes(town, 'MapEvent_CutsceneExitCafe')
    # Completion flags belong to the event dispatcher, even when absent from the callback.
    flags.add(event(town, 'MapEvent_CutsceneExitCafe')['complete_flag'])
    pickup(town, 'MapEvent_CherylsSketchbook')
    pickup(town, 'MapEvent_DoghouseNote')
    pickup(town, 'MapEvent_DoghouseKeyTake')
    house = event(town, 'MapEvent_HouseKeyUse')
    flags.add(house['complete_flag'])
    set_outcomes(town, 'MapEvent_HouseKeyUse')
    remove('InvItemId_HouseKey')
    set_outcomes(town, 'func_800EA6E0')
    for name in ('func_800E9A0C', 'func_800E9A74', 'func_800E9CB4'):
        pickup(town, name)
    locks = event(town, 'func_800E9DD8')
    snapshot('Trois cles - serrures Eclipse', town, locks['point_idx'])
    set_outcomes(town, 'func_800E9DD8')
    for item in re.findall(r'Player_ItemRemove\((\w+), 1\)', body(town, 'func_800E9DD8')):
        remove(item)
    eclipse = event(town, 'MapEvent_DoorOfEclipseEnter')
    flags.add(eclipse['required_flag'])
    set_outcomes(town, 'MapEvent_DoorOfEclipseEnter')
    # The completed scene's player warp is the source for its debug arrival.
    code = body(town, 'MapEvent_DoorOfEclipseEnter')
    p = dict(x=float(re.search(r'position.vx = Q12\(([-\d.]+)f\)', code)[1]),
             z=float(re.search(r'position.vz = Q12\(([-\d.]+)f\)', code)[1]),
             param0=float(re.search(r'rotation.vy = Q12\(([-\d.]+)f\)', code)[1]) * 256)
    checkpoints.append(('Apres Eclipse - nuit', g.value('MapIdx_MAP2_S00'), p, sorted(flags), dict(items), paper_flags, picked_count))
    lower, upper = 'map1_s00', 'map1_s01'
    entry = next(e for e in g.load_events(town) if e['sys_state'] == g.value('SysState_LoadOverlay') and e['map_idx'] == g.value('MapIdx_MAP1_S00') and not e['required_flag'])
    snapshot('Ecole - arrivee', lower, entry['event_param'], town)
    pickup(upper, 'MapEvent_ChemicalItemTake')
    map_code = body(lower, 'MapEvent_TownMapTake')
    index, flag = re.search(r'Event_PaperMapTake\((\d+), (\w+),', map_code).groups()
    set_flag(flag)
    utility = (DECOMP / 'src/bodyprog/events/events_util.c').read_text(encoding='utf-8').split('void Event_PaperMapTake', 1)[1]
    extra = utility.split(f'case {index}:', 1)[1].split('break;', 1)[0]
    paper_flags |= 1 << int(index)
    for expression in re.findall(r'paperMapFlags\s*\|=\s*([^;]+);', extra):
        paper_flags |= g.value(' '.join(expression.split()))
    snapshot('Ecole - acide en poche', upper, event(upper, 'func_800D7308')['point_idx'])
    set_outcomes(upper, 'func_800D7308')
    pickup(upper, 'MapEvent_GoldMedallionItemPickup')
    gold = next(e for e in g.load_events(lower) if e['activation_type'] == g.value('TriggerActivationType_Item') and e['required_item'] == g.value('InvItemId_GoldMedallion'))
    snapshot('Ecole - poser le medaillon or', lower, gold['point_idx'])
    set_outcomes(lower, gold['func'])
    snapshot('Ecole - piano', upper, event(upper, 'MapEvent_PianoPuzzleInteract')['point_idx'])
    set_outcomes(upper, 'PianoPuzzle_Control')
    snapshot('Ecole - argent au sol', upper, event(upper, 'func_800D857C')['point_idx'])
    pickup(upper, 'func_800D857C')
    silver = next(e for e in g.load_events(lower) if e['activation_type'] == g.value('TriggerActivationType_Item') and e['required_item'] == g.value('InvItemId_SilverMedallion'))
    # The silver trigger is inside the tower geometry, not a player spawn.
    # The gold-side clock point has a verified ground arrival in the port.
    snapshot('Ecole - poser le medaillon argent', lower, gold['point_idx'])
    set_outcomes(lower, silver['func'])
    boiler = event(lower, 'MapEvent_Boiler0')
    snapshot('Ecole - chaudiere alimentee', lower, boiler['point_idx'])
    # Preserve the existing checkpoint IDs when adding the pre-pickup arrival.
    checkpoints.append(checkpoints.pop(6))
    lines = ['/* Generated by tools/gen_debug_checkpoints.py. Retail event outcomes, cumulative reference route. */', '#pragma once']
    for i, (_, _, _, state_flags, state_items, _, _) in enumerate(checkpoints):
        lines += [f'static const uint16_t s_debug_flags_{i}[] = {{' + ', '.join(map(str, state_flags)) + '};',
                  f'static const ShInvItem s_debug_items_{i}[] = {{']
        lines += [f'    {{{item}, {count}, 0, 0}},' for item, count in state_items.items() if count]
        lines += ['};']
    lines.append('static const ShDebugCheckpoint s_debug_checkpoints[] = {')
    for i, (label, map_id, p, _, _, maps, picked) in enumerate(checkpoints):
        x, z = (int(p[k] * 4096) / 4096 for k in ('x', 'z'))
        yaw = -p['param0'] * 6.283185307179586 / 256
        lines.append(f'    {{{json.dumps(label)}, {map_id}, {x:.8f}f, {-z:.8f}f, {yaw:.8f}f, {maps}u, {picked}, s_debug_flags_{i}, sizeof(s_debug_flags_{i}) / sizeof(s_debug_flags_{i}[0]), s_debug_items_{i}, sizeof(s_debug_items_{i}) / sizeof(s_debug_items_{i}[0])}},')
    lines += ['};', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: cumulative debug inventories, flags, map ownership and arrival points match retail sources')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
