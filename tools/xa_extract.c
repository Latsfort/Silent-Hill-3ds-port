/* Host tool: cuts gSDXATable clips out of the dumped XA/ files (2336-byte Mode 2 Form 2 sectors).
 * usage: xa_extract <dump/assets/XA> <out dir> <SD_Call xa cmd>... */
#include "sound_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAW_SECTOR 2336
#define AUDIO_BYTES 2304

static const char *const k_xa_files[] = {
    NULL, "05_02152", "10_04432", "15_07496", "20_06552", "25_03904",
    "30_04056", "35_26008", "40_10384", "45_28784",
};

static int extract(const char *xa_dir, const char *out_dir, int idx)
{
    const s_XaItemData *it = &gSDXATable[idx];
    if (it->xaFileIdx < 1 || it->xaFileIdx > 9) {
        fprintf(stderr, "xa %d: bad file idx %d\n", idx, it->xaFileIdx);
        return 1;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", xa_dir, k_xa_files[it->xaFileIdx]);
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    const long total = ftell(f) / RAW_SECTOR;

    u8 sec[RAW_SECTOR];
    u8 *out = NULL;
    u32 count = 0;
    u8 coding = 0;
    double frames = 0;
    const double need = (it->audioLength + 32) / 60.0;
    for (long s = it->sector; s < total; s++) {
        fseek(f, s * RAW_SECTOR, SEEK_SET);
        if (fread(sec, 1, RAW_SECTOR, f) != RAW_SECTOR) {
            break;
        }
        const u8 file = sec[0];
        const u8 chan = sec[1];
        const u8 submode = sec[2];
        if (file != it->field_8_24 || chan != it->field_4_24 || !(submode & 0x04)) {
            continue;
        }
        if (count == 0) {
            coding = sec[3];
        }
        out = (u8 *)realloc(out, (size_t)(count + 1) * AUDIO_BYTES);
        memcpy(out + (size_t)count * AUDIO_BYTES, sec + 8, AUDIO_BYTES);
        count++;
        const int rate = ((coding >> 2) & 3) == 1 ? 18900 : 37800;
        const int stereo = (coding & 3) == 1;
        frames += (stereo ? 2016.0 : 4032.0) / rate;
        if ((submode & 0x80) || frames >= need) {
            break;
        }
    }
    fclose(f);
    if (count == 0) {
        fprintf(stderr, "xa %d: no sectors\n", idx);
        return 1;
    }
    snprintf(path, sizeof(path), "%s/%03d.xac", out_dir, idx);
    FILE *o = fopen(path, "wb");
    if (!o) {
        fprintf(stderr, "cannot write %s\n", path);
        free(out);
        return 1;
    }
    const u8 hdr[8] = {'X', 'A', 'C', '1', (u8)count, (u8)(count >> 8), coding, 0};
    fwrite(hdr, 1, 8, o);
    fwrite(out, AUDIO_BYTES, count, o);
    fclose(o);
    free(out);
    printf("xa %3d: file %d chan %d sectors %u coding 0x%02X %.2fs\n", idx, it->xaFileIdx, it->field_4_24, count,
           coding, frames);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: xa_extract <xa dir> <out dir> <cmd>...\n");
        return 1;
    }
    int err = 0;
    for (int i = 3; i < argc; i++) {
        err |= extract(argv[1], argv[2], atoi(argv[i]) & 0xFFF);
    }
    return err;
}
