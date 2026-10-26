# Acid smoke implementation status

The physics, texture atlas, rendering backend and complete acid callback are
connected. Chemical Use is available at its retail item trigger upstairs.

Sources:

- `src/maps/map1_s01/map1_s01.c`, `func_800D7308` case 6: configuration.
- `src/maps/unk_draw_m1s01.c`: initialization, delayed spawn, movement, shade,
  lifecycle and projection.
- `src/bodyprog/gfx/bodyprog_effects_8005E0DC.c`: global emission/fade update.
- `src/main/rng.c`: shared LCG, initially zero as in the retail BSS.
- SHA1-verified `BODYPROG.BIN`: `ratan2`, `SquareRoot0`, `ratan_tbl`, `SQRT`.

`gen_fixed_gte.py` extracts the two math tables. `check_fixed_gte.py` executes the
original MIPS instructions, including branch delay slots and GTE leading-bit
count, and compares 148310 results against the native C functions.

`gen_school_smoke.py` extracts the 300-slot pool capacity and the acid settings.
It also emits a reference directly from the original particle functions, with
only the projection portion excluded. The native regression compares config,
RNG seed and all particle fields for 32 seeds and 600 frames each. It checks that
reserved slots survive and that 30/60 Hz presentation produces identical physics.

The retail instruction at MAP1_S01 0x800CBDB4 performs DIV without a software
zero-divisor trap. A zero radial divisor leaves the dividend in HI. The port
retains that behavior rather than using undefined C remainder by zero. The test
exercises 1598 such cases.

The BLD atlas is loaded on entry to map1_s01. It contains the three retail smoke
images at every possible source brightness. Palettes are shaded in 5-bit space,
deduplicated to 22 palettes, and packed into a 256x512 texture. This avoids losing
PS1 quantization through an approximate GPU tint. The native regression checks
196608 original texels at all nonzero brightnesses, growth, one-pixel fallback,
depth/culling and output-buffer limits. The emulator confirmed successful texture
and vertex-buffer allocation while the upstairs checkpoint retained grounded
gameplay.

The GPU path uses additive blending, an unlit texture, and depth testing without
depth writes. Billboard projection uses the port's existing camera; a full PS1
screen-position/render comparison has not yet been performed.

`gen_school_acid.py` extracts the event camera/player warp, timing, sound,
animation and flag together with the original border states and geometry.
The callback retains cases 0 through 9, the START jump to case 8, sound 1433,
smoke start/stop and flag 74 at completion. Chemical remains in the inventory,
as in the source. Border shades use Q12 delta, one-second transitions and the
retail 4095 clamp; the final black fade uses the existing screen-fade path.

Native scene regressions pass normal completion and skip from cases 1 through 7
at 30/60 Hz, including the subsequent gold pickup. The normal Old 3DS emulator
scenario uses Chemical through the inventory, confirms actual smoke vertex
submission, restores gameplay and obtains the gold medallion. Build passes with
zero warnings. No hardware run or visual comparison against PS1 has been
performed for this effect.
