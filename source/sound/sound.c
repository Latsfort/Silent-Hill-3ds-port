#include "sh3ds/sound.h"

#include "libsd/libsd.h"
#include "sd_port.h"
#include "sound_system.h"
#include "spu.h"

#include <string.h>

#ifdef __3DS__
#include <3ds.h>
#endif

/* libsd timer: RCnt2 at 7328 ticks of sysclk/8 (~577.7 Hz), counted in SPU samples. */
#define TIMER_TICK_NUM 4233600u
#define TIMER_TICK_DEN (SPU_RATE * 7328u)
#define TASK_RATE 30.0f

static bool s_ready;
#ifdef __3DS__
static bool s_output;
#endif
static u32 s_timer_acc;
static float s_task_acc;
static float s_vsync_acc;

#ifdef __3DS__
#define WAVEBUF_COUNT 4
#define WAVEBUF_FRAMES 1024
#define NDSP_CH 0

static LightLock s_lock;
static LightEvent s_event;
static Thread s_thread;
static volatile bool s_thread_run;
static ndspWaveBuf s_wavebuf[WAVEBUF_COUNT];
static s16 *s_pcm;

static void lock(void)
{
    LightLock_Lock(&s_lock);
}

static void unlock(void)
{
    LightLock_Unlock(&s_lock);
}
#else
static void lock(void)
{
}

static void unlock(void)
{
}
#endif

void sh_sound_render(int16_t *dst, int frames)
{
    lock();
    while (frames > 0) {
        const u32 left = TIMER_TICK_DEN - s_timer_acc;
        int n = (int)((left + TIMER_TICK_NUM - 1) / TIMER_TICK_NUM);
        if (n > frames) {
            n = frames;
        }
        spu_render(dst, n);
        dst += n * 2;
        frames -= n;
        s_timer_acc += (u32)n * TIMER_TICK_NUM;
        if (s_timer_acc >= TIMER_TICK_DEN) {
            s_timer_acc -= TIMER_TICK_DEN;
            if (sd_timer_event) {
                smf_timer();
            }
        }
    }
    unlock();
}

#ifdef __3DS__
static void ndsp_callback(void *data)
{
    (void)data;
    LightEvent_Signal(&s_event);
}

static void mixer_thread(void *arg)
{
    (void)arg;
    while (s_thread_run) {
        for (int i = 0; i < WAVEBUF_COUNT; i++) {
            ndspWaveBuf *wb = &s_wavebuf[i];
            if (wb->status != NDSP_WBUF_DONE && wb->status != NDSP_WBUF_FREE) {
                continue;
            }
            sh_sound_render(wb->data_pcm16, WAVEBUF_FRAMES);
            DSP_FlushDataCache(wb->data_pcm16, WAVEBUF_FRAMES * 2 * sizeof(s16));
            ndspChnWaveBufAdd(NDSP_CH, wb);
        }
        LightEvent_Wait(&s_event);
    }
}

static bool start_output(void)
{
    if (ndspInit() != 0) {
        return false;
    }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(NDSP_CH);
    ndspChnSetInterp(NDSP_CH, NDSP_INTERP_NONE);
    ndspChnSetRate(NDSP_CH, (float)SPU_RATE);
    ndspChnSetFormat(NDSP_CH, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = {0};
    mix[0] = mix[1] = 1.0f;
    ndspChnSetMix(NDSP_CH, mix);

    s_pcm = (s16 *)linearAlloc(WAVEBUF_COUNT * WAVEBUF_FRAMES * 2 * sizeof(s16));
    if (!s_pcm) {
        ndspExit();
        return false;
    }
    memset(s_pcm, 0, WAVEBUF_COUNT * WAVEBUF_FRAMES * 2 * sizeof(s16));
    memset(s_wavebuf, 0, sizeof(s_wavebuf));
    for (int i = 0; i < WAVEBUF_COUNT; i++) {
        s_wavebuf[i].data_pcm16 = s_pcm + i * WAVEBUF_FRAMES * 2;
        s_wavebuf[i].nsamples = WAVEBUF_FRAMES;
        s_wavebuf[i].status = NDSP_WBUF_FREE;
    }

    LightEvent_Init(&s_event, RESET_ONESHOT);
    ndspSetCallback(ndsp_callback, NULL);

    /* The SPU emulation runs on the system core; fall back to the app core. */
    APT_SetAppCpuTimeLimit(70);
    s_thread_run = true;
    s_thread = threadCreate(mixer_thread, NULL, 32 * 1024, 0x18, 1, false);
    if (!s_thread) {
        s_thread = threadCreate(mixer_thread, NULL, 32 * 1024, 0x18, 0, false);
    }
    if (!s_thread) {
        s_thread_run = false;
        ndspSetCallback(NULL, NULL);
        linearFree(s_pcm);
        s_pcm = NULL;
        ndspExit();
        return false;
    }
    return true;
}

static void stop_output(void)
{
    if (s_thread) {
        s_thread_run = false;
        LightEvent_Signal(&s_event);
        threadJoin(s_thread, U64_MAX);
        threadFree(s_thread);
        s_thread = NULL;
    }
    ndspSetCallback(NULL, NULL);
    ndspChnReset(NDSP_CH);
    ndspExit();
    linearFree(s_pcm);
    s_pcm = NULL;
}
#endif

/* g_AudioData indices of the VABs the cafe needs up front (tasks 160/162/166/164/171). */
static const s32 k_preload_files[] = {0, 2, 6, 4, 11};

bool sh_sound_init(void)
{
    bool loaded[ARRAY_SIZE(k_preload_files)];
#ifdef __3DS__
    LightLock_Init(&s_lock);
#endif
    for (size_t i = 0; i < ARRAY_SIZE(k_preload_files); i++) {
        loaded[i] = sd_port_load_file(k_preload_files[i]);
    }

    lock();
    SD_Init();
    sd_port_init();
    unlock();
    s_ready = true;

    /* Sd_VabLoad: file reads alone do not open the VAB or transfer its samples. */
    lock();
    for (size_t i = 0; i < ARRAY_SIZE(k_preload_files); i++) {
        if (loaded[i]) {
            SD_Call(160 + (u32)k_preload_files[i]);
        }
    }
    while (Sd_AudioStreamingCheck() != AudioStreamingState_None) {
        Sd_TaskPoolExecute();
    }
    unlock();

#ifdef __3DS__
    /* Without DSP firmware the driver keeps running silently so scripts waiting on it still advance. */
    s_output = start_output();
    return s_output;
#else
    return true;
#endif
}

void sh_sound_shutdown(void)
{
    if (!s_ready) {
        return;
    }
#ifdef __3DS__
    if (s_output) {
        stop_output();
        s_output = false;
    }
#endif
    lock();
    Sd_AudioStop();
    unlock();
    sd_port_free_files();
    s_ready = false;
}

void sh_sound_update(float dt)
{
    if (dt < 0.0f) {
        dt = 0.0f;
    } else if (dt > 0.25f) {
        dt = 0.25f;
    }

    sd_port_service(lock, unlock);

    s_vsync_acc += dt * 60.0f;
    while (s_vsync_acc >= 1.0f) {
        s_vsync_acc -= 1.0f;
        g_sd_vsync_count++;
    }

    s_task_acc += dt * TASK_RATE;
    int steps = 0;
    while (s_task_acc >= 1.0f && steps < 4) {
        s_task_acc -= 1.0f;
        steps++;
        lock();
        Sd_TaskPoolExecute();
        unlock();
    }
    if (s_task_acc > 1.0f) {
        s_task_acc = 1.0f;
    }
}

void sh_sd_call(uint32_t cmd)
{
    lock();
    SD_Call(cmd);
    unlock();
}

uint8_t sh_sfx_play(uint16_t sfx, int8_t balance, uint8_t vol)
{
    lock();
    const u8 r = Sd_SfxPlay(sfx, balance, vol);
    unlock();
    return r;
}

void sh_sfx_update(uint16_t sfx, int8_t balance, uint8_t vol, int8_t pitch)
{
    lock();
    Sd_SfxAttributesUpdate(sfx, balance, vol, pitch);
    unlock();
}

void sh_sfx_pitch_play(uint16_t sfx, int8_t balance, uint8_t vol, int8_t pitch)
{
    lock();
    Sd_SfxWithPitchPlay(sfx, balance, vol, pitch);
    unlock();
}

void sh_sfx_stop(uint16_t sfx)
{
    lock();
    Sd_SfxStop(sfx);
    unlock();
}

void sh_xa_preload(uint16_t cmd)
{
    lock();
    Sd_XaPreLoadAudioPreTaskAdd(cmd);
    unlock();
}

uint8_t sh_sound_streaming(void)
{
    lock();
    const u8 r = Sd_AudioStreamingCheck();
    unlock();
    return r;
}

bool sh_xa_active(void)
{
    return Sd_XaAudioIdxGet() != 0;
}

void sh_bgm_channel_volume(uint8_t channel, uint8_t vol)
{
    lock();
    Sd_MidiChannelsVolumeSet(channel, vol);
    unlock();
}

uint8_t sh_bgm_channel_volume_get(uint8_t channel)
{
    lock();
    const u8 r = Sd_MidiChannelVolumeGet(channel);
    unlock();
    return r;
}

uint16_t sh_bgm_song_task(void)
{
    return Sd_MidiChannelTaskGet();
}
