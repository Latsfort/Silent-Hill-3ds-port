"""Adapt retail timed-message control; phone pages contain only J/D/N/E codes."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/phone_text_steps.h'


def ntsc(text):
    active = [True]
    result = []
    for line in text.splitlines():
        if line.startswith('#if VERSION_REGION_IS('):
            active.append(active[-1] and 'NTSCJ' not in line)
        elif line.startswith('#endif'):
            active.pop()
        elif active[-1]:
            result.append(line)
    assert len(active) == 1
    return '\n'.join(result)


def constants(code):
    code = re.sub(r'Q12\(0.0f\)', '0', code)
    code = re.sub(r'\b(?:MapMsg\w+|AudioStreamingState_\w+)\b', lambda m: str(g.value(m[0])), code)
    header = (ROOT / 'vendor/silent-hill-decomp/include/bodyprog/events/map_msg.h').read_text()
    all_length = re.search(r'#define MAP_MESSAGE_DISPLAY_ALL_LENGTH (\d+)', header)[1]
    code = code.replace('MAP_MESSAGE_DISPLAY_ALL_LENGTH', all_length)
    code = code.replace('MAP_MSG_UNSKIPPABLE_AUDIO_TYPE_FLAG', str(g.value('MAP_MSG_UNSKIPPABLE_AUDIO_TYPE_FLAG')))
    return code.replace('NO_VALUE', '-1').replace('s32', 'int32_t')


def emit():
    display = (ROOT / 'vendor/silent-hill-decomp/src/bodyprog/text/map_msg_display.c').read_text()
    body = ntsc(function(g.strip_comments(display), 'Gfx_MapMsg_Draw'))
    body = re.sub(r'^\s*(?:static\s+)?(?:s32|bool)\s+\w+;\s*$', '', body, flags=re.M)
    first = body.index('skip = false;')
    last = body.index('if (activeMapMsgIdx != mapMsgIdx)')
    body = body[:first] + body[last:]
    # No selection code can be reached by the six verified J/D/N/E-only strings.
    start = body.index('else if (g_Controller0')
    end = body.index('\n                }\n                else if', start)
    body = body[:start] + body[end:]
    body = re.sub(r'\s*(?:Gfx_MapMsg_Reset\(\)|unkJapVal = Gfx_MapMsg_WidthsCompute\([^;]+\)|Gfx_StringColorSet\([^;]+\)|Gfx_StringPositionSet\([^;]+\)|Game_TimerUpdate\(\));', '', body)
    body = body.replace('g_SysWork.enableHalfHeightGlyphs = false;', '')
    body = body.replace('g_SysWork.bgmStatusFlags & BgmStatusFlag_VoiceDialog', 'voice_dialog')
    body = body.replace('Sd_AudioStreamingCheck()', 'streaming').replace('SD_Call(19)', '(text->stop_audio = true)')
    body = body.replace('Gfx_MapMsg_SelectionUpdate(g_MapMsg_CurrentIdx, &displayLength)', 'phone_text_string(text)')
    fields = {
        'g_SysWork.isMgsStringSet': 'active', 'g_SysWork.mapMsgTimer': 'timer',
        'g_MapMsg_Select.maxIdx': 'max_index', 'g_MapMsg_Select.selectedEntryIdx': 'selected',
        'g_MapMsg_AudioType': 'audio_type', 'g_MapMsg_CurrentIdx': 'current',
        'rolloutState': 'rollout', 'menuSelection': 'menu', 'displayLength': 'display_length',
        'activeMapMsgIdx': 'active_message', 'displayLengthInc': 'increment', 'loadAudio': 'load_audio',
    }
    for name in sorted(fields, key=len, reverse=True):
        body = re.sub(r'\b' + re.escape(name) + r'\b', 'text->' + fields[name], body)
    body = body.replace('mapMsgIdx', 'message').replace('CLAMP(', 'phone_text_clamp(')
    body = body.replace('g_DeltaTimeRaw', 'delta')
    body = constants(body)
    body = re.sub(r'\n\s*if \(text->audio_type == 0\)\s*\{\s*\}', '', body)
    body = re.sub(r'\n[ \t]*\n', '\n', body)
    assert not re.search(r'\b(?:g_|Gfx_|Sd_|SD_|Game_|unkJapVal)\w*', body), body

    draw = (ROOT / 'vendor/silent-hill-decomp/src/bodyprog/text/text_draw.c').read_text()
    string = function(g.strip_comments(draw), 'Gfx_MapMsg_StringDraw')
    jump = string.split('case MAP_MSG_CODE_JUMP:', 1)[1].split('case MAP_MSG_CODE_MIDDLE:', 1)[0]
    jump = jump.replace('g_SysWork.mapMsgTimer', 'text->timer').replace('g_MapMsg_AudioType', 'text->audio_type')
    jump = jump.replace('Q12(digit)', '(digit * 4096)')
    jump = constants(jump)
    instant = constants(string.split('case MAP_MSG_CODE_DISPLAY_ALL:', 1)[1].split('case MAP_MSG_CODE_END:', 1)[0]).strip()
    output = '\n'.join([
        '/* Generated from Gfx_MapMsg_Draw and the original StringDraw J parser. */',
        'static int phone_text_clamp(int value, int low, int high)',
        '{ return value < low ? low : value > high ? high : value; }',
        'static int phone_text_string(ShPhoneText *text)', '{',
        '    const ShPhonePage *page = school_timed_page(text->current);',
        '    if (!page) { return 0; }',
        '    const char *mapMsg = page->raw;',
        '    int displayLength = text->display_length, returnCode = 0;',
        '    int fractionDigits, isFraction, digit, i, arg;',
        '    unsigned length = 0;',
        '    text->visible[0] = 0;',
        '    while (true) {',
        '        switch (*mapMsg) {',
        "        case '_': text->visible[length++] = ' '; mapMsg++; break;",
        "        case ' ': case '\\t': mapMsg++; break;",
        "        case '~': {",
        '            const char code = *++mapMsg;',
        "            arg = *++mapMsg - '0';",
        '            switch (code) {',
        "            case 'J':", jump,
        "            case 'D': " + instant,
        "            case 'N': text->visible[length++] = '\\n'; break;",
        "            case 'E': returnCode = -1; goto finish;",
        '            }',
        '            mapMsg++; break;',
        '        }',
        "        case '\\0': returnCode = 1; goto finish;",
        '        default:',
        '            displayLength--; text->visible[length++] = *mapMsg++;',
        '            if (displayLength <= 0) { goto finish; }',
        '        }',
        '        if (length >= sizeof(text->visible) - 1) { goto finish; }',
        '    }',
        'finish:',
        '    text->visible[length] = 0;',
        '    return returnCode;',
        '}',
        'int sh_phone_text_step(ShPhoneText *text, int message, int32_t delta, bool skip, bool voice_dialog, uint8_t streaming)',
        '{', '    int curRolloutState, curMenuSelection;',
        '    text->stop_audio = false;', '    text->visible[0] = 0;', body,
        '    #undef ROLLOUT_ADVANCE', '}', ''])
    return re.sub(r'\n{3,}', '\n\n', '\n'.join(line.rstrip() for line in output.split('\n')))


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: retail timed-page controller and J parser match generated phone adapter')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
