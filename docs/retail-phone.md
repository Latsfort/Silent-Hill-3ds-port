# Alternative school phone call

Source: MAP1_S02 MapEvent_CutscenePhoneCall, callback14. Original cases0..28
and default must remain, including skip2..25 to27 and early-skip distinction.
The scene uses event path53 and camera tweens, not a DMS file. Existing
PHONE.PLM loading can be reused; it is not yet connected to this scene.

States97/98 are now extracted by the existing verified ROM animation
generators, reusing the same timing/pose machinery. State97 plays1062..1110
at6fps; state98 plays1119..1147 at10fps. Existing native timing/terminal-hold
and HERO/HB_M1S02 pose tests pass for both. This does not yet apply their
source side effects during the phone scene.

gen_phone_data.py extracts six original pages31..36 with Q12 durations,
J audio type, end and instant-display codes, plus animation event thresholds.
The shared player state sets99 at1065, hides the held item until1090 and
clears99 during state98 at1147. Its secondary switch also plays footstep
events and sound1456 at keyframe1062+26; retain those alongside the callback's
explicit sound1456 rather than deduplicating them.

The call itself remains unfinished: original path, steps, XA53/54 preload/
play/stop, animation/held-item lock and restore, timed/skippable message
rollout, phone props and camera tweens still need integration. Do not replace
J0/J1 pages with arbitrary delays or ordinary A-only dialogue. Refer to
Gfx_MapMsg_Draw and Gfx_MapMsg_SelectionUpdate in map_msg_display.c and the
J parser in text_draw.c. Build passes without warnings; generator comparisons
pass. No rendered phone scene or hardware execution has been verified.

phone.h/c now exposes the generated page records and player97/98 side
effects. Native checks verify exact before/after thresholds for flag99,
held-item visibility and all six page metadata entries. The scene host must
call these effects in original player-control order before animation update.

The placed PHONE3_H/PHONE2_H models are now appended to the generated lower
prop table. gen_phone_world.py adapts their original room gate and!99
visibility; native school checks verify both models appear and disappear
with that flag, alongside the existing scene tests. Build and generator
comparisons pass. This is not the completed phone call: callback14 remains
unavailable, and held PHONE.PLM, timed messages, path/camera and audio still
need integration. No rendered or hardware execution is claimed.

gen_phone_motion.py now extracts the original waypoint and heading and all
five position/watch command pairs (steps5/9/15/16/28). The public motion
adapter reuses the existing retail path executor. The view adapter retains
absolute/player-relative coordinates, independent position/watch warp
booleans and separately advanced tween slots0/3. It emits PS1 Q12 targets
for the existing camera adapter rather than inventing a new camera path.

Native tests combine that path with existing locomotion/animation and Q12
movement to reach the original phone point with final heading-90degrees.
They check step5/9 targets, both6.5s tweens and repeated step16/28 cuts at
68/136/272 Q12 deltas. Prior flag/page checks still pass. Generation compares
and make -j4 pass with no warnings. These are prerequisites only: scene14
is not enabled and no rendered/hardware call has been run.
