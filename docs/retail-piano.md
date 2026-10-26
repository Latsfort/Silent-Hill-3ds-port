# School piano port status

`source/game/piano.c` ports `map1_s01.c` `PianoPuzzle_Control`. It retains
the numbered substates, strict timer comparison, note sounds, delayed sound
1421 after a silent key, reset/restart rules, cursor snapping and completion
before cancel handling. Its caller must apply flag 75 only when the returned
result says solved, and advance the main event when finished. The upstairs
dispatcher now enables the complete piano interaction.

`tools/gen_school_piano.py` reads the eleven signed hitbox limits and five-note
sequence from SHA1-verified `MAP1_S01.BIN`, using the USA overlay symbols and
sizes. The remaining hitboxes, bounds, sound IDs, snap positions and delays
come from the original C function. Q12 delay literals truncate float32 as in
the original macro. Both generated headers are ASCII and support `--check`.

The cursor takes retail normalized stick values (-75 through 75). Movement
scales per-frame increments by `30 * dt`; fractional Q12 motion carries across
display frames and resets on a clamp or horizontal snap. Timers use elapsed
Q12 time. The result supplies the original pre-snap cursor position for drawing
and emits each sound only on its source control step.

`tools/school_piano_regression.c` compares against a generated copy of the
original C control flow, including its timer and step helper semantics. There
are 165419 comparisons: all integer positions in the cursor bounds with each
partial sequence, all normalized analog values, fractional positions, clamp
edges, delayed substates, cancel and solved priority. It also completes the
five-note sequence and checks cursor movement at 30/60 Hz for every stick value.
These native checks and the warning-free 3DS build pass.

The cursor upload/draw path is implemented. `GameFs_BgEtcGfxLoad` identifies
BG_ETC.TIM at texture page 12 and CLUT (192,0); the piano's `Gfx_CursorDraw`
selects UV (0,64), 32x32 texels, neutral tint 128, an opaque primitive and
8x8 screen half-extents. The generator verifies the loader, source call,
palette/image bounds and byte equality of the packaged TIM and ROM extraction.
The renderer uploads the exact palette colors to a 32x32 nearest-filtered
texture and uses the original centered-screen geometry. It releases that
texture at shutdown. Loading upstairs checks allocation/upload and reports a
recoverable load error if it fails. The Old 3DS emulator log confirms
`school piano cursor ready=1`; the existing full acid/Gold scene test still
passes. The cursor result is now connected to the gameplay HUD during the
active keyboard step.

The upstairs world-object table now also includes FUTA1_HI, FUTA2_HI and the
two source initialization poses of SILVER_H. Positions use the retail float32
Q12 truncation; ground rotation uses the quantized Q12 angle. The existing
independent map-object ranges select the lid from flag 71, the silver pose
from flag 75 and hide silver after pickup flag 70, within the source piano
chunk. Native school regression checks all eight combinations and adjacent
chunk exclusions. The emulator still completes the acid/Gold route with the
expanded object table. `SchoolPianoObjects` also loads the cumulative piano
checkpoint in the Old 3DS emulator, resolves its four piano object ranges,
selects source visibility mask 80 (open lid and suspended silver), and retains
grounded gameplay. Animated silver uses its separate per-frame vertex path
during the original cutscene cases.

The silver pickup (`func_800D857C`, callback 11) is connected with its original
cases 0 through 8, animations 59/60 from the verified upstairs clip table,
message 30, accept/decline cases 6/7 and flag 70. The prompt sets the flag while
visible; declining clears it before the standing animation. Accepting adds one
silver medallion. The pickup close-up and prompt disappear before Harry stands,
and control/VC are restored after animation completion. The generated inventory
asset table includes the source UNQ62.TMD model and inspection angles.

A ninth cumulative checkpoint, `Ecole - argent au sol`, captures the retail
piano solution before the silver pickup. It retains earlier pickups, Chemical,
Gold and sun-socket state; flag 75 is set while flag 70 and Silver are absent.
Its arrival is the original upstairs pickup point. Existing checkpoint IDs
remain unchanged. Native tests verify all nine inventories/flags with reverse
reloads, plus pickup accept/decline at 30/60 Hz. The Old 3DS emulator loads this
checkpoint grounded, declines then acquires silver, plays both animation states
and returns to gameplay. This exercises the generated post-solution state; it
does not itself verify solving the piano. No hardware run has
been performed for this pickup.

`source/game/silver.c` ports the medallion motion from piano cases 12/13.
It retains PS1 Q12 positions, signed 16-bit rotations, 64-bit fixed-point
products, the source sine/cosine table, accumulated engine gravity and the
original non-clamped floor crossing. Its caller advances the case and emits
sound 1417 on that crossing. Ground/skip warps preserve accumulated velocity.
Pose export remains in PS1 axes for the map model builder.

`gen_silver_motion.py` extracts poses/constants from C and initial D_800DD4FC
from SHA1-verified MAP1_S01 data, checking its virtual address and file offset
against the overlay report. The native reference retains the original case
arithmetic. Regression passes 68811 per-frame comparisons across 1024 timestep
values, checking positions, rotations, velocity, floor crossing and ground warp.
Gravity is supplied equally to both implementations; this does not yet verify
the main engine gravity input, event timing or drawing. The motion is now
wired to the gameplay dispatcher and renderer.

The original main piano state machine is now ported in
`sh_school_piano_update`, retaining cases 0..21, the skip jump to 17, reset to
step -1 and the original fallthroughs. It connects the existing puzzle control,
messages, background/fade transitions, border states, both cameras, player warp,
silver motion and impact sound. The scene receives retail normalized stick
values and a Q12 gravity increment from its native adapter. Camera/warp/wait data are generated by
`gen_piano_scene.py` with source Q12 truncation and PS1-to-3DS conversion.
Player warp storage is now per-event; the existing acid regression still passes.

Native school tests directly exercise this state machine at 30/60 Hz for normal
completion, declining inspection, canceling the puzzle and skipping from every
cutscene step 10..18. They verify both cameras, player warp, silver impact/ground
pose, flags, cleanup and restored control. These scene tests inject the solved
controller counter at step 8; the separate controller regression verifies the
actual five-note solution. They do not establish a full in-game puzzle run.
The gameplay dispatcher now calls this state machine for callback 9. The
post-solution callback 10 routes to upstairs PIANO1 inspection rather than
the downstairs clock callback that shares its number.

Dynamic silver drawing selects the unrotated hanging model's existing atlas
ranges and transforms their vertices with the named-model builder's rotation
order, Q12 matrix coefficients and Q8 coordinate floor/translation. It recovers
the exact local integer vertices from the hanging pose before transforming.
The earlier float GPU-matrix approximation has been removed. A reusable linear
vertex buffer is sized from the actual selected atlas ranges; different ranges
occupy separate slices so submitted GPU draws cannot be overwritten. UVs and
existing baked shading are copied unchanged. The map texture/depth/culling
path remains in use, and static ranges rebind their original vertex buffer.
Allocation failure routes to the recoverable loading error. The ground variant
is suppressed while motion is active, preventing duplicate medallions.

`silver_render_regression` finds SILVER_H in real SCFD00.IPD (150 vertices)
and compares 45000 transformed coordinates against separate pose baking over
swing/fall/ground states. All coordinates now match exactly, with zero maximum
error; UVs and colors are also checked for preservation. This verifies agreement
with the existing native model baker, not PS1 pixel equality. Native scene/mask
tests and warning-free 3DS build pass. Actual GPU allocation and submission
of all 150 silver vertices are confirmed in the piano emulator scenario.

The native input adapter applies the source signed-stick deadzone (64) and
D-pad fallback (45), preserving analog priority and opposing-direction handling.
Circle Pad scaling uses the same 156-unit range as existing gameplay. Generated
gravity uses the original H-blank frequency (15780) and scale (10419), converting
native elapsed seconds to that counter. Unit checks cover deadzone boundaries,
opposing inputs and 30/60 Hz gravity. These adapters retain the port's elapsed
time model; they do not prove PS1 scheduling equivalence under low frame rates.

`SchoolPiano` now loads the cumulative pre-puzzle checkpoint, enters through
the actual event trigger, acknowledges the original messages, plays all five
notes using directional input, observes flag 75 and actual moving-mesh submission,
then acquires the dropped silver through its retail trigger and restores gameplay.
This scenario passes on the emulator configured as Old 3DS. Separate decline,
cancel and skip scenarios also pass: refusal/cancel leave flag 75 clear and no
silver motion; START reaches original step 17, then the silver pickup remains
available and completes. No hardware run, visual PS1 comparison or full
game/story completion is implied by these checks.
