# Alternative school coverage

MAP1_S02 points, events, common pickups and VC roads are generated from the
decompilation. The US messages and room grid are generated as well; the room
bytes are read from the SHA1-verified original overlay.

Implemented in the school event module:

- ReadMessage, including continued messages and A confirmation.
- Original common door callbacks.
- Callback 4 (`MapEvent_LobbyKeyUse`): original key removal, one unlocking
  sound, shared message-with-sound wait and message index 19. The current
  decompiled MAP_EVENTS table does not reference this callback; no new trigger
  has been added. Native tests invoke it directly. Its source messages 19/20
  are empty and continue to message 21; this sequence is preserved, rather
  than replaced with a new unlock message.
- Callback 6 (`func_800DA9D4`): GOLD2 inspection and message 21.
- Callback 7 (`func_800DAA00`): SILVER2 inspection and message 22.
- Callback 8 (`func_800DA8F8`): original cases 0/1/2/default for the locked tower.
- Callback 10 (`MapEvent_DoorWithHorizontalSlotInteract`): original steps
  0 through 10/default, RECEPDR1/2 images, first/repeat descriptions and flag
  94 on completion. The image blend runs during step 7's message and retains
  the source fallthrough into step 8. Its per-frame increment is scaled by
  30*dt. Message IDs, image names, flag and blend increment are generated.
- Callback 11 (`func_800DB058`): generated directly from the original switch.
  It retains camera/player placement, border commands, the .2-second wait,
  animation 94, sound 1454, message 30, sound 1343, reverse animation 95,
  fade-out, control/hand restoration and flag 1207. The original does not
  remove the Picture card. Both animation waits use original blend/once
  timing and playback-end detection, seeded from Harry's current baked pose.
- Callbacks 12/13: Picture card and Rubber ball use the existing common
  `Event_ItemTake` adapter, with generated source item/message/flag data.
  Their names, pickup rotations and model filenames are derived from the
  original item tables and loading switch; packaged model bytes match ROM.
- Callback 16 (`func_800DBF88`): LVTRYMSG book image, flag 97 set before
  display, and continued messages 38/39.
- Callback 17 (`func_800DBFC8`): shotgun pickup using the existing pickup
  state switch and source MAP1_S02 animation 59/60 playback. Prompt/flag data,
  the count 6 and UNQA2 model/rotation/name come from the original tables.
  Accept stores the original weapon item count (loaded ammunition), not six
  separate gun pickups. Refusal clears flag 95. Its event still requires flag
  96; no new trigger or bypass is introduced.
- Callback 20 (`MapEvent_ClassroomKeyItemTake`): the same source crouch/stand
  pickup sequence for one Classroom key, message 51 and flag 114. Its event
  still requires `EventFlag_M1S03_KeyDownTheDrain`. UNQ46, item name and pickup
  rotation are sourced and byte-verified with the other alternative items.
  Counts for both animated pickups are generated per callback.

Native school regression checks the three new interactions at 30/60 fps,
including waiting without input, cancel not dismissing the text, image/text
selection, control restoration and byte-identical save state afterward.
The generator records inspection wrapper flags separately and rejects flag
wrappers with unsupported side effects. The book regression checks the first
update's flag change, both A confirmations, cancel behavior, restored control,
and that flag 97 is the only resulting save change, at 30/60 fps.
The two pickups are checked for accept/decline at 30/60 fps, including the
temporary prompt flag, inventory count and the ability to return after refusal.
The slot inspection is checked on first/repeat visits at 30/60 fps, including
all continued messages, waiting during the blend, cancel not advancing,
late flag assignment, image cleanup and return of control.

This does not enable full MAP1_S02 gameplay routing. The arrival, other scenes,
puzzles, world objects, enemy behavior and exits remain incomplete. Scripted
path/animation/camera prerequisites are described in retail-player-path.md.
The school keyframe adapter also supports source states 94/95 for the card
scene. State 95 uses the source's signed Q12 rate (-8), playing from frame
1037 back to 1032; its mapping time field is not mistaken for the clip's
playback start. All poses of both clips bake against HERO/HB_M1S02 in the
native animation regression, with selection direction and final hold checked.
The card callback has a native 30/60 fps regression covering its original
item trigger, warp/camera/FOV, both animations, positional sounds, A-only
message wait, no START skip, retained item/weapon, final flag and cleanup.
Skeleton interpolation and visual equality to PS1 remain unproven; the native
character rendering still consumes integer keyframe samples.
The shotgun regression covers its required flag, both choices, crouch/stand
animations, prompt visibility, exact item count, pickup statistics and
availability after refusal at 30/60 fps. Both pickup clips also pass the
native HERO/HB_M1S02 pose regression. Weapon combat/equipment behavior is
outside this pickup verification.
The same regression also covers the Classroom key's required flag, both
choices, exact quantity of one and pickup statistics at 30/60 fps.
Build verification passes without warnings. These additions have not been
run as a complete emulator scene or on physical 3DS hardware.
