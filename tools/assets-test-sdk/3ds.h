#pragma once
#include <stdint.h>

typedef int32_t Result;
typedef uint32_t Handle;
typedef struct FS_Path { unsigned type; const void *data; } FS_Path;
#define ARCHIVE_SDMC 9
#define PATH_EMPTY 1
#define PATH_ASCII 3
#define FS_OPEN_READ 1
#define R_FAILED(r) ((r) < 0)
#define R_SUCCEEDED(r) ((r) >= 0)
static inline FS_Path fsMakePath(unsigned type, const void *data) { return (FS_Path){type, data}; }
Result FSUSER_OpenFileDirectly(Handle *, unsigned, FS_Path, FS_Path, unsigned, unsigned);
Result FSFILE_Read(Handle, uint32_t *, uint64_t, void *, uint32_t);
Result FSFILE_GetSize(Handle, uint64_t *);
Result FSFILE_Close(Handle);
Result romfsMountFromFile(Handle, uint32_t, const char *);
Result romfsInit(void);
Result romfsExit(void);
