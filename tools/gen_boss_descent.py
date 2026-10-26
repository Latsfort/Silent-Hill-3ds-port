"""Adapt the original boss-area descent, retaining source fixed-point order."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed
from gen_locker_body import adapt

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_boss_descent.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s02/map1_s02.c').read_text(encoding='utf-8'))
    name = 'func_800DCF00'
    body = function(source, name)
    code = body[body.index('switch ('):]
    code = re.sub(r'Q12_MULT_FLOAT_PRECISE\(g_DeltaTime, ([-\d.]+)f\)', lambda m: f'sh_q12_mul(SH_Q12(dt), {fixed(m[1])})', code)
    code = code.replace('g_DeltaTime', 'SH_Q12(dt)').replace('Q12_MULT_PRECISE', 'sh_q12_mul')
    code = code.replace('D_800E1FE4', 'town->school.descent_speed')
    code = code.replace('playerChara.vy', 'town->school.card_player.position.vy')
    code = code.replace('g_WorldObject_BoxHide.position.vy', 'town->school.boss_box_y')
    code = code.replace('g_ScreenFadeTimestep', 'town->school.descent_fade_speed')
    code = code.replace('MIN(', 'descent_min(')
    code = code.replace('Event_WaitPlayerStop()', 'step_set(town, town->step + 1)')
    code = code.replace('Event_ScreenFadeCmd(ScreenFadeCmd_Start, true, 2, Q12(0.0f), false)', '(town->border.state = SH_BORDER_FADEINSTART)')
    code = re.sub(r'Event_ScreenFadeCmd\(ScreenFadeCmd_Start, true, 0, Q12\(([-\d.]+)f\), false\)',
        lambda m: f'descent_fade_start(town, {fixed(m[1])})', code)
    code = code.replace('Event_ScreenFadeCmd(ScreenFadeCmd_Wait, true, 0, Q12(0.0f), false)', 'descent_fade_wait(town)')
    code = re.sub(r'Event_CharaAnimCmdExecute\(CharaAnimCmd_SetState, &g_SysWork.playerWork.player, (\d+), false\)', r'locker_animation_start(town, \1)', code)
    code = re.sub(r'SD_Call\((\w+)\)', lambda m: f'sh_snd_call({g.value(m[1])})', code)
    code = re.sub(r'Sd_SfxStop\((\w+)\)', lambda m: f'sh_snd_stop({g.value(m[1])})', code)
    init = function(source, 'Map_WorldObjectsInit')
    point = re.search(r'WorldObject_PlacementInit\(&g_WorldObject_BoxHide, "BOX_HIDE", ([^;]+)\)', init)[1]
    position = [fixed(v.strip()) / 4096 for v in point.split(',')]
    position[1], position[2] = -position[1], -position[2]
    code = re.sub(r'Sfx_WithFalloffAndPitchPlay\((\w+), &g_WorldObject_BoxHide.position, ([^;]+), Q12\(([-\d.]+)f\), 0\)',
        lambda m: f'descent_sound(town, {g.value(m[1])}, {m[2]}, {m[3]}f)', code)
    code = adapt(code)
    code = code.replace('s32 var_t0;', 'int32_t var_t0;')
    code = code.replace('            break;\n\n        default:', '            town->fade_speed = SH_Q12_TO_FLOAT(town->school.descent_fade_speed);\n            break;\n\n        default:', 1)
    assert not re.search(r'\b(?:g_|D_|Event_|SD_|Sfx_|playerChara)\w*', code), code
    return '\n'.join(['/* Generated from MAP1_S02 func_800DCF00. */',
        f'#define SH_BOSS_DESCENT_CALLBACK {g.event_funcs("map1_s02").index(name)}',
        'static const float s_descent_sound_position[3] = {' + ', '.join(f'{v:.12f}f' for v in position) + '};',
        'static void descent_sound(ShTown *town, int sound, int volume, float distance)', '{',
        '    float position[3]; memcpy(position, s_descent_sound_position, sizeof(position));',
        '    position[1] = -SH_Q12_TO_FLOAT(town->school.boss_box_y);',
        '    sh_snd_falloff(sound, position, volume, distance, 0);', '}',
        'static void boss_descent(ShTown *town, ShSavegame *save, float dt)', '{', code, '}', ''])


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original descent steps, fade acceleration, sound, box/player movement and flag120')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
