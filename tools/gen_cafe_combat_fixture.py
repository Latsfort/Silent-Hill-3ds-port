"""Extract the actual cafe combat path for focused host input regression tests."""
from pathlib import Path
import re
import gen_map_data as g
from gen_school_smoke import function

root = Path(__file__).resolve().parents[1]
source = g.strip_comments((root / 'source/game/cafe.c').read_text(encoding='utf-8'))
lines = ['/* Extracted from source/game/cafe.c; do not edit. */',
         '#include "sh3ds/cafe.h"', '#include "sh3ds/controls.h"',
         '#include "sh3ds/snd.h"', '#include <3ds.h>', '#include <math.h>']
for name in ('PI_F', 'DEG', 'SH_HANDGUN_DAMAGE', 'SH_KNIFE_DAMAGE', 'SH_HANDGUN_RANGE', 'SH_KNIFE_RANGE'):
    lines.append(re.search(r'^#define ' + name + r'\b[^\n]*', source, re.M)[0])
for name, signature in [
    ('wrap_angle', 'static float wrap_angle(float a)'),
    ('combat_update', 'static void combat_update(ShCafe *cafe, ShSavegame *save, float px, float pz, float yaw, const ShInputState *input, float dt)'),
]:
    lines += [signature, '{', function(source, name), '}']
result = ('\n'.join(lines) + '\n').encode('ascii', 'backslashreplace').decode('ascii')
(root / 'build/cafe_combat_under_test.h').write_text(result, encoding='ascii')
print('PASS: fixture extracted from actual cafe combat implementation')
