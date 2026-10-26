"""Retain both MAP1_S05 event switches, skip branches, flags, light ramps and fade calls."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_boss_scene_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s05/map1_s05.c').read_text())
    header = (ROOT / 'vendor/silent-hill-decomp/include/maps/map1/map1_s05.h').read_text()
    capacity = int(re.search(r'#define MAP_FIELD_4C_COUNT (\d+)', header)[1])
    def adapt(code):
        code = code.replace('g_Controller0->buttonFlags.clicked & g_GameWorkPtr->config.controllerConfig.skip', 'ctx->skip')
        code = re.sub(r'Event_Camera(Position|LookAt)Set\(NULL,\s*Q12\(([-\d.]+)f\), Q12\(([-\d.]+)f\), Q12\(([-\d.]+)f\),\s*Q12\(0.0f\), Q12\(0.0f\), Q12\(0.0f\), Q12\(0.0f\),\s*true\)',
            lambda m: 'ctx->camera(ctx->user, ' + ('false' if m[1] == 'Position' else 'true') + ', &(ShPathPoint){' + ', '.join(str(fixed(m[i])) for i in (2, 3, 4)) + '})', code)
        code = re.sub(r'Q12_MULT_FLOAT_PRECISE\(g_DeltaTime, ([-\d.]+)f\)', lambda m: 'sh_q12_mul(ctx->delta, ' + str(fixed(m[1])) + ')', code)
        replacements = {
            'g_SysWork.sysStateSteps[0]': 'scene->step',
            'g_SysWork.lightIntensity': 'scene->light',
            'sharedData_800D8568_1_s05.field_1': 'ctx->particles->field_1',
            'sharedData_800D8568_1_s05.field_2': 'ctx->particles->field_2',
            'sharedData_800D8568_1_s05.field_10': 'ctx->particles->field_10',
            'D_800D5D11': 'scene->exit_sound_phase',
            'ARRAY_SIZE(sharedData_800DFB7C_0_s00)': str(capacity),
            'sharedData_800DFB7C_0_s00[i].field_A': '*boss_scene_particle(ctx, i)',
            's32': 'int32_t',
            'SysWork_StateStepIncrement(0)': 'boss_scene_next(scene)',
            'SysWork_StateStepReset()': 'boss_scene_reset(scene)',
            'Player_ControlFreeze()': 'ctx->control(ctx->user, true)',
            'Player_ControlUnfreeze(false)': 'ctx->control(ctx->user, false)',
            'CutsceneBorder_ForceShow()': 'ctx->border_show(ctx->user)',
            'vcReturnPreAutoCamWork(': 'ctx->camera_return(ctx->user, ',
            'SysWork_StateSetNext(SysState_Gameplay)': 'boss_scene_gameplay(scene, ctx)',
            'func_8003A16C()': 'ctx->autosave(ctx->user)',
            'Event_WaitTimer(': 'boss_scene_wait(scene, ctx, ',
            'Event_ScreenFadeCmd(': 'boss_scene_fade(scene, ctx, ',
            'SD_Call(': 'ctx->sound(ctx->user, ',
            'Sd_SfxAttributesUpdate(': 'ctx->sound_attributes(ctx->user, ',
            'Q12_FRACT(': 'BOSS_SCENE_FRACT(', 'MAX(': 'BOSS_SCENE_MAX(',
        }
        for old in sorted(replacements, key=len, reverse=True):
            code = code.replace(old, replacements[old])
        code = re.sub(r'Savegame_EventFlag(Set|Clear)\((\w+)\)', lambda m: 'sh_save_event_' + ('set' if m[1] == 'Set' else 'clear') + '(ctx->save, ' + str(g.value(m[2])) + ')', code)
        code = re.sub(r'Q12\(([-\d.]+)f?\)', lambda m: str(fixed(m[1])), code)
        code = re.sub(r'\b(?:ScreenFadeCmd_\w+|Sfx_\w+)\b', lambda m: str(g.value(m[0])), code)
        code = re.sub(r'(boss_scene_next\(scene\);\s*)(case \d+:)', r'\1/* fall through */\n        \2', code)
        assert not re.search(r'\b(?:g_|sharedData_|sharedFunc_|Event_|SysWork_|Player_|SD_|Sd_|Q12|func_)\w*', code), code
        return code
    intro = function(source, 'func_800D49AC')
    outro = function(source, 'func_800D4D1C')
    switch = outro.index('switch (g_SysWork.sysStateSteps[0])')
    outro = 'if (scene->step > 0 && scene->step < 5) { ctx->echo(ctx->user); }\n' + outro[switch:]
    return '\n'.join(['/* Generated from MAP1_S05 func_800D49AC and func_800D4D1C; original steps retained. */',
        f'#define BOSS_SCENE_PARTICLES {capacity}',
        'static void boss_intro(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx)', '{', adapt(intro), '}',
        'static void boss_outro(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx)', '{', adapt(outro), '}', ''])


if __name__ == '__main__':
    result = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == result
        print('PASS: boss intro/outro cases, skip, flags, camera, light, sound and fade requests match retail')
    else:
        OUTPUT.write_text(result, encoding='ascii', newline='\n')
