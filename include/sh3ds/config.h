#pragma once

/* Old 3DS first: conservative budgets for ~128 MiB system RAM. */
#ifndef SH3DS_TARGET_OLD3DS
#define SH3DS_TARGET_OLD3DS 1
#endif

#ifndef SH3DS_DEVELOPMENT
#define SH3DS_DEVELOPMENT 0
#endif

/* Static pools for game/runtime buffers (not GPU memory). */
#define SH3DS_FRAME_ARENA_BYTES (1u * 1024u * 1024u)  /* scratch, reset every update */
#define SH3DS_STREAM_ARENA_BYTES (5u * 1024u * 1024u) /* map window rebuilt over several frames */
#define SH3DS_LEVEL_ARENA_BYTES (10u * 1024u * 1024u) /* files of the loaded area or texture */

/* Upper-screen logical resolution (Citro2D/Citro3D). */
#define SH3DS_TOP_WIDTH  400
#define SH3DS_TOP_HEIGHT 240
#define SH3DS_BOT_WIDTH  320
#define SH3DS_BOT_HEIGHT 240

#define SH3DS_VERSION_MAJOR 0
#define SH3DS_VERSION_MINOR 1
#define SH3DS_VERSION_PATCH 0
