# Alternative school upper floor (MAP1_S03)

Map points/events/spawns, common pickups and camera roads use the existing
generators. `gen_school_roof.py` extracts the exact US messages, reads the
room grid from the SHA1-verified original overlay and adapts the original
switches for both books. Its `--check` compares generated data, including
every camera road value, against the current decompiled sources.

Supported callbacks are common doors/ReadMessage, both drain inspections
(6/7), book callbacks 9/10 and RubberBallUse (15). The latter retains original cases 0 through 10/default:
crouch, black fades, positional sound 1461, flag 111, BALLHOLE, message 36,
return to the world, then standing animation. The source retains the ball in
inventory. The two original item triggers (points 19/20) are unchanged.
The Monster Lurks preserves its first-read introduction, .6-second pause,
chapter pages, image/fades and final flag 116. The fairy tale preserves its
pages and the flag-117 branch that omits the extra childhood recollection on
repeat reads. Both retain the source case numbers and fallthroughs.

Native school regression passes first/repeat reads of both books at 30/60
fps, checking each A-confirmed page, cancel behavior, image selection,
final flags and restored control. The 3DS build passes without warnings.
RubberBallUse passes both trigger points at 30/60 fps, checking animation
waits, the one sound, A confirmation, retained item and final flag/control.
The generator proves the 59/60 overlay mappings and clip definitions match
MAP1_S02 before sharing playback code. It also checks the packaged original
HB_M1S03 bytes; all crouch/stand poses bake with HERO in the native regression.
Background images are exposed only on the source steps that draw them, so
the world is visible again during the return fade and standing animation.
The drain inspection switches are adapted directly, including source point
and activation-type branches. Images and messages depend on flags 111/113/114;
inspecting the hanging key before drainage sets flag 112, and the original
item-use branches set flag 111. A 256-case native matrix checks callbacks 6/7,
points 17-20, all eight flag combinations and button/item activation at
30/60 fps. It verifies image/message selection, both animations, waiting for A,
restored control and byte-exact expected save changes. Callback 6 is tested
directly where no original trigger references it; no extra trigger is created.

This does not enable complete MAP1_S03 gameplay routing. Roof drainage,
water/ball/key motion, lockers, other pickups, transitions and world objects
remain to be connected/ported. No complete emulator traversal or physical
3DS test is claimed for this floor.
# Roof valve and ball props

Locker player states85/86 are now extracted alongside the valve animation
bank by `gen_roof_animation.py`. State85 blends into1008..1019 at12fps;
state86 blends to1027 then plays backward to1020 at-1fps. The latter's
field_38 mapping contains a different keyframe index1031: that is retained
as metadata, not substituted for the AnimInfo endpoint. The generator checks
that `func_8007FB94` reads only mapping statuses, while
`Player_AnimPlaybackStateGet` uses the actual AnimInfo endpoints.

Existing native animation tests now cover both locker states at four deltas,
reverse playback and terminal hold; all frames in their source clips bake
finite geometry from the packaged HB_M1S03.ANM. This does not yet enable the
unopened-locker scene or its animated world-object dependencies. No rendered
or hardware verification of these animations has been performed.

OpenedCatLockerInspect (callback11) now uses the existing source-derived
background inspection adapter with LOCKER1.TIM, message43 and both original
2.5/s fade rates. `gen_roof_locker.py` extracts the callback arguments and
verifies the packaged TIM against the raw dump. Native tests cover the
actual point76 event requiring103, image/brightness, fade rate, one-second
input hold, control restoration, unchanged locker flags and persistent valve
pose at30/60fps. No new hint or replacement scene is introduced.

UnopenedCatLockerInspect and LockerDeadBodyCutscene remain unported. Their
retail switches depend on flags103/106 supplied by animated world objects;
those flags must not be synthesized to bypass the opening/body sequence.
Rendered appearance and physical-3DS execution of this inspection are untested.

LibraryReserveKeyItemTake (callback14) is now adapted with the original
0..6/default switch: freeze/queue model, stop, animation59, await model,
prompt44/flag108, accept-add, animation60 and restore control. Declining
clears108 in the original case4 branch and does not add the item. It reuses
the existing pickup model animation/message adapter and precise school
animation timing, preserving the retail frame ranges already checked for
HB_M1S03. `gen_roof_pickup.py` extracts item69's US name and inventory
rotations and verifies packaged UNQ45.TMD against the raw dump; the asset
preparation script and game/inventory tables include it.

The native school regression covers accept/refuse at30/60fps, input waiting,
both animation poses, no premature flag, one item on accept, restored control
and eligibility to retry after refusal. The original trigger at point82
requires106. The locker/body sequence that supplies that prerequisite, the
full alternative-school route and actual rendered/hardware pickup remain
unfinished or unverified. Earlier notes below referring to an unported key
pickup are superseded by this addition.

The LibraryReserveKeyUse and ClassroomKeyUse callbacks (3/4) now reuse the
existing message-with-sound adapter. `gen_roof_keys.py` extracts their items,
exact messages37/38, UseKey sound1335 and the explicit library mark1311.
Item removal is invoked on each callback tick, matching the original, with
saturating inventory counts. For these callbacks, transition flags apply
the event completion mark before the callback as in
`SysState_EventCallback_Update`; the library's explicit mark is written
after the message/sound call. The message waits for input and restores
control through the existing adapter.

The school regression tests all four original table entries (button/item
activation for each key) at30/60fps, sound position and one-time playback,
item removal across callback invocations, flags, waiting and completion.
These are direct callback integration checks; the full alternative-school
route, library key pickup/locker sequence and hardware behavior remain
unfinished or unverified.

`gen_roof_props.py` extracts `VALVE_HI` and `BALL_HID` placements from
`Map_WorldObjectsInit` and the first world-object update branch. The roof
prop getter now exposes those two models instead of falling back to normal
school boiler props. The visibility mask retains both original signed chunk
tests and flag111 for the ball. Positions/rotations use source Q12 conversion.
The school regression checks 206,082 grid/flag visibility cases, including
chunk boundaries, alongside the existing school event tests.

The game loader and HUD now include this roof map in their prop upload and
visibility paths. A valve-motion adapter reuses the existing moving-object
buffer and transforms the valve around its generated pivot. It preserves
`WorldObjects_Add`'s X-angle Q12-to-Q10-to-Q12 truncation and world position
Q12-to-Q8 shift, recovering local Q8 vertices from the existing static bake.
The native valve regression covers all 4096 angles, the original fractional
pivot, in-place transforms and unchanged UV/color payloads. GPU buffer
preparation is wired for the eventual active valve event.

This covers these two props only. No callback sets valve_active yet: the
other upper school world objects and complete valve-event integration remain unfinished.
Neither loading nor appearance of these props has been verified on hardware.

