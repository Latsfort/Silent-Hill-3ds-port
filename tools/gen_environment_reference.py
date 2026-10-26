"""Extract unchanged retail environment functions for the native regression oracle."""
from pathlib import Path
import re

import gen_map_data as g
import sh_lookup

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'build/environment_retail_reference.h'


def main():
    names = ('Gfx_FogParametersSet', 'func_8003F6F0', 'Math_WeightedAverageGet', 'func_8003FCB0', 'func_8003FD38',
             'func_8003FE04', 'func_8003F838', 'WorldEnv_FogDistanceSet', 'func_800559A8',
             'func_80055648', 'func_80055D78')
    functions = {}
    for name in names:
        matches = list(sh_lookup.find_function(name))
        if len(matches) != 1:
            raise ValueError(name)
        path, line, code = matches[0]
        code = re.sub(r'/\*.*?\*/|//[^\n]*', '', code, flags=re.S)
        # The retail declaration is const-qualified despite writing through its output pointer.
        if name == 'func_8003FE04':
            code = code.replace('const s_MapEffectsInfo* arg0,', 's_MapEffectsInfo* arg0,')
        if name == 'func_80055D78':
            code = code.replace('ptr1 = &g_WorldEnvWork.light.field_38;', 'ptr1 = (VECTOR3*)&g_WorldEnvWork.light.field_38;')
        functions[name] = f'/* {Path(path).relative_to(ROOT).as_posix()}:{line} */\n{code}'
    lines = ['/* Generated from retail C by tools/gen_environment_reference.py. */']
    for name in names:
        lines.append(f'#define {name} retail_{name}')
    for flag in ('DarkEnvironment', 'EnableBrightness', 'EnableLensflare'):
        symbol = 'SpecialEnvEventFlags_' + flag
        lines.append(f'#define {symbol} {g.value(symbol)}')
    for symbol in ('UnkGfxEnum_0', 'UnkGfxEnum_1', 'UnkGfxEnum_2'):
        lines.append(f'#define {symbol} {g.value(symbol)}')
    curve_source = (ROOT / 'vendor/silent-hill-decomp/src/bodyprog/gfx/bodyprog_80055028.c').read_text(encoding='utf-8')
    curve = curve_source.split('s32 D_800AE1C0[] = {', 1)[1].split('};', 1)[0]
    values = [g.value(part) for part in g.strip_comments(curve).split(',') if part.strip()]
    lines.append('static const s32 D_800AE1C0[] = {' + ', '.join(map(str, values)) + '};')
    shape = re.search(r'VECTOR3 D_800AE1B4\[1\]\s*=\s*\{([^}]+)', curve_source)[1]
    values = [g.value(part) for part in shape.split(',')]
    values = [value - (1 << 32) if value >= (1 << 31) else value for value in values]
    lines.append('static const VECTOR3 D_800AE1B4[1] = {{' + ', '.join(map(str, values)) + '}};')
    for name in names:
        code = functions[name]
        declaration = code.split('{', 1)[0].split('*/', 1)[1].strip()
        lines.append(declaration + ';')
    for name in names:
        lines.append(functions[name])
    for name in names:
        lines.append(f'#undef {name}')
    OUTPUT.parent.mkdir(exist_ok=True)
    OUTPUT.write_text('\n'.join(lines) + '\n', encoding='ascii', newline='\n')


if __name__ == '__main__':
    main()
