#pragma once

/* libcd/libetc subset used by sd_call.c. Disc reads are served from romfs (sd_port.c). */

#include "sd_compat.h"

typedef struct {
    u8 minute;
    u8 second;
    u8 sector;
    u8 track;
} CdlLOC;

typedef struct {
    u8 file;
    u8 chan;
    u16 pad;
} CdlFILTER;

typedef struct {
    u8 val0;
    u8 val1;
    u8 val2;
    u8 val3;
} CdlATV;

#define CdlNop 0x01
#define CdlSetloc 0x02
#define CdlReadN 0x06
#define CdlPause 0x09
#define CdlSetfilter 0x0D
#define CdlSetmode 0x0E
#define CdlSeekL 0x15

#define CdlModeSpeed 0x80
#define CdlModeRT 0x40
#define CdlModeSF 0x08

#define CdlComplete 2
#define SyncMode_Count (-1)
#define SyncMode_Wait3 3

s32 CdSync(s32 mode, u8 *result);
s32 CdControl(u8 com, u8 *param, u8 *result);
s32 CdControlB(u8 com, u8 *param, u8 *result);
s32 CdReset(s32 mode);

#define itob(i) ((u8)((i) / 10 * 16 + (i) % 10))

/* Port hooks. */
u32 VSync(s32 mode);
void CdMix(CdlATV *vol);
/* Returns the romfs copy of g_AudioData[idx], or NULL while it is being loaded. */
const u8 *sd_file_get(s32 idx);
/* Selects the gSDXATable entry streamed by the next CdlSeekL/CdlReadN. */
void sd_xa_select(u16 xaIdx);
