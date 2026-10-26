"""Adapt complete MAP1_S05 world initialization/update and the original particle config layout."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_boss_world_steps.h'
CONFIG = ROOT / 'include/sh3ds/school_boss_particle_config.h'


def outputs():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/map1_s05/map1_s05.c').read_text())
    shared = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/include/maps/shared.h').read_text())
    layout = re.search(r'typedef struct _MapHeader_field_5C\s*\{([^}]+)\}', shared)[1]
    for old, new in [('u8', 'uint8_t'), ('u16', 'uint16_t'), ('s16', 'int16_t'), ('s32', 'int32_t'), ('q3_12', 'int16_t')]:
        layout = re.sub(r'\b' + old + r'\b', new, layout)
    config = '#pragma once\n#include <stdint.h>\n/* Generated from shared.h s_MapHeader_field_5C. */\ntypedef struct {' + layout + '} ShSchoolBossParticleConfig;\n'
    def adapt(code):
        code = code.replace('= -0x2CCCu;', '= (int16_t)(-0x2CCCu);')
        code = re.sub(r'\s*i = 0;\s*while \(1\)\s*\{\s*if \(\+\+i >= ARRAY_SIZE\(sharedData_800DFB7C_0_s00\)\)\s*\{\s*break;\s*\}\s*\}', '', code)
        code = re.sub(r'\s*s32 i;', '', code)
        code = re.sub(r'WorldObject_Init\(&g_WorldObject([01]), "([A-Z_]+)", ([^;]+)\)',
            lambda m: 'boss_world_object_init(&world->objects[' + m[1] + '], "' + m[2] + '", ' + ', '.join(str(fixed(v.strip().rstrip('f'))) for v in m[3].split(',')) + ')', code)
        code = re.sub(r'WorldObjects_Add\(&g_WorldObject([01]).object, &g_WorldObject\1.position, &g_WorldObject\1.rotation\)',
            lambda m: 'ctx->object(ctx->user, &world->objects[' + m[1] + '])', code)
        code = re.sub(r'Q12_VECTOR3\(([^)]+)\)', lambda m: '(ShPathPoint){' + ', '.join(str(fixed(v.strip().rstrip('f'))) for v in m[1].split(',')) + '}', code)
        replacements = {
            'D_800D86F8': 'world->light_time', 'D_800D86FC': 'world->light_rate', 'D_800D86FE': 'world->light_amplitude',
            'sharedData_800D8568_1_s05': 'world->particles', 'D_800D8578': 'world->particles.field_10',
            'g_SysWork.lightIntensity': '*ctx->light', 'g_SysWork.lightPosition': 'world->light_position',
            'g_SysWork.lightRotation': 'world->light_rotation', 'g_SysWork.lightBoneCoord': 'world->light_bone',
            'g_SysWork.lensFlareBoneCoord': 'world->flare_bone',
            'g_WorldObject0.rotation.vy': 'world->objects[0].rotation.vy',
            'g_WorldObject1.rotation.vy': 'world->objects[1].rotation.vy',
            'sharedFunc_800CAAD0_1_s05()': 'ctx->particle_init(ctx->user, &world->particles)',
            'g_DeltaTime': 'ctx->delta', 'Math_Sin(': 'sh_fixed_sin(',
            'Rng_GenerateUInt(': 'boss_world_random(', 'Q12_MULT_PRECISE(': 'sh_q12_mul(',
            'Q12_MULT(': 'BOSS_WORLD_MULT(', 'Q12_FRACT(': 'BOSS_WORLD_FRACT(',
            'Sfx_WithFalloffAndPitchPlay(': 'ctx->falloff(ctx->user, ', 'SD_Call(': 'ctx->sound(ctx->user, ',
            's32': 'int32_t', 'NULL': 'NULL', 'Q12(SQUARE(8.0f))': str(fixed('64')),
        }
        for old in sorted(replacements, key=len, reverse=True):
            code = code.replace(old, replacements[old])
        code = re.sub(r'Savegame_EventFlagGet\((\w+)\)', lambda m: 'sh_save_event_get(ctx->save, ' + str(g.value(m[1])) + ')', code)
        code = re.sub(r'Q12(_ANGLE)?\(([-\d.]+)f?\)', lambda m: str(fixed(m[2], bool(m[1]))), code)
        code = re.sub(r'Q8\(([-\d.]+)f?\)', lambda m: str(int(float(m[1]) * 256)), code)
        code = re.sub(r'\bSfx_\w+\b', lambda m: str(g.value(m[0])), code)
        assert not re.search(r'\b(?:g_|D_|sharedData_|sharedFunc_|Savegame_|WorldObject|Sfx_|SD_|Q12|Q8)\w*', code), code
        return code
    steps = '\n'.join(['/* Generated from complete MAP1_S05 Map_WorldObjectsInit/Update; do not edit. */',
        'static void boss_world_begin(ShSchoolBossWorld *world, ShSchoolBossWorldContext *ctx)', '{', adapt(function(source, 'Map_WorldObjectsInit')), '}',
        'static void boss_world_update(ShSchoolBossWorld *world, ShSchoolBossWorldContext *ctx)', '{', adapt(function(source, 'Map_WorldObjectsUpdate')), '}', ''])
    return {OUTPUT: steps, CONFIG: config}


if __name__ == '__main__':
    for path, result in outputs().items():
        if '--check' in sys.argv:
            assert path.read_text(encoding='ascii') == result
        else:
            path.write_text(result, encoding='ascii', newline='\n')
    if '--check' in sys.argv:
        print('PASS: complete boss world init/update, object/light/sound values and particle layout match retail source')
