"""Adapt the complete original Player_Controller input history and weapon restrictions."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/player_controller_steps.h'
FLAGS = ROOT / 'include/sh3ds/player_controller_flags.h'


def outputs():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/player_control.c').read_text())
    joy = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/include/bodyprog/sys/joy.h').read_text())
    flag_names = list(dict.fromkeys(re.findall(r'\b(ControllerFlag_\w+)\s*=', joy)))
    for name, expression in re.findall(r'\b(ControllerFlag_\w+)\s*=\s*([^,]+),', joy):
        g.enums()[name] = g.value(' '.join(expression.split()))
    flags = ['#pragma once', '#include <stdint.h>', '/* Generated from original controller flag enum. */']
    for name in flag_names:
        suffix = re.sub(r'(?<!^)(?=[A-Z])', '_', name[len('ControllerFlag_'):]).upper()
        flags += [f'#define SH_PAD_{suffix} UINT32_C(0x{g.value(name) & 0xffffffff:08x})']
    code = function(source, 'Player_Controller')
    fields = {
        'IsMovingForward': 'moving_forward', 'IsMovingBackward': 'moving_backward',
        'IsTurningLeft': 'turning_left', 'IsTurningRight': 'turning_right',
        'IsSteppingLeftTap': 'step_left_tap', 'IsSteppingRightTap': 'step_right_tap',
        'IsSteppingLeftHold': 'step_left_hold', 'IsSteppingRightHold': 'step_right_hold',
        'IsRunning': 'running', 'IsAiming': 'aiming', 'IsHoldAttack': 'hold_attack',
        'IsAttacking': 'attacking', 'IsShooting': 'shooting',
        'HasMoveInput': 'has_move_input', 'HasActionInput': 'has_action_input',
        'RockDrill_AttackType': 'rock_drill_direction',
    }
    for old in sorted(fields, key=len, reverse=True): code = code.replace('g_Player_' + old, 'state->' + fields[old])
    replacements = {
        'g_Controller0->rawSticks.sticks_0.leftX': 'ctx->stick_x',
        'g_Controller0->rawSticks.sticks_0.leftY': 'ctx->stick_y',
        'g_Controller0->buttonFlags.held': 'ctx->held',
        'g_Controller0->buttonFlags.clicked': 'ctx->clicked',
        'g_Controller1->buttonFlags.held': 'ctx->second_held',
        'g_GameWorkPtr->config.controllerConfig.stepLeft': 'ctx->step_left',
        'g_GameWorkPtr->config.controllerConfig.stepRight': 'ctx->step_right',
        'g_GameWorkPtr->config.controllerConfig.run': 'ctx->run',
        'g_GameWorkPtr->config.controllerConfig.aim': 'ctx->aim',
        'g_GameWorkPtr->config.controllerConfig.action': 'ctx->action',
        'g_GameWork.config.extraWalkRunCtrl': 'ctx->reverse_run',
        'g_GameWork.config.extraWeaponCtrl': 'ctx->hold_aim',
        'g_SysWork.sysState != SysState_Gameplay': '!ctx->gameplay',
        'playerCombat.weaponAttack': 'ctx->weapon_attack',
        'playerCombat.isAiming': 'ctx->is_aiming',
        'playerExtra.lowerBodyState': 'ctx->lower_state',
        'Inventory_HyperBlasterFunctionalTest()': 'ctx->blaster_test',
        'WEAPON_ATTACK_ID_GET(': 'CONTROLLER_WEAPON_ID(',
        'ABS(': 'abs(', 's32': 'int32_t',
        # joy.h FP_STICK(0.5f): Q8(0.5)/2, within its positive clamp0..127.
        'STICK_DEADZONE': str(int(0.5 * 256) // 2),
    }
    for old in sorted(replacements, key=len, reverse=True): code = code.replace(old, replacements[old])
    code = re.sub(r'WEAPON_ATTACK\((\w+),\s*(\w+)\)', lambda m: str(g.value(m[1]) + g.value(m[2]) * 10), code)
    code = re.sub(r'\b(?:ControllerFlag_\w+|EquippedWeaponId_\w+|PlayerLowerBodyState_\w+|RockDrillAttackType_\w+)\b', lambda m: str(g.value(m[0])), code)
    assert not re.search(r'\b(?:g_|Player_|Inventory_|WEAPON_|STICK_)\w*', code), code
    return {OUTPUT: '\n'.join(['/* Generated complete Player_Controller; original bit histories and branches. */',
        'void sh_player_controller_step(ShPlayerController *state, const ShPlayerControllerContext *ctx)', '{', code, '}', '']),
        FLAGS: '\n'.join(flags + [''])}


if __name__ == '__main__':
    for path, result in outputs().items():
        if '--check' in sys.argv: assert path.read_text(encoding='ascii') == result
        else: path.write_text(result, encoding='ascii', newline='\n')
    if '--check' in sys.argv: print('PASS: complete retail Player_Controller, input histories and controller enum values match source')
