# Instructions for AI agents - Silent Hill 1 (PS1) port to Nintendo 3DS

Read this whole file before touching code. The goal is a pixel- and timing-accurate port.
The only source of truth is `vendor/silent-hill-decomp` (C decompilation) and the ROM assets in
`romfs/` (`dump/assets` = raw extraction). Never rely on memory of the game.

## 1. Golden rules
1. NEVER invent anything: no texts, hints, enemies, positions, timings, camera boxes, UI markers.
   If the decomp does not give the answer, stop and report it.
2. Look up the retail code BEFORE coding (section 2). Port it step by step, same order.
3. Never hand-copy tables of numbers. Generate them with a script (section 3).
4. Build after every change: `make -j4`. Zero errors AND zero warnings, or you are not done.
5. Do not touch working systems (renderer, VC camera, papermap) unless the task is about them.
6. Small, focused changes. Do not rewrite a whole file to change one function.
7. When done, say exactly what was verified (build) and what was not (not run on hardware).

## 2. Look-up tool (use it, it is fast)
```
python tools/sh_lookup.py func MapEvent_CutsceneExitCafe   # retail code, flags + messages annotated
python tools/sh_lookup.py map map2_s00                     # files + MapEvent_* of a map
python tools/sh_lookup.py events map2_s00 Cafe             # which event fires where (flags, trigger, point)
python tools/sh_lookup.py msg map2_s00 16                  # exact US text of a map message
python tools/sh_lookup.py flag 874  |  flag CofeArrow      # event flag number <-> name
python tools/sh_lookup.py spawns map1_s00                  # enemy spawns
python tools/sh_lookup.py point map2_s00 1                 # one map point
python tools/sh_lookup.py coord 15.54 -1.5 55.64 90        # PS1 -> 3DS coordinates
python tools/sh_lookup.py q12 0x36A                        # fixed point -> float / angle
python tools/sh_lookup.py grep "Event_DisplayMapMsg" --map map2_s00
```
Map names: map0_s00 (opening + alley nightmare), map0_s01 (cafe 5to2), map1_s00 (school),
map2_s00 (Old Silent Hill streets). Unsure? `sh_lookup.py map <map>` lists its events.

## 3. Data generators
- Map points / enemy spawns / events: `python tools/gen_map_data.py MAP source/game/mapdata_MAP.h`
  (C types in `include/sh3ds/mapdata.h`).
- Common street pickups: `python tools/gen_common_pickups.py MAP source/game/map_pickups_MAP.h`.
- VC camera roads: `python tools/gen_vc_roads.py <vc_road_data.h> <out.h> <symbol>`.
- Any other table: write a new generator in `tools/`, then compare its output with the decomp
  using a script (count + values). Generated headers must be ASCII only.

## 4. Conversions (the #1 source of bugs)
| Thing | PS1 (decomp) | 3DS port |
|---|---|---|
| Position | (x, y, z), Y down, feet y=0, head ~ -1.7 | (x, -y, -z) |
| Yaw | `yaw` | `-yaw`; forward = (-sin yaw, -cos yaw) |
| Look-at | target - camera | `yaw = atan2f(-dx, -dz)`, `pitch = atan2f(dy, horiz)` (+ = up) |
| Q12 | `Q12(1.0)` = 4096 | float = v / 4096 |
| Q12 angle | `Q12(0.25)` = 1/4 turn | 90 deg |
| Q8_ANGLE | 256 = 360 deg | deg = v / 256 * 360 |
| Per-frame counter | `x += k` each frame (30 fps) | `x += k * 30.0f * dt` |
| Timer | `Q12(1.5)` seconds | 1.5f seconds with dt |
| Screen fade | `Q12(0.0)` timestep = default | 3.0 / s (~0.33 s); 1.5 / s while a file loads |
| FOV | gameplay projection | 53.13 deg, NO zoom when aiming |

Easy mistakes: sign of Z (mirrored scene), sign of yaw (Harry walks backwards), using the PS1
frame counter as seconds (10x too fast/slow), forgetting Y is down (camera underground).

## 5. Porting an event (`MapEvent_*`)
1. `sh_lookup.py func <Name>` and `events <map> <Name>` (trigger, required/complete flags).
2. Keep the retail `switch (step)` with the SAME case numbers, same fallthroughs, same
   skip button logic (`skip` jumps to the same step as retail).
3. `Event_DisplayMapMsg` = message that waits for A (no auto timeout). Use the exact text.
4. Set the same flags at the same step (`sh_save_event_set(save, N)`).
5. Camera: `Event_CameraPositionSet` / `LookAtSet` -> convert with section 4, FOV 53.13.
6. At the end of a cutscene: give control back, `sh_vc_warp(&vc)`, fade in exactly as retail.
7. Enemies only from the spawn table / retail code, never placed by hand.

## 6. Code layout
- `source/game/game.c` main loop, asset loading (`romfs:/game/tim/*.TIM`), HUD fill.
- `source/game/cafe.c`, `alley.c`, `town.c` one module per area (`sh_<area>_begin/update/...`).
- `source/game/vc.c` retail gameplay camera; `papermap.c` paper map marks; `gfx.c` rendering.
- Save: `sh_save_event_get/set`, `sh_save_item_count`. Sound: `sh_snd_play`, `sh_snd_at`, `sh_bgm_*`.
- Style: C99, 4 spaces, `snake_case`, `sh_` prefix for public API, `static` otherwise.
  Comments only to cite the retail origin (`/* ExitCafe case 7 */`) or a non-obvious constraint.

## 7. Checklist before saying "done"
- [ ] Every value traced to a decomp line (no guess).
- [ ] Coordinates/angles converted with section 4.
- [ ] Timers use `dt`, per-frame values multiplied by 30.
- [ ] `make -j4` passes with zero warnings.
- [ ] No leftover debug text, no invented subtitle/hint.
- [ ] Report what is untested.
