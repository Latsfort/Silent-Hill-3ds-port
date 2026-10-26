"""Extract the original selection flash and sound constants."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/choice_data.h'


def emit():
    source = (ROOT / 'vendor/silent-hill-decomp/src/bodyprog/text/map_msg_display.c').read_text(encoding='utf-8')
    selection = function(g.strip_comments(source), 'Gfx_MapMsg_SelectionUpdate')
    maximum = re.search(r'#define FLASH_TIMER_MAX\s+Q12\(([-\d.]+)f\)', selection)[1]
    shift, scale, base = re.search(r'\(\(g_MapMsg_SelectFlashTimer >> (\d+)\) \* (\d+)\) \+ (\d+)', selection).groups()
    assert 'g_MapMsg_Select.selectedEntryIdx--' in selection and 'g_MapMsg_Select.selectedEntryIdx++' in selection
    layout = re.search(r'Gfx_StringPositionSet\((\d+), \(STRING_LINE_OFFSET \* i\) \+ (\d+)\)', selection)
    spacing = re.search(r'#define STRING_LINE_OFFSET\s+(\d+)', selection)[1]
    colors = (ROOT / 'vendor/silent-hill-decomp/src/bodyprog/text/text_draw.c').read_text(encoding='utf-8')
    palette = re.findall(r'COLOR_RGBC\(\s*(\d+),\s*(\d+),\s*(\d+),', colors.split('STRING_COLORS[StringColorId_Count] = {', 1)[1].split('};', 1)[0])
    assert layout and len(palette) == 8
    graphics = ['static const uint8_t s_choice_rgb[][3] = {']
    for color in palette: graphics.append('    {' + ', '.join(str(min(255, int(v) * 2)) for v in color) + '},')
    graphics += ['};', f'#define SH_CHOICE_X {layout[1]}', f'#define SH_CHOICE_Y {layout[2]}', f'#define SH_CHOICE_LINE {spacing}']
    return '\n'.join(['/* Generated from map_msg_display.c selection helpers. */',
        f'#define SH_CHOICE_FLASH_MAX {fixed(maximum)}', f'#define SH_CHOICE_FLASH_SHIFT {shift}',
        f'#define SH_CHOICE_FLASH_SCALE {scale}', f'#define SH_CHOICE_FLASH_BASE {base}',
        f'#define SH_CHOICE_WHITE {g.value("StringColorId_White")}',
        f'#define SH_CHOICE_MOVE {g.value("Sfx_MenuMove")}',
        f'#define SH_CHOICE_CONFIRM {g.value("Sfx_MenuConfirm")}',
        f'#define SH_CHOICE_CANCEL {g.value("Sfx_MenuCancel")}'] + graphics + [''])


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original selection flash/color and move/confirm/cancel sounds')
    else: OUTPUT.write_text(data, encoding='ascii', newline='\n')
