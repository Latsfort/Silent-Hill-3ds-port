"""Adapt the complete original Split Head damage and eight control functions."""
from pathlib import Path
import re
import struct
import hashlib
import sys
import gen_map_data as g
from gen_school_smoke import function
from gen_silver_motion import fixed

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'source/game/split_head_steps.h'


def emit():
    source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/maps/characters/split_head.c').read_text())
    def adapt(code):
        code = re.sub(r'ANIM_STATUS\((\w+), (true|false)\)', lambda m: str(g.value(m[1]) * 2 + (m[2] == 'true')), code)
        code = code.replace('ANIM_STATUS_IDX_GET(', 'SPLIT_ANIM_INDEX(')
        code = re.sub(r'WEAPON_ATTACK\((\w+), (\w+)\)', lambda m: str(g.value(m[1]) + g.value(m[2]) * 10), code)
        code = re.sub(r'Q12(_ANGLE)?\(([-\d.]+)f?\)', lambda m: str(fixed(m[2], bool(m[1]))), code)
        code = re.sub(r'Q8\(([-\d.]+)f?\)', lambda m: str(int(float(m[1]) * 256)), code)
        code = code.replace('g_DeltaTime', 'ctx->delta')
        code = code.replace('g_SavegamePtr->gameDifficulty', 'ctx->difficulty').replace('g_SysWork.playerCombat.weaponAttack', 'ctx->weapon_attack')
        code = code.replace('g_SysWork.playerWork.player.position', 'ctx->player')
        code = code.replace('splitHead->model.controlState', 'splitHead->model.control_state')
        code = code.replace('splitHead->model.anim.status', 'splitHead->model.animation_status').replace('splitHead->model.anim.time', 'splitHead->model.time')
        code = code.replace('splitHead->model.anim.alpha', 'splitHead->model.alpha')
        code = code.replace('splitHead->headingAngle', 'splitHead->heading_angle').replace('splitHead->collision.state', 'splitHead->collision_state')
        code = code.replace('splitHead->moveSpeed', 'splitHead->move_speed').replace('splitHead->damage.amount', 'splitHead->damage_amount')
        for old, new in [('angle_F0', 'angle_f0'), ('angle_EC', 'angle_ec'), ('field_EE', 'field_ee'), ('timer_F2', 'timer_f2'), ('timer_F4', 'timer_f4'), ('animTime', 'anim_time'), ('flags', 'flags'), ('field_EA', 'field_ea'), ('field_108', 'sound_latches'), ('field_10C', 'sound_timer')]:
            code = code.replace('splitHeadProps.' + old, 'splitHead->props.' + new)
        code = code.replace('sharedData_800D5880_1_s05', 'splitHead->wait_limit').replace('sharedData_800D5A8C_1_s05', 'splitHead->idle_timer')
        code = code.replace('Chara_MoveSpeedUpdate3(splitHead,', 'split_speed(splitHead, ctx,')
        code = re.sub(r'Chara_MoveSpeedUpdate\(splitHead, ([^;]+)\)', r'split_speed(splitHead, ctx, \1, 0)', code)
        code = code.replace('sharedFunc_800D4530_1_s05(splitHead)', 'split_stride_end(splitHead)')
        code = re.sub(r'Player_DisableDamage\(&isPlayerDead, ([^;]+)\)', r'split_player_damage(ctx, &isPlayerDead, \1)', code)
        code = code.replace('Chara_DamageClear(splitHead)', '(splitHead->damage_amount = 0)')
        code = code.replace('Math_AngleBetweenPositionsGet(', 'split_angle_between(').replace('Math_AngleNormalizeSigned(', 'split_angle_signed(')
        code = code.replace('Math_Vector2MagCalcSafeQ6(', 'split_distance(')
        code = code.replace('Math_Cos(', 'sh_fixed_cos(').replace('Math_Sin(', 'sh_fixed_sin(')
        code = code.replace('Rng_AddGeneratedUInt(', 'SPLIT_RANDOM_ADD(').replace('Rng_GenerateUInt(', 'split_random(').replace('Rng_Rand16()', 'sh_rng_rand16()')
        code = code.replace('Rng_GenerateInt(', 'split_random(')
        code = code.replace('g_SplitHead_MovementAnimDuration', 'splitHead->movement_duration')
        code = code.replace('Q12_DIV(', 'SPLIT_DIV(')
        code = code.replace('Q12_MULT_PRECISE(', 'sh_q12_mul(').replace('Q12_MULT(', 'SPLIT_MULT(')
        code = code.replace('ratan2(', 'sh_fixed_atan2(').replace('SquareRoot0(', 'sh_fixed_sqrt0(')
        code = code.replace('SQUARE(', 'SPLIT_SQUARE(').replace('Q12_TO_Q6(', 'SPLIT_Q12_TO_Q6(').replace('Q6_TO_Q12(', 'SPLIT_Q6_TO_Q12(')
        code = code.replace('ARRAY_SIZE(', 'SPLIT_ARRAY_SIZE(')
        code = code.replace('Q12_TO_Q8(', 'SPLIT_Q12_TO_Q8(').replace('ANIM_TIME_RANGE_CHECK(', 'SPLIT_TIME_RANGE(')
        code = code.replace('Q12_FRACT(', 'SPLIT_FRACT(')
        code = code.replace('sharedData_800D5AB0_1_s05', 's_split_effect_points').replace('sharedData_800D5BE0_1_s05', 's_split_effect_edges')
        code = code.replace('sharedFunc_800CBE7C_1_s05(', 'ctx->shed(ctx->user, ')
        code = code.replace('sharedFunc_800CCE2C_1_s05(', 'ctx->strand(ctx->user, ')
        code = re.sub(r'DIV_FAST\((\w+), 4\)', r'SPLIT_DIV4(\1)', code)
        code = code.replace('ctx->player.collision.cylinder.radius', 'ctx->player_radius')
        code = code.replace('g_SysWork.playerWork.player.collision.cylinder.radius', 'ctx->player_radius')
        code = code.replace('sharedFunc_800D4408_1_s05(', 'split_bone_point(ctx, ')
        code = code.replace('func_8005C478(', 'split_segment_projection(')
        code = code.replace('sharedFunc_800D4594_1_s05(', 'split_edge_projection(')
        code = code.replace('s_sharedFunc_800D4594_1_s05', 'ShSplitProjection')
        for old, new in [('box.top', 'top'), ('box.height', 'height'), ('box.bottom', 'bottom'), ('box.offsetY', 'offset_y'),
                         ('cylinder.field_2', 'radius'), ('cylinder.radius', 'cylinder_radius'), ('shapeOffsets.box.', 'offset.'), ('shapeOffsets.cylinder.', 'cylinder_offset.')]:
            code = code.replace('splitHead->collision.' + old, 'splitHead->hit_shape.' + new)
        for old, new in [('sharedData_800D8618_1_s05', 'contact_points'), ('sharedData_800D5CF8_1_s05', 'edge_type'),
                         ('sharedData_800D5D08_1_s05', 'edge_side'), ('sharedData_800D5AAE_1_s05', 'first_edge'), ('sharedData_800D5AAF_1_s05', 'attack_overlap')]:
            code = code.replace(old, 'splitHead->' + new)
        code = code.replace('sharedData_800D8684_1_s05', 'splitHead->player_offset[0]').replace('sharedData_800D8688_1_s05', 'splitHead->player_offset[1]')
        code = code.replace('TIMESTEP_ANGLE(', 'SPLIT_TIMESTEP_ANGLE(').replace('FP_FROM(', 'SPLIT_FP_FROM(').replace('Q12_SHIFT', '12')
        code = code.replace('ABS(', 'abs(').replace('NO_VALUE', '-1')
        code = code.replace('CLAMP_HIGH(', 'SPLIT_MIN(').replace('CLAMP_LOW(', 'SPLIT_MAX(').replace('MAX(', 'SPLIT_MAX(').replace('MIN(', 'SPLIT_MIN(')
        code = code.replace('SPLIT_SPLIT_', 'SPLIT_')
        code = re.sub(r'(angleDeltaToPlayer < \d+ \|\|) (distToPlayer > \d+ && angleDeltaToPlayer < \d+)', r'\1 (\2)', code)
        code = re.sub(r'\b(?:SplitHeadFlag_\w+|SplitHeadAnim_\w+|SplitHeadControl_\w+|SplitHeadVariantMesh_\w+|GameDifficulty_\w+|SfxFlag_\w+|Sfx_\w+|CharaFlag_\w+|CharaCollisionState_\w+)\b(?!\s*\()', lambda m: str(g.value(m[0])), code)
        code = re.sub(r'Savegame_EventFlagGet\((\w+)\)', lambda m: f'sh_save_event_get(ctx->save, {g.value(m[1])})', code)
        code = re.sub(r'Savegame_EventFlagSet\((\w+)\)', lambda m: f'sh_save_event_set(ctx->save, {g.value(m[1])})', code)
        code = re.sub(r'Sfx_WithFlagsPlay\((\d+), &splitHead->position, ([^,]+), ([^)]+)\)', r'split_sound(splitHead, ctx, \1, 0, \2, 0, 0, \3)', code)
        code = re.sub(r'Sfx_WithPitchPlay\((\d+), &(\w+), ([^,]+), ([^)]+)\)', r'split_pitch_sound(ctx, &\2, \1, \3, \4)', code)
        code = re.sub(r'Sfx_WithFalloffAndPitchPlay\((\d+), &splitHead->position, ([^,]+), ([^,]+), ([^)]+)\)', r'split_sound(splitHead, ctx, \1, 1, \2, \3, \4, 0)', code)
        code = re.sub(r'SD_Call\((\d+)\)', r'split_sound(splitHead, ctx, \1, 2, 0, 0, 0, 0)', code)
        code = re.sub(r'Sd_SfxStop\((\d+)\)', r'split_sound(splitHead, ctx, \1, 3, 0, 0, 0, 0)', code)
        code = code.replace('Ray_LosHitCheck(&trace, &splitHead->position, &dir, splitHead)', '(trace = ctx->ray(ctx->user, &splitHead->position, &dir))')
        code = code.replace('trace.hasHit', 'trace.has_hit').replace('trace.character != NULL', 'trace.character_hit')
        code = code.replace('func_8005F6B0(&g_SysWork.playerWork.player, &unkPos, 2, 7)', 'ctx->player_effect(ctx->user, &unkPos, 2, 7)')
        code = code.replace('s_RayTrace', 'ShSplitRay').replace('VECTOR3', 'ShPathPoint').replace('VECTOR', 'ShPathPoint')
        for old, new in [('q19_12', 'int32_t'), ('q20_12', 'uint32_t'), ('q3_12', 'int16_t'), ('q4_12', 'int16_t'), ('s32', 'int32_t'), ('u32', 'uint32_t'), ('s16', 'int16_t'), ('u16', 'uint16_t'), ('u8', 'uint8_t')]:
            code = re.sub(r'\b' + old + r'\b', new, code)
        code = re.sub(r'(\n\s*\})(\s*case \d+:)', r'\1\n            /* fall through */\2', code)
        assert not re.search(r'\b(?:g_|sharedData_|sharedFunc_|SplitHead_|Chara_|Player_|Ray_|Math_|Sfx_|SD_|Sd_|Savegame_)\w*', code), code
        return code

    config = (ROOT / 'vendor/silent-hill-decomp/configs/USA/maps/map1_s05.yaml').read_text()
    raw = (ROOT / 'dump/assets/VIN/MAP1_S05.BIN').read_bytes()
    assert hashlib.sha1(raw).hexdigest() == re.search(r'^sha1: (\w+)', config, re.M)[1]
    base = int(re.search(r'    vram: (0x[\dA-Fa-f]+)', config)[1], 16)
    wait = struct.unpack_from('<h', raw, 0x800D5880 - base)[0]
    idle = struct.unpack_from('<h', raw, 0x800D5A8C - base)[0]
    init = function(source, 'SplitHead_Init')
    init = re.sub(r'\s*s32 i;', '', init)
    init = init.replace('Chara_PropsClear(splitHead)', 'memset(&splitHead->props, 0, sizeof(splitHead->props))')
    init = re.sub(r'Chara_AnimSet\(splitHead, ([^;]+), (\d+)\);',
        r'splitHead->model.anim.status = \1;\n    splitHead->model.time = \2 * 4096;\n    splitHead->model.keyframe = \2;', init)
    init = init.replace('ModelAnim_AnimInfoSet(&splitHead->model.anim, SPLIT_HEAD_ANIM_INFOS);', '')
    init = init.replace('g_SplitHead_MovementOffsetX', 'splitHead->movement_offset[0]').replace('g_SplitHead_MovementOffsetZ', 'splitHead->movement_offset[1]')
    assert not any(raw[0x800D8618 - base:0x800D8618 - base + 9 * 12])
    assert not any(raw[0x800D5CF8 - base:0x800D5CF8 - base + 9])
    assert not any(raw[0x800D5D08 - base:0x800D5D08 - base + 9])
    first_edge, overlap = struct.unpack_from('<2B', raw, 0x800D5AAE - base)
    result = ['/* Generated from SplitHead_DamageTake and every original control function; ROM state verified. */',
        f'void sh_split_head_control_reset(ShSplitHead *splitHead) {{ splitHead->wait_limit = {wait}; splitHead->idle_timer = {idle}; }}',
        'void sh_split_head_begin(ShSplitHead *splitHead)', '{', adapt(init), '    sh_split_head_control_reset(splitHead);',
        '    memset(splitHead->contact_points, 0, sizeof(splitHead->contact_points));',
        '    memset(splitHead->edge_type, 0, sizeof(splitHead->edge_type));',
        '    memset(splitHead->edge_side, 0, sizeof(splitHead->edge_side));',
        f'    splitHead->first_edge = {first_edge}; splitHead->attack_overlap = {overlap};',
        '    splitHead->player_offset[0] = splitHead->player_offset[1] = 0;', '}',
        'static bool split_stride_end(const ShSplitHead *splitHead)', '{', adapt(function(source, 'sharedFunc_800D4530_1_s05')), '}',
        'void sh_split_head_damage(ShSplitHead *splitHead, ShSplitContext *ctx)', '{', adapt(function(source, 'SplitHead_DamageTake')), '}']
    for i in range(1, 9):
        result += [f'static void split_control_{i}(ShSplitHead *splitHead, ShSplitContext *ctx)', '{', adapt(function(source, f'SplitHead_Control_{i}')), '}']
    dispatch = function(source, 'SplitHead_ControlUpdate')
    dispatch = re.sub(r'\s*extern void \(\*g_SplitHead_ControlFuncs\[\]\)\(s_SubCharacter\* splitHead\);', '', dispatch)
    dispatch = dispatch.replace('g_SplitHead_ControlFuncs[splitHead->model.controlState](splitHead);',
        'switch (splitHead->model.controlState) {\n' + '\n'.join(f'case {i}: split_control_{i}(splitHead, ctx); break;' for i in range(1, 9)) + '\ndefault: return false;\n}')
    result += ['bool sh_split_head_control(ShSplitHead *splitHead, ShSplitContext *ctx)', '{',
        '    if (!ctx->ray || !ctx->player_effect) { return false; }', '    ctx->sound_count = 0;', adapt(dispatch), '    return true;', '}', '']
    symbols = (ROOT / 'vendor/silent-hill-decomp/configs/USA/maps/sym.map1_s05.txt').read_text()
    address = int(re.search(r'^SPLIT_HEAD_ANIM_INFOS\s*=\s*(0x[\dA-Fa-f]+)', symbols, re.M)[1], 16)
    controls_address = int(re.search(r'^g_SplitHead_ControlFuncs\s*=\s*(0x[\dA-Fa-f]+)', symbols, re.M)[1], 16)
    assert controls_address - address == 30 * 16
    controls = struct.unpack_from('<9I', raw, controls_address - base)
    for index in range(1, 9):
        expected = int(re.search(r'^SplitHead_Control_' + str(index) + r'\s*=\s*(0x[\dA-Fa-f]+)', symbols, re.M)[1], 16)
        assert controls[index] == expected
    body_symbols = (ROOT / 'vendor/silent-hill-decomp/configs/USA/sym.bodyprog.txt').read_text()
    kinds = {0: -1}
    for name, kind in [('Anim_BlendLinear', 1), ('Anim_PlaybackOnce', 2), ('Anim_PlaybackLoop', 0)]:
        kinds[int(re.search(r'^' + name + r'\s*=\s*(0x[\dA-Fa-f]+)', body_symbols, re.M)[1], 16)] = kind
    table = list(struct.iter_unpack('<Ibbhihh', raw[address - base:address - base + 30 * 16]))
    assert len(table) == 30
    result += ['static const struct { int kind; ShAnimClipTiming clip; int32_t rate; } s_split_clips[] = {']
    for pointer, status, variable, link, rate, first, last in table:
        assert pointer in kinds and not variable
        result.append(f'    {{{kinds[pointer]}, {{{first}, {last}, {link}}}, {rate}}},')
    result += ['};']
    animation = function(source, 'SplitHead_AnimUpdate')
    def switch_at(text, offset):
        start = text.index('switch (', offset)
        brace = text.index('{', start)
        depth = 1
        for end in range(brace + 1, len(text)):
            depth += (text[end] == '{') - (text[end] == '}')
            if depth == 0: return start, end + 1
        raise ValueError('switch')
    start, end = switch_at(animation, 0)
    mesh = animation[start:end]
    mesh = re.sub(r'WorldGfx_CharaMeshSwap\(splitHead->model.charaId, MESH_SWAP_STATUS\(SplitHeadSwappableMesh_Head, (SplitHeadVariantMesh_\w+)\)\)', r'(splitHead->mesh_variant = \1)', mesh)
    start, end = switch_at(animation, end)
    duration = animation[start:end]
    duration = duration.replace('g_SplitHead_MovementAnimDuration', 'splitHead->movement_duration')
    duration = re.sub(r'SPLIT_HEAD_ANIM_INFOS\[[^]]+\].duration.constant', 'duration', duration)
    twist_start = animation.index('if (splitHead->model.anim.status == ANIM_STATUS(SplitHeadAnim_WalkForward, true)', end)
    twist_end = animation.index('if (splitHeadProps.field_EE !=', twist_start)
    twist = animation[twist_start:twist_end]
    result += ['bool sh_split_head_timing(ShSplitHead *splitHead, ShSplitContext *ctx, ShAnimPoseTiming *pose)', '{',
        '    const int status = splitHead->model.animation_status;',
        '    if (status < 0 || status >= (int)(sizeof(s_split_clips) / sizeof(s_split_clips[0])) || s_split_clips[status].kind < 0) { return false; }',
        '    splitHead->rotation.vy = split_angle_signed(splitHead->rotation.vy);',
        adapt(mesh), '    int32_t duration = s_split_clips[status].rate;', adapt(duration),
        '    ShAnimTiming timing = {status, splitHead->model.keyframe, splitHead->model.time, splitHead->model.alpha};',
        '    const int32_t timestep = sh_q12_mul(duration, ctx->delta);',
        '    if (s_split_clips[status].kind == 1) { sh_anim_blend_timing(&timing, &s_split_clips[status].clip, timestep, pose); }',
        '    else if (s_split_clips[status].kind == 2) { sh_anim_once_timing(&timing, &s_split_clips[status].clip, timestep, pose); }',
        '    else { sh_anim_loop_timing(&timing, &s_split_clips[status].clip, timestep, pose); }',
        '    splitHead->model.animation_status = timing.status; splitHead->model.keyframe = timing.keyframe;',
        '    splitHead->model.time = timing.time; splitHead->model.alpha = timing.alpha;',
        adapt(twist), '    splitHead->props.angle_ec = splitHead->rotation.vy;', '    return true;', '}', '']
    math_source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/bodyprog_math_8005BF38.c').read_text())
    result += ['typedef struct { int32_t field_0, field_4, field_8; uint8_t field_C, field_D, field_E; } ShSplitProjection;',
        'static uint32_t split_segment_projection(int16_t *arg0, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2)', '{',
        adapt(function(math_source, 'func_8005C478')), '}',
        'static void split_edge_projection(ShSplitProjection *arg0, int32_t posX, int32_t posZ, const ShPathPoint *vec0, const ShPathPoint *vec1)', '{',
        adapt(function(source, 'sharedFunc_800D4594_1_s05').replace('s32   temp_a1;', '')), '}']
    body = function(source, 'sharedFunc_800D2D74_1_s05')
    assert 'sp20[unkIdx + 4].vy' in body
    body = body.replace('sp20[unkIdx + 4].vy', 'sp50[unkIdx].vy')
    result += ['/* Retail sp20[4] aliases the immediately following sp50[4] on the PS1 stack. */',
        'static void split_hit_shape(ShSplitHead *splitHead, ShSplitContext *ctx)', '{', adapt(body), '}']
    push = function(source, 'sharedFunc_800D3388_1_s05')
    assert push.count('&sp18[i * 16] + 32') == 2 and push.count('&sp18[k * 16] + 32') == 2
    push = push.replace('&sp18[i * 16] + 32', '&sp38[i]').replace('&sp18[k * 16] + 32', '&sp38[k]')
    push = push.replace('sp18[9];', 'sp18[9] = {0};').replace('sp28[9];', 'sp28[9] = {0};')
    result += ['/* Retail stack sp18+32 is sp38; preserve its typed projection entries. */',
        'static void split_player_offset(ShSplitHead *splitHead, ShSplitContext *ctx, int32_t *offsetX, int32_t *offsetZ)', '{', adapt(push), '}',
        'void sh_split_head_offset_prepare(ShSplitHead *splitHead, ShSplitContext *ctx)', '{',
        '    splitHead->player_offset[0] = splitHead->player_offset[1] = 0;',
        '    split_player_offset(splitHead, ctx, &splitHead->player_offset[0], &splitHead->player_offset[1]);', '}']
    contact = function(source, 'sharedFunc_800D3B30_1_s05')
    vector_table = struct.unpack_from('<15h', raw, 0x800D5A90 - base)
    result += ['static const ShPathPoint s_split_contact_offsets[] = {']
    result += ['    {' + ', '.join(map(str, vector_table[i:i+3])) + '},' for i in range(0, 15, 3)]
    result += ['};']
    contact = contact.replace('sharedData_800D5A90_1_s05', 's_split_contact_offsets')
    contact = contact.replace('sharedFunc_800D3388_1_s05(splitHead, &offsetX, &offsetZ)', 'split_player_offset(splitHead, ctx, &offsetX, &offsetZ)')
    contact = contact.replace('sharedFunc_800D2E8C_0_s00(offsetX + sharedData_800D8684_1_s05, offsetZ + sharedData_800D8688_1_s05, &sp38)',
        'split_player_push(ctx, offsetX + sharedData_800D8684_1_s05, offsetZ + sharedData_800D8688_1_s05, &sp38)')
    contact = re.sub(r'Chara_AttackReceivedSet\(&g_SysWork.playerWork.player, WEAPON_ATTACK\((\w+), (\w+)\)\)',
        lambda m: f'(ctx->player_attack = {g.value(m[1]) + g.value(m[2]) * 10})', contact)
    contact = contact.replace('Sfx_WithFlagsPlay(Sfx_Unk1473, &g_SysWork.playerWork.player.position, Q8(0.999f), SfxFlag_NoDistAtten)',
        'split_sound_at(ctx, &ctx->player, Sfx_Unk1473, Q8(0.999f), SfxFlag_NoDistAtten)')
    result += ['static void split_player_push(ShSplitContext *ctx, int32_t x, int32_t z, const ShPathPoint *velocity)', '{',
        '    ctx->player_push = (ShPathPoint){x, 0, z}; ctx->player_push_velocity = *velocity;', '}',
        'bool sh_split_head_contact(ShSplitHead *splitHead, ShSplitContext *ctx)', '{',
        '    if (!ctx->world_bones || ctx->bone_count <= 23) { return false; }',
        '    split_hit_shape(splitHead, ctx);', adapt(contact), '    return true;', '}', '']
    movement = function(source, 'SplitHead_MovementUpdate')
    assert movement.count('collResult') == 2
    movement = movement.replace('s_CollisionResult collResult;', '')
    movement = movement.replace('Chara_MovementUpdate1(splitHead, &collResult, g_SplitHead_MovementOffsetX, g_SplitHead_MovementOffsetZ)',
        'ctx->move(ctx->user, splitHead, splitHead->movement_offset[0], splitHead->movement_offset[1])')
    movement = movement.replace('g_SplitHead_MovementOffsetX', 'splitHead->movement_offset[0]').replace('g_SplitHead_MovementOffsetZ', 'splitHead->movement_offset[1]')
    result += ['bool sh_split_head_movement(ShSplitHead *splitHead, ShSplitContext *ctx)', '{',
        '    if (!ctx->world_bones || ctx->bone_count <= 2 || !ctx->move) { return false; }',
        adapt(movement), '    return true;', '}', '']
    result += ['void sh_split_head_sounds(ShSplitHead *splitHead, ShSplitContext *ctx)', '{',
        adapt(function(source, 'sharedFunc_800CF990_1_s05')), '}', '']
    effect_address = int(re.search(r'^sharedData_800D5AB0_1_s05\s*=\s*(0x[\dA-Fa-f]+)', symbols, re.M)[1], 16)
    edge_address = int(re.search(r'^sharedData_800D5BE0_1_s05\s*=\s*(0x[\dA-Fa-f]+)', symbols, re.M)[1], 16)
    assert edge_address - effect_address == 38 * 8
    points = list(struct.iter_unpack('<4h', raw[effect_address - base:edge_address - base]))
    edges = list(struct.iter_unpack('<3h', raw[edge_address - base:edge_address - base + 6 * 6]))
    assert len(points) == 38 and len(edges) == 6
    bone_count = (ROOT / 'romfs/game/anim/FAT.ANM').read_bytes()[6]
    assert all(0 <= p[3] < bone_count for p in points)
    assert all(0 <= e[0] < 38 and 0 <= e[1] < 38 for e in edges)
    result += ['static const struct { ShPathPoint vec; int16_t idx; } s_split_effect_points[] = {']
    result += [f'    {{{{{x}, {y}, {z}}}, {idx}}},' for x, y, z, idx in points]
    result += ['};', 'static const int16_t s_split_effect_edges[6][3] = {']
    result += ['    {' + ', '.join(map(str, edge)) + '},' for edge in edges]
    effects = function(source, 'sharedFunc_800D4070_1_s05')
    effects = effects.replace('(FP_FROM(splitHead->model.anim.time, Q12_SHIFT) > 14 && FP_FROM(splitHead->model.anim.time, Q12_SHIFT) < 20 ||',
        '((FP_FROM(splitHead->model.anim.time, Q12_SHIFT) > 14 && FP_FROM(splitHead->model.anim.time, Q12_SHIFT) < 20) ||')
    effects = effects.replace('splitHeadProps.field_EA - g_DeltaTime / 68);', 'splitHeadProps.field_EA - g_DeltaTime / 68);\n            /* fall through */')
    result += ['};', 'bool sh_split_head_effects(ShSplitHead *splitHead, ShSplitContext *ctx)', '{',
        f'    if (!ctx->shed || !ctx->strand || !ctx->world_bones || ctx->bone_count <= {max(p[3] for p in points)}) {{ return false; }}',
        adapt(effects), '    return true;', '}', '']
    npc_source = g.strip_comments((ROOT / 'vendor/silent-hill-decomp/src/bodyprog/bodyprog_npc_8005BF38.c').read_text())
    position = function(npc_source, 'Chara_MovementUpdate1')
    position = position[:position.index('if (collResult != NULL)')]
    position = position.replace('s_CollisionResult', 'ShSplitMoveResult').replace('chara->headingAngle', 'splitHead->heading_angle')
    position = position.replace('chara->moveSpeed', 'splitHead->move_speed').replace('chara->fallSpeed', 'splitHead->fall_speed')
    position = position.replace('chara->position.', 'splitHead->position.')
    position = position.replace('newCollResult.surface.groundHeight', 'newCollResult.ground_height')
    position = position.replace('    s32               wallResponse;', '')
    position = position.replace('wallResponse = Collision_WallDetect(&newCollResult, &offset, chara);',
        'if (!ctx->collide(ctx->user, splitHead, &offset, &newCollResult)) { return false; }')
    position = position.replace('OVERFLOW_GUARD(', 'SPLIT_OVERFLOW_GUARD(')
    position = re.sub(r'(Q12_MULT_PRECISE\([^;]+\)) << scaleRestoreShift', r'SPLIT_RESTORE(\1, scaleRestoreShift)', position)
    result += ['bool sh_split_head_position_step(ShSplitHead *splitHead, ShSplitContext *ctx, int16_t offsetX, int16_t offsetZ)', '{',
        '    if (!ctx->collide) { return false; }', adapt(position), '    return true;', '}', '']
    return '\n'.join(line.rstrip() for line in '\n'.join(result).split('\n'))


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: boss damage/eight controls/30 clips plus original bone contacts, movement, player push/bite and sound rules')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
