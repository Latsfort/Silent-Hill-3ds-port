"""Adapt every original phone callback step around existing port helpers."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_locker_body import adapt

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_phone_scene.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s02/map1_s02.c').read_text())
    name = 'MapEvent_CutscenePhoneCall'
    code = function(source, name)
    code = re.sub(r'\(g_Controller0->buttonFlags.clicked & g_GameWorkPtr->config.controllerConfig.skip\)',
                  'sh_input_down(input, KEY_START)', code)
    code = code.replace('D_800E1FD8', 'town->school.phone.early_skip')
    code = re.sub(r'Event_PathWaypointSet\([^;]+\);\s*Event_TweenReset\(0\);\s*Event_TweenReset\(3\);',
                  'sh_phone_motion_begin(&town->school.phone.motion, &town->school.card_player);', code)
    code = code.replace('Event_PathWaypointExecutePlayer(53, 1, false)', 'phone_path(town, dt)')
    code = code.replace('Event_WaitPlayerStop()', 'phone_wait_stop(town, dt)')
    # Both camera calls have already been generated and checked by gen_phone_motion.py.
    for step in (5, 9, 15, 16, 28):
        start = code.index(f'case {step}:')
        end = re.search(r'\n\s*(?:case \d+|default):', code[start + 8:]).start() + start + 8
        block = code[start:end]
        block, count = re.subn(r'Event_CameraPositionSet\([^;]+\);\s*Event_CameraLookAtSet\([^;]+\);',
                               f'phone_camera(town, {step}, dt);', block)
        assert count == 1
        code = code[:start] + block + code[end:]
    code = re.sub(r'Event_CharaAnimCmdExecute\(CharaAnimCmd_SetState, &g_SysWork.playerWork.player, (\d+), false\)',
                  r'locker_animation_start(town, \1)', code)
    code = code.replace('Event_CharaAnimCmdExecute(CharaAnimCmd_AwaitAnimEnd, &g_SysWork.playerWork.player, 0, false)',
                        'phone_animation_wait(town)')
    code = code.replace('Event_CharaAnimCmdExecute(CharaAnimCmd_AnimLock, &g_SysWork.playerWork.player, 0, false)',
                        '(town->school.phone.locked = true)')
    code = code.replace('Event_CharaAnimCmdExecute(CharaAnimCmd_AnimUnlock, &g_SysWork.playerWork.player, 0, false)',
                        '(town->school.phone.locked = false)')
    code = re.sub(r'Event_DisplayMapMsg\(false, (\d+), 0, 0, 0, false\)', r'phone_message(town, input, \1, dt)', code)
    code = code.replace('func_8003D03C()', '(town->school.phone.hidden = true)')
    code = code.replace('func_8003D01C()', '(town->school.phone.hidden = false)')
    code = code.replace('Player_EmptyWeaponHandSet()', '(town->school.empty_hand = true)')
    code = code.replace('Player_WeaponAttackRestore()', '(town->school.empty_hand = false)')
    code = code.replace('Player_ControlUnfreeze(false)', 'phone_unfreeze(town)')
    code = code.replace('Player_ControlFreeze()', 'phone_freeze(town)')
    code = code.replace('Game_TurnFlashlightOn()', 'sh_save_flashlight_set(save, true)')
    code = code.replace('D_800E1FD4 = WorldGfx_PlayerHeldItemSet(InvItemId_CutscenePhone)', '(town->school.phone.held = true)')
    code = code.replace('D_800E1FD4 = WorldGfx_PlayerPrevHeldItem(&g_SysWork.playerCombat)', '(town->school.phone.held = false)')
    code = code.replace('WorldGfx_PlayerPrevHeldItem(&g_SysWork.playerCombat)', '(town->school.phone.held = false)')
    code = code.replace('Fs_QueueIsEntryLoaded(D_800E1FD4)', 'phone_held_ready(town)')
    code = code.replace('Fs_QueueWaitForEmpty()', 'phone_assets_wait(town)')
    code = code.replace('SysWork_StateStepReset()', 'step_set(town, -1)')
    code = code.replace('Event_ScreenFadeCmd(ScreenFadeCmd_Auto, true, 2, Q12(0.0f), false)', 'locker_border_in(town)')
    code = code.replace('Event_ScreenFadeCmd(ScreenFadeCmd_Start, false, 2, Q12(0.0f), false)', '(town->border.state = SH_BORDER_FADEOUTSTART)')
    code = code.replace('CutsceneBorder_Reset()', '(town->border.state = SH_BORDER_RESET)')
    code = code.replace('vcReturnPreAutoCamWork(true)', '(town->cam_active = town->script_use_cam = false)')
    code = re.sub(r'Event_ScreenFadeCmd\(ScreenFadeCmd_(Auto|Start), (true|false), 0, Q12\(0.0f\), false\)',
        lambda m: f'ladder_fade(town, {2 if m[1] == "Auto" else 0}, {m[2]}, 0)', code)
    code = re.sub(r'SD_Call\((\w+)\)', lambda m: f'sh_snd_call({g.value(m[1])})', code)
    code = re.sub(r'Sd_XaPreLoadAudioPreTaskAdd\((\w+)\)', lambda m: f'sh_xa_preload({g.value(m[1])})', code)
    code = re.sub(r'Savegame_EventFlagClear\((\w+)\)', lambda m: f'sh_save_event_clear(save, {g.value(m[1])})', code)
    code = re.sub(r'SysWork_StateStepSet\(0, (\d+)\)', r'step_set(town, \1)', code)
    code = code.replace('playerChara.', 'town->school.card_player.position.')
    code = code.replace('            phone_assets_wait(town);', '            if (!phone_assets_wait(town)) { break; }')
    code = adapt(code)
    code = code.replace('            (town->border.state = SH_BORDER_RESET);', '            card_warp(town);\n            (town->border.state = SH_BORDER_RESET);')
    assert not re.search(r'\b(?:Savegame_|Fs_|Sd_|WorldGfx_|vcReturn|D_|g_)\w*', code), code
    output = '\n'.join(['/* Generated from MAP1_S02 MapEvent_CutscenePhoneCall; all cases retained. */',
        f'#define SH_PHONE_CALLBACK {g.event_funcs("map1_s02").index(name)}',
        'static void phone_scene(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)', '{', code, '}', ''])
    return '\n'.join(line.rstrip() for line in output.split('\n'))


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original phone callback cases0..28/default, skip branches, flags and call order')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
