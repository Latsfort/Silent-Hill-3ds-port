#pragma once

#include "sd_compat.h"

#define SPU_RATE 44100
#define SPU_RAM_SIZE 0x80000
#define SPU_VOICES 24

/* CD audio input (XA): callback produces `n` interleaved stereo frames at SPU_RATE. */
typedef void (*SpuCdSource)(s16 *dst, int n);

void spu_reset(void);
void spu_set_cd_source(SpuCdSource src);
/* Renders `frames` interleaved stereo samples. Caller holds the sound lock. */
void spu_render(s16 *out, int frames);
u8 *spu_ram(void);
