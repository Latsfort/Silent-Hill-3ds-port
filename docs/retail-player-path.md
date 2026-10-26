# Scripted player paths

`tools/gen_player_path.py` adapts `src/maps/characters/player.c`
`Player_PathWaypointExecute` directly. Cases 0 through 8, angle ties,
Q8 distance thresholds, waypoint ordering and completion remain in source order.
The unused distance read after advancing beyond the final waypoint is guarded.
State assignments use the `Player_ExtraStateSet` resets of both model state and
control counters, and upper/lower body states.

`tools/gen_path_locomotion.py` extracts the walk/turn cases, including speed,
signed yaw increments, model animation status setup, cutscene-state transitions
and animation-reset requests from `sharedFunc_800CDAA8_0_s02.h`. The caller must
supply the actual animation keyframe. This helper does not implement animation
playback, footsteps, gravity or collision.

`tools/gen_path_animation_data.py` now extracts the six walk/turn blend and
loop definitions from `HARRY_BASE_ANIM_INFOS`, and the walk cadence branches
from `Player_VariableAnimDurationGet`. That cadence depends on controller
inputs, not directly on movement speed. `sh_path_animation_timing` advances
both models through the original blend/loop timing and exposes their pose
samples. It also includes the arrival blend and non-looping playback, with
the initial status resolved from the SHA1-verified MAP1_S02 overlay mapping.
`sh_path_arrival_start` applies the corresponding model setup. The native
test advances to the last pose at four timesteps and checks that it stays
there for another 100 updates. Idle playback remains pending. The route regression now supplies actual
animation keyframes from this playback to the locomotion calculation.

Native verification covers all 4096 final headings, both turn directions,
waypoint advancement, completion, and a flat-ground integration using the
existing PsyQ-compatible sine/cosine functions at Q12 timesteps 68/136/204/272.
Both generators support `--check`. The 3DS build passes without warnings.

The helpers are not yet connected to the alternative-school arrival scene.
The timing adapter now also resolves school states 94/95 from the original
overlay and loads their blend/once clips, including the signed reverse rate.
`sh_path_school_animation_done` follows `Player_AnimPlaybackStateGet`: a blend
is not completion, and a reverse clip ends at its start keyframe. Native tests
advance each clip at four timesteps and verify the final pose is held for
another 100 updates. The insertion callback now uses this adapter for both
animation waits and exposes its current keyframe to the school renderer.
Pending: skeleton pose interpolation, idle playback, original collision/ground handling,
camera movement, voice message sequencing, environment changes and scene
completion. No emulator scene run or physical 3DS verification is claimed.

The scripted camera entry point `sh_vc_script_update` now accepts separate
camera/watch warp commands and movement parameters. Zero parameter values
select the source defaults, generated as exact Q12 values by
`tools/gen_script_camera_data.py`. It reuses the existing native floating-point
VC movement calculations; the gameplay camera retains its previous constants
and call path. Native tests verify coordinate conversion, default resolution,
separate warps, finite output and reduced speed limits at 30/60 fps. These
checks do not establish frame-exact equality with the original fixed-point
camera. The entry point remains to be connected to the arrival callback.
