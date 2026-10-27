#pragma once

#include "sh3ds/savegame.h"

typedef enum ShSavefileResult {
    SH_SAVEFILE_OK,
    SH_SAVEFILE_MISSING,
    SH_SAVEFILE_INVALID,
    SH_SAVEFILE_IO_ERROR
} ShSavefileResult;

ShSavefileResult sh_savefile_read(const char *path, ShAutosave *snapshot);
ShSavefileResult sh_savefile_write(const char *path, const ShAutosave *snapshot);
