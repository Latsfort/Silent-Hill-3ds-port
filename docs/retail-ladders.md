# School ladder port status

The existing Harry animation path now supports MAP1_S00 states 87 and 88.
`gen_school_ladder_anims.py` reads the animation mapping from SHA1-verified
MAP1_S00.BIN and checks the source playback-loop records and shared player
state keyframes. It emits frames 873..909 and 910..946, both at 18 fps.
HB_M1S00.ANM is packaged byte-for-byte from the ROM extraction, loaded on entry
to the lower school, and included in normal asset preparation.

The existing school animation regression now also bakes all 74 ladder poses
from the original HERO model and animation file, verifies finite geometry and
four loop restarts. The original 76 upstairs poses still pass. The 3DS build
passes without warnings. No new character renderer or substituted poses were
introduced.

Both callbacks are now enabled. `gen_school_ladders.py` adapts the actual retail
functions into a generated header instead of rewriting their switches. It
retains case numbers/fallthroughs, flag operations, skip conditions and position
expressions; constants use original float32 Q12 truncation. Unsupported original
API identifiers fail generation. Small adapters reuse existing fade, animation,
save and camera functions. Town update exchanges the full player XYZ/yaw with
the callbacks, preserving vertical motion. Harry uses the original empty hand
mesh during states 87/88.

Native scene regression passes normal and skip completion at both skip steps,
for both entrance points and exit flag branches at 30/60 Hz. It checks height,
rotation, camera, animation, flag cleanup and restored control. The Old 3DS
emulator also completes both entrance/room-change/exit paths, using the real
retail interaction points and generic room transition logic.

The emulator test positions Harry at original map points, waits 90 frames for
normal collision settling, then presses A. It is not a continuous walking test
from the clock tower. The initial collision failure is fixed: the old zero-
distance fallback always pushed +X, so a character exactly on a horizontal edge
slid repeatedly along the wall. The fallback now uses the split edge normal
toward its valid surface (the same surface-side distinction used by retail
func_8006B318/8006B8F8). Nonzero-distance responses are unchanged. Edges with no
valid side retain their previous fallback.

`collision_ladder_regression` uses actual SCFEFE.IPD data at both original ladder
points. It verifies a single perpendicular settling displacement and no further
sideways drift over 90 updates. The emulator repeats both entrance/exit paths
with the 90-frame settling waits and passes. A separate test-order issue pressed
A before the test warp took effect; the waits also avoid that issue. Triggers,
map points and wall geometry have not been changed.

Build passes without warnings. Ladder animation/transition tests pass natively
and at exact trigger points in the emulator. No hardware run or visual PS1
comparison has been performed.
