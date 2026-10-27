#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Silent Hill sound driver: the decomp's libsd + sd_call running on a software PS1 SPU
 * (source/sound). Every entry point below takes the SPU lock, so they are safe to call
 * from the game thread while the mixer runs.
 */

bool sh_sound_init(void);
void sh_sound_shutdown(void);
/* Game thread, once per frame: services file loads and runs Sd_TaskPoolExecute at 30 Hz. */
void sh_sound_update(float dt);

/* Renders interleaved stereo PCM at 44100 Hz (used by the NDSP thread and host tests). */
void sh_sound_render(int16_t *dst, int frames);
bool sh_sound_movie_begin(const char *path);
void sh_sound_movie_end(void);

void sh_sd_call(uint32_t cmd);
uint8_t sh_sfx_play(uint16_t sfx, int8_t balance, uint8_t vol);
void sh_sfx_update(uint16_t sfx, int8_t balance, uint8_t vol, int8_t pitch);
void sh_sfx_pitch_play(uint16_t sfx, int8_t balance, uint8_t vol, int8_t pitch);
void sh_sfx_stop(uint16_t sfx);
void sh_xa_preload(uint16_t cmd);
/* Sd_AudioStreamingCheck(): 0 when idle. */
uint8_t sh_sound_streaming(void);
bool sh_xa_active(void);

void sh_bgm_channel_volume(uint8_t channel, uint8_t vol);
uint8_t sh_bgm_channel_volume_get(uint8_t channel);
uint16_t sh_bgm_song_task(void);

#ifdef __cplusplus
}
#endif
