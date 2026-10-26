"""Compare ported integer maths against execution of the original MIPS instructions."""
import random
import struct
import subprocess

from gen_fixed_gte import ROOT, retail


def signed(value):
    return value - 0x100000000 if value & 0x80000000 else value


class RetailMath:
    def __init__(self):
        self.data, self.vram, self.symbols = retail()

    def run(self, name, a, b=0):
        regs = [0] * 32
        regs[4], regs[5], regs[31] = a & 0xffffffff, b & 0xffffffff, 0xfffffff0
        pc = self.symbols[name]
        next_pc = pc + 4
        lo, hi, leading = 0, 0, 0
        for _ in range(512):
            if pc == 0xfffffff0:
                return signed(regs[2])
            instruction = struct.unpack_from('<I', self.data, pc - self.vram)[0]
            current, pc, next_pc = pc, next_pc, next_pc + 4
            op, rs, rt, rd, shift, fn = instruction >> 26, (instruction >> 21) & 31, (instruction >> 16) & 31, (instruction >> 11) & 31, (instruction >> 6) & 31, instruction & 63
            immediate = instruction & 65535
            simm = immediate - 65536 if immediate & 32768 else immediate
            if op == 0:
                if fn == 0: regs[rd] = regs[rt] << shift
                elif fn == 2: regs[rd] = regs[rt] >> shift
                elif fn == 3: regs[rd] = signed(regs[rt]) >> shift
                elif fn == 4: regs[rd] = regs[rt] << (regs[rs] & 31)
                elif fn == 7: regs[rd] = signed(regs[rt]) >> (regs[rs] & 31)
                elif fn == 8: next_pc = regs[rs]
                elif fn == 16: regs[rd] = hi
                elif fn == 18: regs[rd] = lo
                elif fn == 26:
                    n, d = signed(regs[rs]), signed(regs[rt])
                    assert d != 0
                    lo = (abs(n) // abs(d)) * (-1 if (n < 0) != (d < 0) else 1)
                    hi = n - lo * d
                elif fn in (32, 33): regs[rd] = regs[rs] + regs[rt]
                elif fn in (34, 35): regs[rd] = regs[rs] - regs[rt]
                elif fn == 36: regs[rd] = regs[rs] & regs[rt]
                elif fn == 42: regs[rd] = int(signed(regs[rs]) < signed(regs[rt]))
                else: raise AssertionError(f'Unsupported function {instruction:08x} at {current:08x}')
            elif op == 1:
                assert rt in (0, 1)
                if (signed(regs[rs]) >= 0) == (rt == 1): next_pc = current + 4 + simm * 4
            elif op == 2: next_pc = ((current + 4) & 0xf0000000) | ((instruction & 0x3ffffff) << 2)
            elif op == 4:
                if regs[rs] == regs[rt]: next_pc = current + 4 + simm * 4
            elif op == 5:
                if regs[rs] != regs[rt]: next_pc = current + 4 + simm * 4
            elif op in (8, 9): regs[rt] = regs[rs] + simm
            elif op == 12: regs[rt] = regs[rs] & immediate
            elif op == 15: regs[rt] = immediate << 16
            elif op == 18:
                if rs == 4 and rd == 30:
                    value = regs[rt]
                    if value & 0x80000000: value ^= 0xffffffff
                    leading = 32 - value.bit_length()
                elif rs == 0 and rd == 31: regs[rt] = leading
                else: raise AssertionError(f'Unsupported GTE access {instruction:08x}')
            elif op == 33:
                address = (regs[rs] + simm) & 0xffffffff
                regs[rt] = struct.unpack_from('<h', self.data, address - self.vram)[0]
            else: raise AssertionError(f'Unsupported instruction {instruction:08x} at {current:08x}')
            regs = [value & 0xffffffff for value in regs]
            regs[0] = 0
        raise AssertionError('Retail function did not return')


if __name__ == '__main__':
    retail_math = RetailMath()
    cases = [('a', y, x) for y in range(-128, 129) for x in range(-128, 129)]
    cases += [('s', n, 0) for n in range(65536)]
    rng = random.Random(0)
    for _ in range(8192):
        cases.append(('a', rng.randint(-0x7fffffff, 0x7fffffff), rng.randint(-0x7fffffff, 0x7fffffff)))
        cases.append(('s', rng.randrange(0x80000000), 0))
    for bit in range(31):
        for offset in (-1, 0, 1):
            n = (1 << bit) + offset
            if 0 <= n <= 0x7fffffff: cases.append(('s', n, 0))
        for sign in (-1, 1):
            n = sign * (1 << bit)
            cases += [('a', n, 0), ('a', 0, n), ('a', n, n), ('a', n, -n)]
    requests = ''.join(f'{op} {a}' + (f' {b}' if op == 'a' else '') + '\n' for op, a, b in cases)
    result = subprocess.run([str(ROOT / 'build/fixed_gte_regression.exe')], input=requests, capture_output=True, text=True, check=True)
    values = list(map(int, result.stdout.splitlines()))
    assert len(values) == len(cases), result.stderr
    for (op, a, b), actual in zip(cases, values):
        expected = retail_math.run('ratan2' if op == 'a' else 'SquareRoot0', a, b)
        assert actual == expected, (op, a, b, actual, expected)
    print(f'PASS: {len(cases)} atan/root results match execution of SHA1-verified retail MIPS code')
