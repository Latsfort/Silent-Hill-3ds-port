"""Adapt original circular shared-pool allocation and SplitHead shed/strand factories."""
from pathlib import Path
import re
import sys
from gen_school_smoke import function
import gen_map_data as g

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/school_boss_emissions_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/unk_draw_m1s05.c').read_text())
    effects = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/gfx/bodyprog_effects_8005E0DC.c').read_text())
    def adapt(code):
        replacements = {
            'D_800C4408': 'pool->next_slot',
            'g_MapOverlayHdr.unkTable1Count_50': '500',
            'g_MapOverlayHdr.unkTable1_4C': 'pool->slots',
            'sharedData_800DFB7C_0_s00': 'pool->slots',
            'func_8005E7E0(': 'boss_effect_allocate(pool, ',
            'Rng_AddGeneratedUInt(': 'BOSS_EMISSION_ADD(',
            'Rng_GenerateUInt(': 'boss_emission_random(', 'NO_VALUE': '-1',
            's32': 'int32_t',
        }
        for old in sorted(replacements, key=len, reverse=True): code = code.replace(old, replacements[old])
        assert not re.search(r'\b(?:g_|sharedData_|func_|Rng_)\w*', code), code
        return code
    return '\n'.join(['/* Generated source allocator8005E7E0 and factories800CBE7C/800CCE2C. */',
        'static int32_t boss_effect_allocate(ShSchoolBossParticles *pool, int32_t arg0)', '{', adapt(function(effects, 'func_8005E7E0')), '}',
        'void sh_school_boss_shed(ShSchoolBossParticles *pool, int32_t arg0, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4)', '{', adapt(function(source, 'sharedFunc_800CBE7C_1_s05')), '}',
        'void sh_school_boss_strand(ShSchoolBossParticles *pool, int32_t arg0, int32_t arg1, int32_t posX0, int32_t posY0, int32_t posZ0, int32_t posX1, int32_t posY1, int32_t posZ1)', '{', adapt(function(source, 'sharedFunc_800CCE2C_1_s05')), '}', ''])


if __name__ == '__main__':
    result = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == result
        print('PASS: circular500-slot allocator, shed RNG and paired-strand allocation/rollback match source')
    else: OUTPUT.write_text(result, encoding='ascii', newline='\n')
