#include "sh3ds/assets.h"

#include <3ds.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void sh_assets_log(const char *format, ...)
{
    FILE *file = fopen("sdmc:/sh3ds_load.log", "a");
    if (!file) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
}

static bool resources_readable(void)
{
    const char *paths[] = {"romfs:/game/bg/THR0000.IPD", "romfs:/game/bg/SC0000.IPD"};
    for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        FILE *file = fopen(paths[i], "rb");
        if (!file) {
            sh_assets_log("unreadable %s", paths[i]);
            return false;
        }
        const int byte = fgetc(file);
        fclose(file);
        if (byte == EOF) {
            sh_assets_log("empty %s", paths[i]);
            return false;
        }
    }
    return true;
}

static uint32_t read_u32(const uint8_t *p)
{
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool mount_sd_file(const char *path)
{
    char normalized[1024];
    if (!path || !*path) {
        return false;
    }
    if (strncmp(path, "sdmc:", 5) == 0) {
        path += 5;
    } else if (strncmp(path, "3dslink:/", 9) == 0) {
        const int length = snprintf(normalized, sizeof(normalized), "/3ds%s", path + 8);
        if (length < 0 || (size_t)length >= sizeof(normalized)) {
            return false;
        }
        path = normalized;
    } else if (*path != '/') {
        const int length = snprintf(normalized, sizeof(normalized), "/%s", path);
        if (length < 0 || (size_t)length >= sizeof(normalized)) {
            return false;
        }
        path = normalized;
    }
    Handle file;
    Result result = FSUSER_OpenFileDirectly(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                                            fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0);
    sh_assets_log("open %s result=%08lX", path, (unsigned long)(uint32_t)result);
    if (R_FAILED(result)) {
        return false;
    }
    uint8_t header[44];
    uint32_t count = 0;
    uint64_t file_size = 0;
    result = FSFILE_Read(file, &count, 0, header, sizeof(header));
    if (R_FAILED(result) || count != sizeof(header) || read_u32(header) != 0x58534433u ||
        (header[4] | ((unsigned)header[5] << 8)) < sizeof(header) ||
        R_FAILED(FSFILE_GetSize(file, &file_size)) || read_u32(header + 40) < sizeof(header) ||
        (uint64_t)read_u32(header + 40) + 40u > file_size) {
        sh_assets_log("invalid or incomplete 3dsx %s", path);
        FSFILE_Close(file);
        return false;
    }
    /* libctru romfsMountFromFile owns the file handle on success and failure. */
    result = romfsMountFromFile(file, read_u32(header + 40), "romfs");
    sh_assets_log("mount result=%08lX size=%llu", (unsigned long)(uint32_t)result,
                  (unsigned long long)file_size);
    if (R_FAILED(result)) {
        return false;
    }
    if (resources_readable()) {
        return true;
    }
    romfsExit();
    return false;
}

bool sh_assets_init(int argc, char *argv[])
{
    FILE *file = fopen("sdmc:/sh3ds_load.log", "w");
    if (file) {
        fprintf(file, "Silent Hill 3DS %s %s\n", __DATE__, __TIME__);
        fclose(file);
    }
    const Result result = romfsInit();
    sh_assets_log("romfsInit=%08lX argc=%d argv0=%s", (unsigned long)(uint32_t)result, argc,
                  argc > 0 && argv && argv[0] ? argv[0] : "(absent)");
    if (R_SUCCEEDED(result)) {
        if (resources_readable()) {
            return true;
        }
        romfsExit();
    }
    if (argc > 0 && argv && mount_sd_file(argv[0])) {
        return true;
    }
    if (mount_sd_file("/3ds/silent-hill-3ds/silent-hill-3ds.3dsx") ||
        mount_sd_file("/3ds/silent-hill-3ds.3dsx") || mount_sd_file("/silent-hill-3ds.3dsx")) {
        return true;
    }
    sh_assets_log("No readable embedded game resources found");
    return false;
}
