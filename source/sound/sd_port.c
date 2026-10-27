#include "sd_port.h"

#include "sd_files.h"
#include "spu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* g_FileXaLoc (main/fileinfo.c, USA): only feeds the CdlLOC math in sd_call.c. */
u32 g_FileXaLoc[] = {
    0x00000, 0x099BF, 0x0A227, 0x0B377, 0x0D0BF, 0x0EA57, 0x0F997, 0x1096F, 0x16F07, 0x19797, 0x00000,
};

#define SD_FILE_COUNT ((int)(sizeof(g_SdAudioFiles) / sizeof(g_SdAudioFiles[0])))
#define XA_SECTOR_BYTES 2304

static char s_root[128] = "romfs:/game/snd";
static char s_xa_root[128] = "romfs:/game/xa";
static u8 *s_files[SD_FILE_COUNT];
static volatile s32 s_file_request = -1;

u32 g_sd_vsync_count;

typedef struct XaClip {
    u8 *data;
    u32 sectors;
    u8 coding;
} XaClip;

static XaClip s_xa;
static volatile s32 s_xa_request = -1; /* gSDXATable index waiting to be read */
static s32 s_xa_loaded = -1;
static s32 s_xa_selected = -1;
static bool s_xa_playing;
static u32 s_xa_sector;
static s16 s_xa_pcm[2][4032];
static int s_xa_pcm_count;
static int s_xa_pcm_pos;
static s32 s_xa_hist[2][2];
static u32 s_xa_phase; /* 16.16 source position inside s_xa_pcm */
static u32 s_xa_step;
static s16 s_xa_prev[2];

static const s32 k_xa_f0[4] = {0, 60, 115, 98};
static const s32 k_xa_f1[4] = {0, 0, -52, -55};

void sd_port_set_roots(const char *snd_root, const char *xa_root)
{
    if (snd_root) {
        snprintf(s_root, sizeof(s_root), "%s", snd_root);
    }
    if (xa_root) {
        snprintf(s_xa_root, sizeof(s_xa_root), "%s", xa_root);
    }
}

static u8 *read_whole(const char *path, u32 *size_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *buf = size > 0 ? (u8 *)malloc((size_t)size) : NULL;
    if (buf && fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (buf && size_out) {
        *size_out = (u32)size;
    }
    return buf;
}

bool sd_port_load_file(s32 idx)
{
    if (idx < 0 || idx >= SD_FILE_COUNT || !g_SdAudioFiles[idx].name[0]) {
        return false;
    }
    if (s_files[idx]) {
        return true;
    }
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", s_root, g_SdAudioFiles[idx].name);
    u32 size = 0;
    u8 *data = read_whole(path, &size);
#ifdef SD_PORT_TRACE
    printf("sd_port: load %s (%s)\n", g_SdAudioFiles[idx].name, data ? "ok" : "missing");
#endif
    if (!data || size < g_SdAudioFiles[idx].size) {
        free(data);
        return false;
    }
    s_files[idx] = data;
    return true;
}

const u8 *sd_file_get(s32 idx)
{
    if (idx < 0 || idx >= SD_FILE_COUNT) {
        return NULL;
    }
    if (!s_files[idx]) {
        s_file_request = idx;
    }
    return s_files[idx];
}

void sd_port_free_files(void)
{
    for (int i = 0; i < SD_FILE_COUNT; i++) {
        free(s_files[i]);
        s_files[i] = NULL;
    }
    free(s_xa.data);
    memset(&s_xa, 0, sizeof(s_xa));
    s_xa_loaded = -1;
}

/* ---- XA ------------------------------------------------------------------ */

static bool xa_read_clip_path(const char *path, XaClip *out)
{
    u32 size = 0;
    u8 *raw = read_whole(path, &size);
    if (!raw || size < 8 || memcmp(raw, "XAC1", 4) != 0) {
        free(raw);
        return false;
    }
    const u32 sectors = (u32)raw[4] | ((u32)raw[5] << 8);
    if (8 + sectors * XA_SECTOR_BYTES > size) {
        free(raw);
        return false;
    }
    out->data = raw;
    out->sectors = sectors;
    out->coding = raw[6];
    return true;
}

static bool xa_read_clip(s32 xa_idx, XaClip *out)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%03d.xac", s_xa_root, (int)xa_idx);
    return xa_read_clip_path(path, out);
}

void sd_port_service(SdLockFn lock, SdLockFn unlock)
{
    const s32 file = s_file_request;
    if (file >= 0) {
        s_file_request = -1;
        sd_port_load_file(file);
    }

    const s32 xa = s_xa_request;
    if (xa >= 0) {
        XaClip clip = {0};
        const bool ok = xa_read_clip(xa, &clip);
        lock();
        u8 *old = s_xa.data;
        if (ok) {
            s_xa = clip;
        } else {
            memset(&s_xa, 0, sizeof(s_xa));
        }
        s_xa_loaded = xa;
        s_xa_playing = false;
        if (s_xa_request == xa) {
            s_xa_request = -1;
        }
        unlock();
        free(old);
    }
}

void sd_xa_select(u16 xaIdx)
{
    s_xa_selected = xaIdx;
    if (s_xa_loaded != xaIdx) {
        s_xa_request = xaIdx;
    }
}

static void xa_decode_sector(void)
{
    const u8 *sec = s_xa.data + 8 + (size_t)s_xa_sector * XA_SECTOR_BYTES;
    const bool stereo = (s_xa.coding & 3) == 1;
    const bool eight = ((s_xa.coding >> 4) & 3) == 1;
    int nl = 0;
    int nr = 0;
    for (int g = 0; g < 18; g++) {
        const u8 *grp = sec + g * 128;
        const int units = eight ? 4 : 8;
        for (int u = 0; u < units; u++) {
            const u8 hdr = grp[4 + u];
            int shift = hdr & 0x0F;
            const int filter = (hdr >> 4) & 3;
            if (shift > 12) {
                shift = 9;
            }
            const int ch = stereo ? (u & 1) : 0;
            s32 *h = s_xa_hist[ch];
            for (int j = 0; j < 28; j++) {
                s32 raw;
                if (eight) {
                    raw = (s32)(s8)grp[16 + j * 4 + u] << 8;
                } else {
                    raw = (s32)(s16)(((grp[16 + j * 4 + (u >> 1)] >> ((u & 1) * 4)) & 0x0F) << 12);
                }
                s32 s = (raw >> shift) + ((h[0] * k_xa_f0[filter] + h[1] * k_xa_f1[filter] + 32) >> 6);
                s = s < -32768 ? -32768 : (s > 32767 ? 32767 : s);
                h[1] = h[0];
                h[0] = s;
                if (ch == 0) {
                    s_xa_pcm[0][nl++] = (s16)s;
                } else {
                    s_xa_pcm[1][nr++] = (s16)s;
                }
            }
        }
    }
    if (!stereo) {
        memcpy(s_xa_pcm[1], s_xa_pcm[0], (size_t)nl * sizeof(s16));
        nr = nl;
    }
    s_xa_pcm_count = nl < nr ? nl : nr;
    s_xa_pcm_pos = 0;
    s_xa_sector++;
}

static void xa_cd_source(s16 *dst, int n)
{
    for (int i = 0; i < n; i++) {
        if (!s_xa_playing) {
            dst[i * 2] = dst[i * 2 + 1] = 0;
            continue;
        }
        while ((s_xa_phase >> 16) >= 1) {
            s_xa_phase -= 1u << 16;
            s_xa_prev[0] = s_xa_pcm[0][s_xa_pcm_pos];
            s_xa_prev[1] = s_xa_pcm[1][s_xa_pcm_pos];
            if (++s_xa_pcm_pos >= s_xa_pcm_count) {
                if (s_xa_sector >= s_xa.sectors) {
                    s_xa_playing = false;
                    break;
                }
                xa_decode_sector();
            }
        }
        if (!s_xa_playing) {
            dst[i * 2] = dst[i * 2 + 1] = 0;
            continue;
        }
        const s32 frac = (s32)(s_xa_phase & 0xFFFF);
        for (int c = 0; c < 2; c++) {
            const s32 a = s_xa_prev[c];
            const s32 b = s_xa_pcm[c][s_xa_pcm_pos];
            dst[i * 2 + c] = (s16)(a + (((b - a) * frac) >> 16));
        }
        s_xa_phase += s_xa_step;
    }
}

static void xa_start(void)
{
    if (!s_xa.data || s_xa_loaded != s_xa_selected || s_xa.sectors == 0) {
        s_xa_playing = false;
        return;
    }
    const u32 rate = ((s_xa.coding >> 2) & 3) == 1 ? 18900 : 37800;
    s_xa_step = (u32)(((u64)rate << 16) / SPU_RATE);
    s_xa_sector = 0;
    s_xa_phase = 0;
    memset(s_xa_hist, 0, sizeof(s_xa_hist));
    s_xa_prev[0] = s_xa_prev[1] = 0;
    xa_decode_sector();
    s_xa_playing = true;
}

bool sd_port_xa_playing(void)
{
    return s_xa_playing;
}

bool sd_port_movie_begin(const char *path, SdLockFn lock, SdLockFn unlock)
{
    XaClip clip = {0};
    if (!xa_read_clip_path(path, &clip)) { return false; }
    lock();
    u8 *old = s_xa.data;
    s_xa = clip;
    s_xa_request = -1;
    s_xa_loaded = s_xa_selected = -2;
    xa_start();
    unlock();
    free(old);
    return true;
}

void sd_port_movie_end(void)
{
    s_xa_playing = false;
}

/* ---- libcd / libetc ------------------------------------------------------ */

u32 VSync(s32 mode)
{
    (void)mode;
    return g_sd_vsync_count;
}

void CdMix(CdlATV *vol)
{
    (void)vol;
}

s32 CdSync(s32 mode, u8 *result)
{
    (void)mode;
    if (result) {
        *result = CdlComplete;
    }
    return CdlComplete;
}

s32 CdControl(u8 com, u8 *param, u8 *result)
{
    (void)param;
    (void)result;
    switch (com) {
    case CdlSeekL:
        return s_xa_request < 0 && s_xa_loaded == s_xa_selected;
    case CdlReadN:
        xa_start();
        return 1;
    case CdlPause:
        s_xa_playing = false;
        return 1;
    default:
        return 1;
    }
}

s32 CdControlB(u8 com, u8 *param, u8 *result)
{
    return CdControl(com, param, result);
}

s32 CdReset(s32 mode)
{
    (void)mode;
    return 1;
}

void sd_port_init(void)
{
    spu_set_cd_source(xa_cd_source);
}
