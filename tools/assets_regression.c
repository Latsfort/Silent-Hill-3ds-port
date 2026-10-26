#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <3ds.h>

static bool mounted, fallback_resources, primary_resources;
static int primary_result, broken_header, fail_mount, opens, closes, mount_calls;
static FILE *sd_file;
static const char *accepted_path;

static FILE *test_fopen(const char *path, const char *mode)
{
    (void)mode;
    if (strncmp(path, "romfs:/", 7) || !mounted || !(fallback_resources || primary_resources)) {
        return NULL;
    }
    FILE *file = tmpfile();
    assert(file);
    fputc(1, file);
    rewind(file);
    return file;
}

#define fopen test_fopen
#include "../source/platform/assets.c"
#undef fopen

Result FSUSER_OpenFileDirectly(Handle *out, unsigned archive, FS_Path ap, FS_Path path, unsigned flags, unsigned attrs)
{
    (void)ap; (void)flags; (void)attrs;
    assert(archive == ARCHIVE_SDMC && !sd_file);
    if (!accepted_path || strcmp(path.data, accepted_path)) { return -1; }
    sd_file = fopen("silent-hill-3ds.3dsx", "rb");
    assert(sd_file);
    opens++;
    *out = 1;
    return 0;
}

Result FSFILE_Read(Handle file, uint32_t *count, uint64_t offset, void *data, uint32_t size)
{
    assert(file == 1 && sd_file);
    assert(fseek(sd_file, (long)offset, SEEK_SET) == 0);
    *count = (uint32_t)fread(data, 1, size, sd_file);
    if (offset == 0 && broken_header == 1) { *count = 12; }
    if (offset == 0 && broken_header == 2) { ((uint8_t *)data)[0] ^= 1; }
    if (offset == 0 && broken_header == 3) { memset((uint8_t *)data + 40, 255, 4); }
    return 0;
}

Result FSFILE_GetSize(Handle file, uint64_t *size)
{
    assert(file == 1 && sd_file && fseek(sd_file, 0, SEEK_END) == 0);
    *size = (uint64_t)ftell(sd_file);
    return 0;
}

Result FSFILE_Close(Handle file)
{
    assert(file == 1 && sd_file);
    fclose(sd_file);
    sd_file = NULL;
    closes++;
    return 0;
}

Result romfsMountFromFile(Handle file, uint32_t offset, const char *name)
{
    uint32_t count, header;
    assert(!strcmp(name, "romfs"));
    mount_calls++;
    FSFILE_Read(file, &count, offset, &header, sizeof(header));
    assert(count == sizeof(header) && header == 40);
    if (fail_mount) { FSFILE_Close(file); return -1; }
    mounted = true;
    fallback_resources = true;
    return 0;
}

Result romfsInit(void) { mounted = primary_result >= 0; return primary_result; }
Result romfsExit(void) { mounted = false; if (sd_file) { FSFILE_Close(1); } return 0; }

int main(void)
{
    primary_result = 0;
    primary_resources = true;
    assert(sh_assets_init(0, NULL) && !opens);
    romfsExit();
    primary_result = -1;
    primary_resources = false;
    accepted_path = "/3ds/silent-hill-3ds/silent-hill-3ds.3dsx";
    char *relative[] = {"3ds/silent-hill-3ds/silent-hill-3ds.3dsx"};
    assert(sh_assets_init(1, relative) && mounted && mount_calls == 1);
    romfsExit();
    char *prefixed[] = {"sdmc:/3ds/silent-hill-3ds/silent-hill-3ds.3dsx"};
    assert(sh_assets_init(1, prefixed));
    romfsExit();
    char *network[] = {"3dslink:/silent-hill-3ds/silent-hill-3ds.3dsx"};
    assert(sh_assets_init(1, network));
    romfsExit();
    assert(sh_assets_init(0, NULL));
    romfsExit();
    for (broken_header = 1; broken_header <= 3; broken_header++) {
        assert(!sh_assets_init(1, prefixed) && !sd_file);
        assert(opens == closes);
    }
    broken_header = 0;
    fail_mount = 1;
    assert(!sh_assets_init(1, prefixed) && !sd_file && opens == closes);
    puts("PASS: default mount, launcher path variants, missing argv, truncated/corrupt files and handle ownership");
    return 0;
}
