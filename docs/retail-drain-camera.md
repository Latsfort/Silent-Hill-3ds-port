# Roof valve camera

Current integration status: callback8 is enabled through the existing retail
map event (point21, requires112, completes113). The original switch, animation,
hand-driven valve/sounds, water module, scripted camera/light, player visibility
and control restoration are connected. Valve pose survives another interaction
on this overlay and resets on overlay entry; hand/position seeds survive event
initialization. WATER.TIM uploads on overlay entry, matching source effect
loading, and its atlases remain resident until leaving the overlay or title.

`roof_valve_scene_regression.c` now follows every event step from the real
entry point to completion, baking HERO/HB_BASE/HB_M1S03 poses each frame and
feeding the actual decoded right hand. Both circuits pass at 30/60fps:
579/768 frames at30fps and1138/1522 at60fps. Each accepted run produces two
squeaks, six clicks, one loop start and one stop. The no-ball message waits
for input; only the ball branch sets113. Both restore control, camera and
visible Harry, stop effects, and retain the ball inventory item. Further
checks cover pose persistence through another callback and overlay reset.

This is native simulation/asset decoding, not GPU or hardware validation.
Old3DS allocation capacity, visual/audio output, GTE/pixel equivalence,
subframe skeleton interpolation and global flare/reflection rendering remain
unverified or incomplete. The broader alternative-school route and full-game
port are not complete. Incremental notes below describe earlier milestones;
their statements that this trigger is disabled are superseded by this audit.

Frame rendering is now connected while the event is under integration:
MAP1_S03 loads the water atlases when the effect starts, updates the source
surface/particle module once after the player/camera update, and supplies
its quads, configs and visibility to the HUD. It uses the existing generated
H-blank gravity helper. The scripted light override is converted with the
original Custom direction formula and used for world/effect lighting;
case11 restores the torso light. The render path honors the original player
visibility flag and selects the precise 107/108 keyframes.

Effect GPU allocations are released when inactive and on return to title;
the simulation is stopped when leaving the overlay or returning to title so
it cannot leak into a later game. Native integration tests cover prepared
case8 startup (160/290 active particles), one effect frame, case11 cleanup,
flag113 only in the ball branch, closing animation and control restoration.
These tests enter the relevant steps explicitly; they are not an end-to-end
rendered gameplay test. The light direction also aligns with all 71,680
source camera/look samples. The real trigger remains disabled pending the
remaining state-lifetime/entry audit. No hardware, visual or memory-budget
confirmation is claimed.

The continuous 1465 update is now dispatched after the hand block whenever
the source running flag remains set. Distance uses the original player/view
midpoint, source water-origin offsets, Q6 component truncation and fixed
square root before the extracted attenuation formula. The game seeds the
current player position before the school update. The balance point is
extracted from the verified overlay, halved with the source signed shift,
then sent with attenuation and zero pitch through `sh_snd_update`.
Camera balance follows the port's existing floating spatial-audio algorithm
with an explicit current camera; exact `VectorNormal`/matrix equivalence and
monoural configuration handling are not established by this change.

Native tests compare 4096 distance samples and check event dispatch of the
loop attributes, negative half-balance, and absence of updates after stop.
The existing exhaustive attenuation/rotation tests and school tests pass.
Effect frame ownership/rendering and complete event enablement remain pending;
no real audio or hardware playback verification is claimed.

The event dispatch now runs the original post-switch hand block through
`sh_drain_valve_hand`. It uses the current clip's original startKeyframeIdx,
forces relative frame zero for the first two callback calls, and consumes the
seeded world-hand matrix only in steps7/12. The squeak/click source position
is extracted from the SHA1-verified overlay constant referenced by the retail
function. The adapter dispatches 1463/1462 and start/stop1465 according to the
extracted sound-phase logic. Native school tests check those state changes
and sound commands for both animations and first/second sound phases, with
no premature flag113. The actual loop volume update, effect rendering and
remaining frame ownership still need connection before enabling the trigger.

`gen_school_valve.py` now adapts the original event switch (cases 0..12 and
default) into `school_roof_valve.h`, reusing the extracted camera/light,
animations and water components. Original choice destinations, message
indices, warps, wait, visibility toggles, border commands and flag writes
remain in source order. A private dispatch path supports native integration
tests, but `sh_school_begin` still rejects callback8: the public gameplay
trigger is intentionally not enabled while the source's post-switch
hand/sound block and frame-level graphics ownership remain disconnected.

The school regression tests steps0..7 at 30/60fps: map mark1364, action wait,
accept/refuse branching, refusal control restoration, accepted warp/camera,
1.5-second wait, animation107 start, and absence of premature flag113. It
also verifies that the actual map event (point21, requires112, completes113)
remains rejected by the gameplay entry point. Later switch steps are compiled
but are not covered by these new integration assertions. This is not a claim
that the full scene is playable or complete.

`gen_drain_camera.py` verifies the MAP1_S03 overlay SHA1, then extracts six
u16 timeline entries and two arrays of five Q12 vectors. The source's
address-named externs are checked against original MIPS LUI/ADDIU address
references inside `MapEvent_DrainageValveInteract`; table extents follow the
adjacent source declarations.

The sampler adapts case 9's branch selection and integer interpolation
directly. With no ball it ends at the source's fourth boundary; with the
ball it follows all five segments. A false return leaves the previous
camera unchanged, matching the branch that advances the event step.

The native regression compares every Q12 tick against 64-bit rational
interpolation, totaling 71,680 position/look samples across both branches.
It also checks the exact completion boundaries. Generator checks and the
3DS build pass without warnings.

This is a prerequisite, not the complete valve cutscene. Hand-driven valve
rotation/sounds, animations, water/spray particles, key movement, lighting,
visibility changes and event integration remain incomplete. No visual,
emulator or physical-3DS verification of this scene is claimed.
# Valve player animations

`gen_drain_camera.py` also extracts the original case-9 light position and
rotation assignments. `sh_drain_camera_light` preserves the Q6 intermediate
horizontal magnitude, PsyQ fixed atan2 and minimum PS1 Y=-1.5 position clamp.
It does not substitute the native floating camera angles. The 71,680 camera
sample regression now checks these light attributes throughout both branches.
The scene adapter must still select this override, detach the light/flare
bones at the original steps and restore normal flashlight attributes in
case 11. This helper alone does not enable the valve scene or change gameplay.

`gen_roof_animation.py` resolves original states 107/108 through the SHA1-verified
MAP1_S03 mapping, then extracts their four blend/playback entries from
`HARRY_M1S03_ANIM_INFOS`. The original virtual mapping selects clip 60 for
opening (1092 down to 1045 at -8 frames/s), and clip 59 for closing (1047
through 1092 at 8 frames/s). Reverse rate is the signed 32-bit result of
the original Q12 constant, not its apparent positive decimal literal.

`path_animation.c` selects the roof definitions by player state, keeping
MAP1_S02 clips separate even when packed status numbers overlap. It reuses
the existing blend/once timing and terminal-frame detection. Native tests
cover both states at four deltas, matching extra-model timing and holding
the correct terminal frame. All poses in both clips bake finite geometry
from the packaged HB_M1S03.ANM. The generator checks that asset against the
raw dump.

`gen_drain_valve.py` now extracts the hand-driven rotation block directly.
`sh_drain_valve_hand` accepts the original relative animation frame and hand
angle, updates the original-width rotation/opening/phase variables and returns
the exact sound commands for 1463, 1462 and start/stop 1465. The adapter must
supply relative frame zero during the first two callback invocations, as in
the original. Normalization preserves positive half-turn (2048), following
`Math_AngleNormalizeSigned`, rather than wrapping that tie negative.

The native regression compares 1,966,080 state transitions against an
independent reference, covering every signed angle, active/inactive callback
steps, frame-window boundaries, sound phases, running state and clamps.

`sh_drain_valve_hand_angle` now transforms the original hand-local SVECTOR
through a supplied PS1 world bone matrix and applies the original pivot
offsets before fixed-point atan2. Its local point and right-hand bone index
are generated from the SHA1-verified overlay and original Harry enum; no
point was inferred from the model. A further 4096 matrix cases compare the
transform against independent signed floor division. This checks the helper,
not the game's eventual world-bone matrix composition or pose interpolation.

`sh_drain_water_attenuation` is generated from the original final volume
branch. It preserves the one-metre inner region, 17-metre cutoff, intermediate
Q12 division/multiply truncation and final complemented attenuation byte.
The native regression checks 17,768,689 distance/opening combinations,
including the exact boundary ticks. `sh_snd_update` exposes the existing
`sh_sfx_update` path for the eventual per-frame `Sd_SfxAttributesUpdate`
adapter; it does not start or restart a voice.

The game now caches the original right-hand bone immediately after Harry's
pose bake, before another character can replace the shared skeleton cache.
Before the school update on MAP1_S03, it composes that model-space matrix
with the rendered player yaw and Q8 world translation and seeds the school
state. Reloading Harry invalidates the hand cache; missing samples clear the
school validity flag. The native regression compares the composition to a
full matrix-product reference over 4096 angles, including negative fixed
rounding and in-place operation. This follows the port's current integer
keyframe pose; original subframe bone interpolation remains unverified.

The complete valve callback, actual sound playback and
water scene ownership are still pending; this does not enable the event in gameplay.
No hardware or rendered animation verification has been performed.

