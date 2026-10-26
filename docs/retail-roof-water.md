# Roof water initialization

`roof_water.c` now groups the effect lifetime for the valve callback:
prepare applies the original case-4 assignments without clearing untouched
fields; begin follows case 8 (water, spray0, spray1, conditional spray2);
stop follows case 11 and discards cached drawing data. Update generates the
continuous surfaces before the ascending particle pass, retaining the source
RNG order and restart visibility. The module owns one original-size pool,
generated bounded surface storage and its draw mask. It must be initialized
in place because its pool points into its own particle array.

The lifecycle regression checks 2880 complete frames over both circuits,
repeated start/stop, and a pool containing unrelated occupied slots. It
compares particles, cursor, flags, surface quads, visibility, scroll state and
final RNG state against explicit dispatch of the original component functions.
Stopped frames preserve unrelated particles, scroll state and RNG. This
provides the effect module for scene integration; it does not enable or
complete the valve event, and has not run on a real 3DS.

`sh_water_particles_step` now performs the original ascending-index particle
pass for types 27/28 and records each update's draw decision in a separate
300-byte visibility mask. Both GPU mesh builders accept that mask; restarted
particles remain in the simulation but are omitted from that frame's draw.
The raw 20-byte particle layout and existing pool ownership are unchanged.
This pass excludes `func_9C` (continuous water surfaces and scroll timers),
which precedes the particle loop in the original.

`gen_water_surface.py` now extracts that `sharedFunc_800CCE60_1_s02` path
before projection. It retains source loop/branch order, per-edge lighting,
terminal taper, overlap fades, texture spans and post-draw scroll updates.
`sh_water_surface` returns world-Q12 quads with individual vertex shades and
the original UV/CLUT/page. Output truncation leaves all scroll timers advancing;
invalid stream dimensions are rejected before the original fixed-size scratch
arrays could overflow. The original duplicate start row at scroll zero and
the function-scoped final-span variable are retained.

Unlike particles, continuous strips use CLUT78 rather than CLUT14 (same
page45). Their texture/lighting rendering path therefore needs a separate
palette treatment; the particle atlas cannot be reused blindly.
The texture generator now resolves their U64..95/V64..127 source rectangle
and second CLUT row from the original packed UV code. All 16 palette entries
have STP, including nonzero entry zero. `sh_water_surface_decode` preserves
that entry as visible and tiles the 32x64 RGBA8 rectangle (8 KiB); it does not
apply a uniform shade to the varying vertex intensities. The native texture
test compares all 2048 source texels to raw TIM indices and palette bytes,
checks entry-zero opacity and output guards. GPU Gouraud modulation and
rendered equivalence remain unverified and are not connected to a scene.

`sh_water_surface_build_mesh` converts those quads to native triangles while
preserving the original Q12-to-Q8 camera-relative signed-vector truncation.
Each vertex retains its own original shade byte, not a uniform shaded atlas
slot. The eventual GPU path must apply 2x texture modulation (PS1 neutral
shade is 128); exact interpolation/5-bit output equivalence remains unproven.
The surface regression now checks mesh coordinates, four-corner UV mapping,
individual RGB values, alpha, complete triangles and output guards for all
75,505 generated bands. This helper does not advance the scroll counters;
the surface simulation must run once per frame before the particle pass.

The GPU path now uploads the 8 KiB surface texture with the particle atlases
and allocates a separate 45-quad vertex buffer. That bound is generated from
the original four-entry stream arrays (one terminal entry) and 16 scratch
rows. Shutdown, NULL reset and failed upload release all surface resources.
The HUD can supply surface quads for drawing before ribbons/spray, matching
the source pass order. Source byte shades are modulated with a 2x TEV RGB
scale, restored immediately after this draw; alpha scaling remains unchanged.
PICA normalized-byte modulation and interpolation have not been demonstrated
pixel-equivalent to PS1 /128 modulation and 5-bit truncation. This is a
compiled rendering adapter, not verified pixel-accurate output. No scene
currently supplies the surface data, and no hardware test has been performed.
The native surface regression checks 75,505 bands across both circuits and
three deltas, including source stream heights, UVs, scroll wrap, clipped
output, zero-capacity output and guards. GPU projection/drawing and scene
integration remain pending; no hardware verification was performed.

The native frame regression compares 1,080,000 particle updates with explicit
per-slot source-order dispatch, including final RNG seeds and 63,909 omitted
restart frames. Invalid pool size is rejected before mutation; an empty pass
clears the visibility mask. Both mesh regressions also verify that the mask
suppresses otherwise visible geometry. Full valve event integration remains
pending, and this has not been executed on hardware.

`gen_water_ribbon.py` extracts the geometry after movement in
`sharedFunc_800CBDA8_1_s02`. `sh_water_ribbon` returns the four original signed
Q8 camera-relative corner vectors, unmodulated shade, clipped texture span
and packed variant. The source scratch fields retain their original widths.
Horizontal stream tangent/width calculations, vertical camera-facing width,
height interpolation, bend overlap fade and terminal-stream taper are all
retained. Movement is not repeated in this helper; callers must omit a
particle when its preceding step returns false.

The native ribbon regression covers 320,464 valid progress/variant states in
both roof circuits. It checks camera translation, original height formulas,
vertical corner pairing, texture spans, shade bounds and particle immutability.
This verifies preprojection geometry; integration with the full valve callback
remains unfinished.

The texture generator now also resolves the ribbon UV packing from
`particle_water.c`: four 32x64 regions at U64/96 and V0/64, with the same
CLUT/page as spray. The decoder shares the existing exact 5-bit shaded
palettes and tiling code. Its separate 256x1024 RGBA8 atlas would require
1 MiB when uploaded.
The native ribbon texture test compares 2,097,152 texels to raw TIM
nibbles/CLUT entries, checks unused tiles and rejects undersized output.
The existing 2,097,152 spray comparisons also pass with the shared decoder.

`sh_water_ribbon_build_mesh` now converts the source Q8 camera-relative
corners into native triangles. It retains the original average-depth limit
and fourth-corner screen bounds (generated from the source), shade modulation,
texture clipping and variant selection. Projection uses the port camera
rather than GTE emulation; a nonpositive fourth-corner depth is rejected
before division. Exact GTE rounding and near-plane edge equivalence have not
been established.

The native mesh regression checks 18,856 progress/variant cases against the
ribbon helper, including native coordinate conversion, exact atlas endpoints,
buffer guards and behind-camera/distance rejection. It does not verify GPU
appearance. `sh_gfx_set_water_atlas` now uploads both atlases lazily (2 MiB
combined), and frees both on NULL or failure. Ribbons and spray occupy
separate ranges of the same 300-particle vertex buffer, so the second draw
does not overwrite vertices queued for the first. No game scene invokes this
path yet; hardware memory availability and rendering remain untested.

`gen_roof_water.py` extracts the original particle/configuration types,
retaining their signed widths and overlapping unions. It adapts the water
configuration assignments from DrainageValveInteract case 4, including both
ball/no-ball branches and the terminal stream settings. Fields not written
by that case remain untouched.

Particle initialization is adapted directly from
`sharedFunc_800CBA4C_1_s02`, using the existing retail RNG/fixed maths.
Random call order, stream selection, initial positions, progress, velocity
and packed variant bytes remain in source order. Allocation/type ownership
is left to the caller, as in the source.

The native test compares 24,576 initialized particles byte-for-byte with an
independent seeded reference, and checks final RNG state, both circuits,
the original 20-byte union layout and preservation of untouched fields.
Generator checks and the 3DS build pass without warnings.

The generator now also extracts the movement portions of
`sharedFunc_800CBDA8_1_s02` before projection: horizontal progress and endpoint
clipping, vertical gravity/speed cap, and restart at the next stream. The
original scratch sine/cosine values are replaced with the same fixed maths
used to prepare them in `bodyprog_effects_8005E0DC.c`. The API accepts original
Q12 delta and gravity inputs; it does not invent a new simulation clock.
Its false return identifies a restarted particle, preserving the source's
decision to omit that frame's projection.

An additional 768,000 full particle-state comparisons cover both circuits,
four deltas and varying gravity, including 6,748 stream transitions. Particle
bytes and final RNG state agree with an independent reference on every step.

Pool begin/stop now adapt `sharedFunc_800CBC98_1_s02` and
`sharedFunc_800CBD58_1_s02`, using the original circular free-slot search from
`func_8005E7E0`. Begin leaves occupied entries intact, stops allocating when
full, resets active stream timers and sets the water-effect bit. Stop clears
only particle kinds 27/28 and that bit; it preserves the allocation cursor,
other effect bits and all other payload bytes. The API binds caller-owned
storage rather than clearing or allocating a replacement particle group.

Spray configuration/type and `func_800CDCE0`/`func_800CE0CC` initialization
are now adapted directly from MAP1_S03 as well. All five source spawn shapes
are retained, along with inclusive random ranges, the eight-attempt disk
fallback, packed color/variant bytes and age preservation on restart.
`func_800CE164`'s pre-projection age/restart logic is ported separately from
the renderer. Spray allocations share the same circular pool (kind 28).

The spray regression compares 20,480 spawns and 1,024,000 lifespan updates
byte-for-byte, including final RNG state. Starting the source water and spray
groups together uses 160 particles without the ball and 290 with it, fitting
the original 300-entry pool; cleanup frees both kinds.

The spray's post-projection rectangle calculations from `func_800CE164` are
now exposed as `sh_water_spray_quad`: age-dependent size, projected depth and
pitch scaling, bottom-anchored corners, the four packed UV variants, CLUT/page
and illumination-modulated triangular fade. It accepts the original projected
screen/depth and lighting inputs (positive depth required), preserving integer
rounding including the falling fade's upward rounding. A 1,416,192-case native
test compares corners, shade, UVs and texture state against independent
integer formulas. This does not yet issue GPU draws.

`gen_water_texture.py` resolves the original Water effect loading branch to
WATER.TIM/D_800A908C, verifies its page/CLUT against the spray's packed UVs,
and checks the packaged TIM against the ROM extraction. Its four 64x32
variants use the first WATER palette. All 256 vertex shades are represented
with exact 5-bit modulation and saturation. Deduplicating identical palettes
produces 24 slots in a 512x512 RGBA atlas (1 MiB), without changing texels.
`water_texture.c` decodes that atlas in 3DS tiled order.

The native texture test compares 2,097,152 shade/variant texels against raw
TIM nibbles and palette entries, plus unused padding and the buffer-size
guard. Actual GPU allocation, drawing, blending and visual verification are
still pending; the 1 MiB requirement has not been tested on hardware.

4,610 native pool cases cover all occupancy masks of an eight-slot group,
all wraparound cursors, both circuits and the original 300-slot capacity.
They verify particle bytes, RNG consumption, untouched timers, unrelated
effects and selective cleanup.

This is not the complete water effect: binding to the game's shared pool,
water strips/spray rendering, valve/hand coupling, key movement and scene
integration remain pending. No emulator or physical-3DS test is claimed.
# Spray mesh adapter

`water_mesh.c` converts the rectangles from `func_800CE164` to camera-facing
triangles for the existing 3DS vertex format. It reuses
`sh_env_light_intensity` (`func_80055D78`), including the signed axis planes,
ambient contribution and source brightness cap. The atlas slot selects the
prequantized PS1 texture modulation for the resulting byte shade.

This adapter uses the port's floating-point camera projection, not a GTE
emulation. Screen coordinates and depth are quantized before the original
rectangle helper; its bottom anchor and original inclusive UV endpoints are
preserved. Nonpositive camera depth is omitted to avoid division by zero.
No smoke-specific distance or screen culling was copied into this path.

`water_mesh_regression.c` checks 21,504 combinations of camera heading/pitch,
stream, texture variant and ambient intensity. It projects emitted triangles
back to the expected source rectangles, checks atlas endpoints, rejects
particles behind the camera and verifies capacity guards. This is a native
geometry test, not a rendered-image or hardware test.

GPU upload/drawing is exposed by `sh_gfx_set_water_atlas` and the water fields
of `ShMapHud`. The atlas is allocated lazily (512x512 RGBA8) and the vertex
buffer is bounded by the original 300-particle pool. Passing NULL releases
both allocations; failed uploads also release them. The drawing helper is
shared with smoke and retains its additive blend, alpha test, no depth writes
and state restoration. No texture or particles are enabled merely by entering
a map: valve-scene loading and lifetime ownership remain to be connected.
This GPU path has compiled but has not been run in an emulator or on hardware.

Valve-scene integration remains incomplete. The scene
caller must omit a particle on the update where `sh_water_spray_step` returns
false, matching the original restart path that skips drawing that frame.

