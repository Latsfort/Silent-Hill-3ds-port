#include "sh3ds/snd.h"

#include "sh3ds/savegame.h"
#include "sh3ds/sound.h"

#include <math.h>
#include <stddef.h>

#define Q12F(x) ((int32_t)((x) * 4096.0f))

static float s_cam[3];
static float s_cam_right[3] = {1.0f, 0.0f, 0.0f};
static float s_player[3];
static int s_weapon_item = -1;

/* BGM state (g_SysWork / g_GameWork fields + bgm_update.c statics). */
static unsigned s_bgm_status;
static int s_bgm_idx;
static int s_xfade_step;
static float s_layer_vols[9];
static int s_channel_set_state;

/* g_Pow2NegFracTable, including the original's off entry [1]. */
static const int32_t k_pow2neg_frac[33] = {
    Q12F(1.0f),    Q12F(0.9876f), Q12F(0.9576f), Q12F(0.9371f), Q12F(0.917f),  Q12F(0.8973f), Q12F(0.878f),
    Q12F(0.8592f), Q12F(0.8406f), Q12F(0.8228f), Q12F(0.8052f), Q12F(0.788f),  Q12F(0.771f),  Q12F(0.7544f),
    Q12F(0.7383f), Q12F(0.7225f), Q12F(0.7071f), Q12F(0.692f),  Q12F(0.6771f), Q12F(0.6626f), Q12F(0.6482f),
    Q12F(0.6343f), Q12F(0.621f),  Q12F(0.6075f), Q12F(0.5945f), Q12F(0.582f),  Q12F(0.5694f), Q12F(0.5572f),
    Q12F(0.5452f), Q12F(0.5335f), Q12F(0.522f),  Q12F(0.5108f), Q12F(0.5f),
};

/* g_BgmTaskLoad / g_BgmChannelSetTask (background_sound_init.c). */
static const uint16_t k_bgm_task_load[42] = {
    0,  0,  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 64, 65, 66, 67, 68, 69, 45, 70, 71, 63,
};
static const uint8_t k_bgm_channel_set[42] = {
    0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 33, 34, 35, 36, 37, 38, 14, 39, 40, 32,
};

/* Math_Pow2Neg */
static int32_t pow2neg(int32_t x)
{
    const int32_t frac = x & 0xFFF;
    const int32_t ipart = x >> 12;
    if (ipart >= 12) {
        return 0;
    }
    if (ipart < -20) {
        return INT32_MAX;
    }
    const int32_t alpha = (x & 0x7F) << 5;
    const int idx = frac >> 7;
    int32_t v = k_pow2neg_frac[idx];
    v = ipart > 0 ? v >> ipart : (ipart < 0 ? v << -ipart : v);
    if (alpha != 0) {
        int32_t n = k_pow2neg_frac[idx + 1];
        n = ipart > 0 ? n >> ipart : (ipart < 0 ? n << -ipart : n);
        v = (int32_t)(((int64_t)v * (4096 - alpha)) >> 12) + (int32_t)(((int64_t)n * alpha) >> 12);
    }
    return v;
}

static int32_t pow2neg_clamped(int32_t x)
{
    const int32_t v = pow2neg(x);
    return v > Q12F(4.0f) ? Q12F(4.0f) : (v < 0 ? 0 : v);
}

static float dist3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/* Vc_StereoBalanceGet */
static int stereo_balance(const float pos[3])
{
    float d[3] = {pos[0] - s_cam[0], pos[1] - s_cam[1], pos[2] - s_cam[2]};
    const float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-4f) {
        return 0;
    }
    const float dot = (d[0] * s_cam_right[0] + d[1] * s_cam_right[1] + d[2] * s_cam_right[2]) / len;
    int b = (int)(dot * 128.0f);
    return b > 127 ? 127 : (b < -127 ? -127 : b);
}

int sh_snd_camera_balance(const ShCamera *camera, const float position[3])
{
    const float dx = position[0] - camera->pos[0], dy = position[1] - camera->pos[1], dz = position[2] - camera->pos[2];
    const float length = sqrtf(dx * dx + dy * dy + dz * dz);
    if (length < 1e-4f) { return 0; }
    const int balance = (int)((dx * cosf(camera->yaw) - dz * sinf(camera->yaw)) / length * 128.0f);
    return balance > 127 ? 127 : balance < -127 ? -127 : balance;
}

/* Sfx_DistanceAttenuatedVolumeGet: vol and result in Q8. */
static int dist_atten_volume(const float pos[3], int vol)
{
    int32_t cam = pow2neg_clamped((Q12F(dist3(s_cam, s_player)) - Q12F(2.5f)) / 10);
    if (cam > Q12F(1.0f)) {
        cam = Q12F(1.0f);
    }
    const int32_t src = pow2neg_clamped((Q12F(dist3(s_player, pos)) - Q12F(6.0f)) / 4);
    int32_t adj = (int32_t)(((int64_t)cam * src) >> 12);
    adj = adj > 8192 ? 8192 : (adj < 0 ? 0 : adj);
    adj = (int32_t)(((int64_t)vol * adj) >> 12);
    return adj > 255 ? 255 : (adj < 0 ? 0 : adj);
}

static int vol_q8(float vol)
{
    const int v = (int)(vol * 256.0f);
    return v > 255 ? 255 : (v < 0 ? 0 : v);
}

void sh_snd_reset(void)
{
    s_weapon_item = -1;
    s_bgm_status = 0;
    s_bgm_idx = 0;
    s_xfade_step = 0;
    s_channel_set_state = 0;
    for (size_t i = 0; i < sizeof(s_layer_vols) / sizeof(s_layer_vols[0]); i++) {
        s_layer_vols[i] = 0.0f;
    }
}

void sh_snd_set_listener(const ShCamera *cam, const float player[3])
{
    if (cam) {
        s_cam[0] = cam->pos[0];
        s_cam[1] = cam->pos[1];
        s_cam[2] = cam->pos[2];
        s_cam_right[0] = cosf(cam->yaw);
        s_cam_right[1] = 0.0f;
        s_cam_right[2] = -sinf(cam->yaw);
    }
    if (player) {
        s_player[0] = player[0];
        s_player[1] = player[1];
        s_player[2] = player[2];
    }
}

void sh_snd_play(uint16_t sfx, int balance, int atten)
{
    sh_sfx_play(sfx, (int8_t)balance, (uint8_t)atten);
}

/* Sd_SfxAttributesUpdate. */
void sh_snd_update(uint16_t sfx, int balance, int atten, int pitch)
{
    sh_sfx_update(sfx, (int8_t)balance, (uint8_t)atten, (int8_t)pitch);
}

void sh_snd_at(uint16_t sfx, const float pos[3], float vol)
{
    const int adj = dist_atten_volume(pos, vol_q8(vol));
    sh_sfx_play(sfx, (int8_t)stereo_balance(pos), (uint8_t)~adj);
}

void sh_snd_at_pitch(uint16_t sfx, const float pos[3], float vol, int pitch)
{
    const int adj = dist_atten_volume(pos, vol_q8(vol));
    sh_sfx_pitch_play(sfx, (int8_t)stereo_balance(pos), (uint8_t)~adj, (int8_t)pitch);
}

void sh_snd_falloff(uint16_t sfx, const float pos[3], int vol, float falloff, int pitch)
{
    if (vol > 255) {
        vol = 255;
    }
    if (vol < 0) {
        return;
    }
    const int32_t dist = Q12F(dist3(s_player, pos));
    const int32_t atten = (int32_t)(((int64_t)vol * dist) / Q12F(falloff));
    const int32_t base = vol - 255;
    int adj = 0;
    if (atten - base >= 0) {
        adj = atten - base < 255 ? (uint8_t)(atten - (vol + 1)) : 255;
    }
    sh_sfx_update(sfx, (int8_t)stereo_balance(pos), (uint8_t)adj, (int8_t)pitch);
}

void sh_snd_stop(uint16_t sfx)
{
    sh_sfx_stop(sfx);
}

void sh_snd_call(uint32_t cmd)
{
    sh_sd_call(cmd);
}

void sh_snd_stop_all(void)
{
    sh_sd_call(20); /* all SFX, BGM and XA */
    s_bgm_idx = 0;
    s_xfade_step = 0;
    s_channel_set_state = 0;
    for (size_t i = 0; i < sizeof(s_layer_vols) / sizeof(s_layer_vols[0]); i++) {
        s_layer_vols[i] = 0.0f;
    }
}

void sh_snd_weapon(int item_id)
{
    if (item_id == s_weapon_item) {
        return;
    }
    s_weapon_item = item_id;
    if (item_id == SH_ITEM_HANDGUN) {
        sh_sd_call(166);
    } else if (item_id == SH_ITEM_POCKET_KNIFE) {
        sh_sd_call(164);
    }
}

/* ---- BGM ------------------------------------------------------------------ */

void sh_bgm_frame_begin(void)
{
    s_bgm_status = 0;
}

void sh_bgm_status_set(unsigned flags)
{
    s_bgm_status |= flags;
}

unsigned sh_bgm_status(void)
{
    return s_bgm_status;
}

int sh_bgm_current(void)
{
    return s_bgm_idx;
}

static void bgm_song_set(int idx)
{
    if (idx <= 1 || idx >= 42) {
        return;
    }
    s_bgm_idx = idx;
    sh_sd_call(k_bgm_task_load[idx]);
}

static void bgm_layers_mute(void)
{
    for (int i = 0; i < 9; i++) {
        s_layer_vols[i] = 0.0f;
    }
}

void sh_bgm_crossfade_reset(void)
{
    s_xfade_step = 0;
}

bool sh_bgm_crossfade(int bgm_idx)
{
    /* XA voice must not soft-lock the crossfade forever (intro → cafe). */
    if (sh_sound_streaming() != 0 && s_xfade_step == 0) {
        return false;
    }
    switch (s_xfade_step) {
    case 0:
        if (bgm_idx <= 1 || s_bgm_idx == bgm_idx) {
            s_xfade_step = 3;
            break;
        }
        s_bgm_status |= SH_BGM_STATUS_REQUEST_MUTE;
        s_xfade_step++;
        break;
    case 1:
        s_bgm_status |= SH_BGM_STATUS_REQUEST_MUTE;
        sh_sd_call(23);
        s_xfade_step++;
        break;
    case 2:
        s_bgm_status |= SH_BGM_STATUS_REQUEST_MUTE;
        if (sh_bgm_song_task() == 0 || sh_sound_streaming() != 0) {
            /* Proceed even if a lingering XA kept song_task busy. */
            bgm_song_set(bgm_idx);
            s_xfade_step++;
        }
        break;
    default:
        s_xfade_step = 0;
        return true;
    }
    return false;
}

static void bgm_layers_from_channels(void)
{
    for (int i = 1; i < 8; i++) {
        s_layer_vols[i] = (float)sh_bgm_channel_volume_get((uint8_t)i) / 128.0f;
    }
    if (sh_bgm_song_task() == 0) {
        s_layer_vols[0] = 1.0f;
    }
    s_layer_vols[8] = 0.0f;
}

void sh_bgm_layers_update(unsigned flags, float fade_speed, bool player_dead, float dt)
{
    int ch_vols[8];

    if (player_dead) {
        flags &= SH_BGM_KEEP_ALIVE;
        flags |= SH_BGM_LAYER1;
        fade_speed = 0.2f;
    }
    if (s_bgm_status & SH_BGM_STATUS_REQUEST_MUTE) {
        flags = SH_BGM_LAYER1 | SH_BGM_MUTE_ALL;
        s_bgm_status |= SH_BGM_STATUS_APPLY_MUTE;
    }
    if (flags & SH_BGM_LAYER1) {
        flags &= SH_BGM_KEEP_ALIVE | SH_BGM_MUTE_ALL;
    } else {
        flags ^= SH_BGM_LAYER1;
    }

    for (int i = 0; i < 9; i++) {
        float cur = s_layer_vols[i];
        float target, step;
        if (i == 8) {
            step = dt * 0.25f;
            if (s_bgm_status & SH_BGM_STATUS_APPLY_MUTE) {
                target = 1.0f;
            } else if (s_bgm_status & SH_BGM_STATUS_RADIO_ACTIVE) {
                target = 0.75f;
            } else {
                target = (s_bgm_status & SH_BGM_STATUS_DUCK) ? 0.5f : 0.0f;
            }
        } else if ((flags >> i) & 1) {
            step = dt * fade_speed * 2.0f;
            target = 1.0f;
        } else {
            step = dt * fade_speed;
            target = 0.0f;
        }
        const float diff = target - cur;
        if (cur != target) {
            if (step < diff) {
                cur += step;
            } else if (diff >= -step) {
                cur = target;
            } else {
                cur -= step;
            }
        }
        s_layer_vols[i] = cur;
    }

    bool any_active = false;
    const float first_layer = 1.0f - s_layer_vols[8];
    for (int i = 0; i < 8; i++) {
        float v = s_layer_vols[i];
        any_active |= v != 0.0f;
        if (i == 0) {
            v *= first_layer;
        }
        int c = (int)(v * 127.0f);
        ch_vols[i] = c > 127 ? 127 : (c < 0 ? 0 : c);
    }

    const uint16_t task = sh_bgm_song_task();
    const bool channels_active = task != 0 && task != 0xFFFF;
    bool playing = false;
    if (any_active) {
        switch (s_channel_set_state) {
        case 3:
            bgm_layers_mute();
            if (channels_active) {
                s_channel_set_state = 0;
            } else {
                if (s_bgm_idx > 1 && s_bgm_idx < 42) {
                    sh_sd_call(0x300u + k_bgm_channel_set[s_bgm_idx]);
                }
                s_channel_set_state = 2;
            }
            break;
        case 2:
            bgm_layers_mute();
            s_channel_set_state = 1;
            break;
        case 1:
            if (channels_active) {
                bgm_layers_from_channels();
            } else {
                bgm_layers_mute();
            }
            s_channel_set_state = 0;
            break;
        default:
            playing = true;
            break;
        }
    } else if (flags & SH_BGM_MUTE_ALL) {
        if (s_channel_set_state != 3) {
            s_channel_set_state = 3;
            sh_sd_call(18);
        }
    } else if (s_channel_set_state == 0) {
        playing = true;
    }

    if (playing) {
        if (channels_active) {
            for (int i = 0; i < 8; i++) {
                sh_bgm_channel_volume((uint8_t)i, (uint8_t)ch_vols[i]);
            }
        } else {
            bgm_layers_mute();
            s_channel_set_state = 3;
        }
    }
}
