#pragma once

#include "sd_compat.h"
#include "sd_cd.h"

typedef void (*SdLockFn)(void);

extern u32 g_sd_vsync_count;

void sd_port_init(void);
void sd_port_set_roots(const char *snd_root, const char *xa_root);
/* Synchronous load of g_SdAudioFiles[idx]; call outside the SPU lock. */
bool sd_port_load_file(s32 idx);
/* Runs pending file / XA clip reads requested by the state machines. */
void sd_port_service(SdLockFn lock, SdLockFn unlock);
void sd_port_free_files(void);
bool sd_port_xa_playing(void);
