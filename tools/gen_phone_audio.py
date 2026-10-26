"""Reuse original player footstep selection/latches and phone animation sound calls."""
from pathlib import Path
import re
import sys
import gen_map_data as g
from gen_school_smoke import function

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/phone_audio_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/player_control.c').read_text())
    shared = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/include/maps/shared/sharedFunc_800CDAA8_0_s02.h').read_text())

    def adapt(code):
        code = re.sub(r'ANIM_STATUS\((\w+), (true|false)\)', lambda m: str(g.value(m[1]) * 2 + (m[2] == 'true')), code)
        code = re.sub(r'Q8(?:_CLAMPED)?\(([-\d.]+)f\)', lambda m: str(int(float(m[1]) * 256)), code)
        code = code.replace('player->model.anim.status', 'player->model.animation_status').replace('player->model.anim.keyframeIdx', 'player->model.keyframe')
        code = code.replace('playerProps.flags', 'audio->flags').replace('player->properties.player.field_10C', 'audio->intensity')
        code = re.sub(r'Sfx_WithPitchPlay\((\w+), &player->position, ([^,]+), ([^)]+)\)', r'phone_audio_emit(frame, \1, \2, \3, 0)', code)
        code = re.sub(r'Sfx_WithFlagsPlay\((\w+), &player->position, ([^,]+), ([^)]+)\)', r'phone_audio_emit(frame, \1, \2, 0, \3)', code)
        code = code.replace('Rng_Rand16()', 'sh_rng_rand16()')
        code = re.sub(r'\b(?:PlayerFlag_\w+|SfxFlag_\w+|Sfx_\w+|GroundType_\w+|MapIdx_\w+|PlayerState_\w+)\b', lambda m: str(g.value(m[0])), code)
        code = re.sub(r'\b(?:s32|u32|s8)\b', 'int', code)
        return code

    ground = function(source, 'Player_FootstepSfxGet')
    start = ground.index('switch (g_SavegamePtr->mapIdx)')
    end = ground.index('switch (groundType)', start)
    overrides = ground[start:end]
    assert 'case MapIdx_MAP1_S02:' not in overrides
    ground = ground[:start] + ground[end:]
    ground = re.sub(r'asm volatile\(.*?: "memory"\);', '', ground, flags=re.S)
    ground = ground.replace('g_SavegamePtr->mapIdx', str(g.value('MapIdx_MAP1_S02')))
    # The only remaining positional special case is MAP2_S00, impossible in MAP1_S02.
    start = ground.index('if (mapIdx == MapIdx_MAP2_S00)')
    end = ground.index('            break;', start)
    ground = ground[:start] + ground[end:]
    ground = re.sub(r'\s*(?:s32 mapIdx;|mapIdx = \d+;)', '', ground)
    ground = adapt(ground)
    foot = adapt(function(source, 'Player_FootstepSfxPlay'))
    event = adapt(function(source, 'func_80071620'))
    secondary = shared.split('Player_FootstepSfxGet(surface.groundType, &footstepSfxId, &pitch0, &pitch1);', 1)[1]
    calls = []
    for state in ('PlayerState_Reset', 'PlayerState_WalkForward', 'PlayerState_TurnRight', 'PlayerState_TurnLeft', 'PlayerState_Unk97'):
        body = secondary.split(f'case {state}:', 1)[1].split('#endif', 1)[0]
        calls += [f'case {state}:', body]
    calls = '\n'.join(calls)
    calls = calls.replace('Player_FootstepSfxPlay(', 'phone_footstep(audio, frame, ').replace('func_80071620(', 'phone_event_sound(audio, frame, ')
    pickup = shared.split('case PlayerState_Unk97:', 1)[1].split('#endif', 1)[0]
    base = re.search(r'keyframeIdx = (\d+)', pickup)[1]
    calls = re.sub(r'(?<!\.)\bkeyframeIdx\b', base, calls)
    calls = adapt(calls)
    result = '\n'.join([
        '/* Generated from Player_FootstepSfxGet/Play, func_80071620 and shared player states. */',
        f'void sh_phone_audio_reset(ShPhoneAudio *audio) {{ audio->flags = {g.value("PlayerFlag_Unk4") | g.value("PlayerFlag_Unk5")}; audio->intensity = 0; }}',
        f'int sh_phone_sound_no_distance_flag(void) {{ return {g.value("SfxFlag_NoDistAtten")}; }}',
        'static void phone_audio_emit(ShPhoneAudioFrame *frame, int id, int volume, int pitch, int flags)',
        '{ if (frame->count < sizeof(frame->sounds) / sizeof(frame->sounds[0])) { frame->sounds[frame->count++] = (ShPhoneSound){id, volume, pitch, flags}; } }',
        'static void phone_ground_sound(int groundType, int *sfxId, int *pitch0, int *pitch1)', '{', ground, '}',
        'static bool phone_footstep(ShPhoneAudio *audio, ShPhoneAudioFrame *frame, int animStatus, const ShPathPlayer *player, int keyframe0, int keyframe1, int sfx, int pitch)', '{', foot, '}',
        'static bool phone_event_sound(ShPhoneAudio *audio, ShPhoneAudioFrame *frame, int animStatus, const ShPathPlayer *player, int keyframeIdx, int sfxId)', '{', event, '}',
        'void sh_phone_audio_step(ShPhoneAudio *audio, const ShPathPlayer *player, int ground_type, ShPhoneAudioFrame *frame)', '{',
        '    int footstepSfxId, pitch0, pitch1;', '    frame->count = 0;',
        '    phone_ground_sound(ground_type, &footstepSfxId, &pitch0, &pitch1);',
        '    switch (player->state) {', calls, '    default: break;', '    }', '}', ''])
    assert not re.search(r'\b(?:g_|Player_|Sfx_|Rng_|playerProps|asm)\w*', g.strip_comments(result)), result
    return '\n'.join(line.rstrip() for line in result.split('\n'))


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: original phone player sound selection, RNG, footstep/event latches and frame triggers')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
