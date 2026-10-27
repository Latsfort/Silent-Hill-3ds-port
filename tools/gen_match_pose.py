"""Generate the match arm rotations by executing the original MIPS/GTE matrix routine."""
from pathlib import Path
import re
import struct
import sys
import gen_map_data as g
from gen_fixed_gte import ROOT, retail
from gen_silver_motion import fixed

DECOMP = ROOT / 'vendor/silent-hill-decomp'
OUTPUT = ROOT / 'source/game/match_pose_data.h'


def signed(value, bits=32):
    value &= (1 << bits) - 1
    return value - (1 << bits) if value >> (bits - 1) else value


class RetailRotation:
    def __init__(self):
        self.data, self.base, self.symbols = retail()

    def matrix(self, z, x, y, name='Math_RotMatrixZxyNegGte'):
        memory = {0x1000 + i: byte for i, byte in enumerate(struct.pack('<3h', x, y, z))}
        registers = [0] * 32
        registers[4], registers[5], registers[31] = 0x1000, 0x2000, 0xfffffff0
        gte = [0] * 32
        pc = self.symbols[name]
        next_pc = pc + 4
        lo = hi = 0
        for _ in range(512):
            if pc == 0xfffffff0:
                return struct.unpack('<9h', bytes(memory[0x2000 + i] for i in range(18)))
            instruction = struct.unpack_from('<I', self.data, pc - self.base)[0]
            current, pc, next_pc = pc, next_pc, next_pc + 4
            op, rs, rt, rd, shift, fn = instruction >> 26, (instruction >> 21) & 31, (instruction >> 16) & 31, (instruction >> 11) & 31, (instruction >> 6) & 31, instruction & 63
            immediate = instruction & 65535
            simm = signed(immediate, 16)
            if op == 0:
                if fn == 0: registers[rd] = registers[rt] << shift
                elif fn == 2: registers[rd] = registers[rt] >> shift
                elif fn == 3: registers[rd] = signed(registers[rt]) >> shift
                elif fn == 8: next_pc = registers[rs]
                elif fn == 16: registers[rd] = hi
                elif fn == 18: registers[rd] = lo
                elif fn == 24:
                    product = signed(registers[rs]) * signed(registers[rt])
                    lo, hi = product & 0xffffffff, (product >> 32) & 0xffffffff
                elif fn in (32, 33): registers[rd] = registers[rs] + registers[rt]
                elif fn in (34, 35): registers[rd] = registers[rs] - registers[rt]
                elif fn == 37: registers[rd] = registers[rs] | registers[rt]
                else: raise AssertionError(f'Unknown instruction {instruction:08x}')
            elif op == 3:
                registers[31] = current + 8
                next_pc = ((current + 4) & 0xf0000000) | ((instruction & 0x3ffffff) << 2)
            elif op in (8, 9): registers[rt] = registers[rs] + simm
            elif op == 12: registers[rt] = registers[rs] & immediate
            elif op == 15: registers[rt] = immediate << 16
            elif op == 33:
                address = (registers[rs] + simm) & 0xffffffff
                if address in memory:
                    registers[rt] = signed(memory[address] | memory[address + 1] << 8, 16)
                else:
                    registers[rt] = struct.unpack_from('<h', self.data, address - self.base)[0]
            elif op == 41:
                address = (registers[rs] + simm) & 0xffffffff
                memory[address], memory[address + 1] = registers[rt] & 255, (registers[rt] >> 8) & 255
            elif op == 18:
                if rs == 4: gte[rd] = signed(registers[rt], 16 if 8 <= rd <= 11 else 32)
                elif rs == 0: registers[rt] = gte[rd]
                else:
                    assert fn in (0x3d, 0x3e)
                    scale = 12 if instruction & (1 << 19) else 0
                    for i in range(1, 4):
                        accumulator = gte[24 + i] << scale if fn == 0x3e else 0
                        result = (accumulator + gte[8] * gte[8 + i]) >> scale
                        gte[24 + i] = signed(result)
                        gte[8 + i] = min(32767, max(0 if instruction & 1024 else -32768, result))
            else: raise AssertionError(f'Unknown instruction {instruction:08x} at {current:08x}')
            registers = [value & 0xffffffff for value in registers]
            registers[0] = 0
        raise AssertionError('Matrix routine did not return')


def emit():
    rotation = RetailRotation()
    assert rotation.matrix(0, 0, 0) == (4096, 0, 0, 0, 4096, 0, 0, 0, 4096)
    assert rotation.matrix(0, 0, 1024) == (0, 0, 4096, 0, 4096, 0, -4096, 0, 0)
    for z in range(0, 4096, 1024):
        for x in range(0, 4096, 1024):
            for y in range(0, 4096, 1024):
                assert rotation.matrix(z, x, y) == rotation.matrix(z, x, y, 'Math_RotMatrixZxyNeg')
    source = g.strip_comments((DECOMP / 'src/bodyprog/player_control.c').read_text(encoding='utf-8'))
    block = source.split('if (g_SysWork.enablePlayerMatchAnim && playerExtra.state < PlayerState_Unk58)', 1)[1].split('}', 1)[0]
    calls = re.findall(r'func_80044F14\(&g_SysWork.playerBoneCoords\[(\w+)\],\s*Q12_ANGLE\(([-\d.]+)f\),\s*Q12_ANGLE\(([-\d.]+)f\),\s*Q12_ANGLE\(([-\d.]+)f\)\)', block)
    assert len(calls) == 3
    lines = ['/* Generated by tools/gen_match_pose.py; retail func_8007D090 and MIPS/GTE. */',
             'static const struct { unsigned bone; int16_t matrix[3][3]; } s_match_pose[] = {']
    for bone, z, x, y in calls:
        matrix = rotation.matrix(fixed(z, True), fixed(x, True), fixed(y, True))
        rows = ['{' + ', '.join(map(str, matrix[i:i + 3])) + '}' for i in range(0, 9, 3)]
        lines.append('    {' + str(g.value(bone)) + ', {' + ', '.join(rows) + '}},')
    return '\n'.join(lines + ['};', ''])


if __name__ == '__main__':
    data = emit()
    if '--check' in sys.argv:
        assert OUTPUT.read_text(encoding='ascii') == data
        print('PASS: all match arm matrices from original MIPS/GTE and source bone angles')
    else:
        OUTPUT.write_text(data, encoding='ascii', newline='\n')
