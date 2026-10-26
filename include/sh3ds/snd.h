#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sh3ds/gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Game-side sound helpers on top of sh3ds/sound.h, ported from bodyprog:
 * sfx.c (positional SFX), bgm_update.c / bgm_utils.c (BGM layers, crossfades).
 * Positions are 3DS-space metres (X = PS1 X, Y = -PS1 Y, Z = -PS1 Z).
 */

/* SfxId (sfx_id_enum.h) used by the cafe. */
enum {
    SH_SFX_MENU_ERROR = 1304,
    SH_SFX_MENU_MOVE = 1305,
    SH_SFX_MENU_CANCEL = 1306,
    SH_SFX_MENU_CONFIRM = 1307,
    SH_SFX_MENU_MAP = 1308,
    SH_SFX_FOOTSTEP_CONCRETE = 1311,
    SH_SFX_CHERYL_FOOTSTEP = 1353, /* Sfx_CherylFootstep */
    SH_SFX_ALLEY_WHEEL = 1358,     /* Sfx_Unk1358 — otherworld alley */
    SH_SFX_NIGHTMARE_AMB = 1359,   /* Sfx_Unk1359 */
    SH_SFX_ALLEY_TENSION = 1360,   /* Sfx_Unk1360 */
    SH_SFX_OPENING_AMB = 1361,     /* Sfx_Unk1361 — opening fog / street */
    SH_SFX_HANDGUN_SHOT = 1283,
    SH_SFX_HANDGUN_RELOAD = 1284,
    SH_SFX_HANDGUN_EMPTY = 1285,
    SH_SFX_KNIFE_SWING = 1293,
    SH_SFX_HIT_1313 = 1313,
    SH_SFX_USE_KEY = 1335,     /* Sfx_UseKey */
    SH_SFX_DOOR_LOCKED = 1344, /* Sfx_DoorLocked */
    SH_SFX_UNK1349 = 1349,     /* Sfx_Unk1349 */
    SH_SFX_HIT_1316 = 1316,
    SH_SFX_HARRY_DAMAGE = 1326,
    SH_SFX_RADIO_INTERFERENCE = 1321,
    SH_SFX_RADIO_STATIC = 1322,
    SH_SFX_CAFE_DOOR = 1357,
    SH_SFX_WINDOW_1373 = 1373,
    SH_SFX_WINDOW_1374 = 1374,
    SH_SFX_WINDOW_1375 = 1375,
    SH_SFX_BIRD_1376 = 1376,
    SH_SFX_BIRD_HURT = 1377,
    SH_SFX_BIRD_ATTACK = 1378,
    SH_SFX_BIRD_BITE = 1379,
    SH_SFX_BIRD_DEATH = 1380,
};

/* e_BgmStatusFlags */
enum {
    SH_BGM_STATUS_PAUSE = 1 << 0,
    SH_BGM_STATUS_APPLY_MUTE = 1 << 1,
    SH_BGM_STATUS_RADIO_ACTIVE = 1 << 2,
    SH_BGM_STATUS_DUCK = 1 << 3,
    SH_BGM_STATUS_VOICE_DIALOG = 1 << 5,
    SH_BGM_STATUS_REQUEST_MUTE = 1 << 7,
};

/* e_BgmFlags */
enum {
    SH_BGM_LAYER1 = 1 << 0,
    SH_BGM_LAYER2 = 1 << 1,
    SH_BGM_KEEP_ALIVE = 1 << 8,
    SH_BGM_MUTE_ALL = 1 << 9,
};

void sh_snd_reset(void);
void sh_snd_set_listener(const ShCamera *cam, const float player[3]);

/* Sd_SfxPlay with raw balance / attenuation (0 = full volume). */
void sh_snd_play(uint16_t sfx, int balance, int atten);
void sh_snd_update(uint16_t sfx, int balance, int atten, int pitch);
int sh_snd_camera_balance(const ShCamera *camera, const float position[3]);
/* Sfx_WithFlagsPlay(sfx, pos, vol, SfxFlag_None); vol in [0, 1]. */
void sh_snd_at(uint16_t sfx, const float pos[3], float vol);
/* Sfx_WithPitchPlay. */
void sh_snd_at_pitch(uint16_t sfx, const float pos[3], float vol, int pitch);
/* Sfx_WithFalloffAndPitchPlay: vol in Q8 (0..255), falloff in metres. */
void sh_snd_falloff(uint16_t sfx, const float pos[3], int vol_q8, float falloff, int pitch);
void sh_snd_stop(uint16_t sfx);
void sh_snd_call(uint32_t cmd);
/* Back to title / new game: stop voice, music and the cafe radio loops. */
void sh_snd_stop_all(void);

/* Loads the weapon VAB for the equipped InvItemId (func_8007F14C); no-op when unchanged. */
void sh_snd_weapon(int item_id);

/* g_SysWork.bgmStatusFlags, cleared at the start of every game frame. */
void sh_bgm_frame_begin(void);
void sh_bgm_status_set(unsigned flags);
unsigned sh_bgm_status(void);
int sh_bgm_current(void);
/* Bgm_CrossfadeToTrack: call every frame of the event step; true once done. */
bool sh_bgm_crossfade(int bgm_idx);
/* Abort a stuck crossfade (e.g. XA streaming never cleared). */
void sh_bgm_crossfade_reset(void);
/* Bgm_LayersUpdate. */
void sh_bgm_layers_update(unsigned bgm_flags, float fade_speed, bool player_dead, float dt);

#ifdef __cplusplus
}
#endif
