"""Extract the actual game.c NPC asset path for native verification with GPU-upload stubs."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'source/game/game.c').read_text()
begin = source.index('#define SH_NPC_ILM_MAX')
end = source.index('static void load_harry_map_anm', begin)
prefix = '\n'.join(re.search(r'^#define ' + name + r' .+$', source, re.M)[0]
                   for name in ('SH_CHARA_DIR', 'SH_ANIM_DIR'))
(ROOT / 'build/npc_pose_under_test.h').write_text(prefix + '\n' + source[begin:end], encoding='ascii', newline='\n')
print('Extracted actual game.c NPC loading, caching, upload and boss bone path for native test')
