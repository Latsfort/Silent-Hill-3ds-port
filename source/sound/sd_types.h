#pragma once

#include "sd_compat.h"

typedef struct SdAudioFile {
    u8 type;
    u16 vagOff;
    u32 size;
    const char *name;
} SdAudioFile;
