#pragma once
#include "sh3ds/savegame.h"
#include "sh3ds/path.h"

typedef struct ShPhonePage {
    int message;
    int32_t duration;
    uint8_t audio_type;
    bool end, instant;
    const char *raw;
} ShPhonePage;

const ShPhonePage *sh_phone_pages(unsigned *count);
bool sh_phone_animation_events(ShSavegame *save, int state, int keyframe, bool *held_hidden);

typedef struct ShPhoneText {
    int current, active_message, timer, audio_type;
    int rollout, menu, display_length, increment, max_index, selected;
    int active;
    bool load_audio, stop_audio;
    char visible[400 + 1];
} ShPhoneText;

/* Gfx_MapMsg_Draw: one retail 30Hz invocation; -1 setup/page change, 0 busy, 1 finished. */
int sh_phone_text_step(ShPhoneText *text, int message, int32_t delta, bool skip,
                       bool voice_dialog, uint8_t streaming);

typedef struct ShPhoneMotion {
    ShPath path;
    ShPathPoint waypoint;
    int16_t heading;
    int32_t tween[2];
} ShPhoneMotion;

typedef struct ShPhoneView {
    ShPathPoint position, look;
    bool warp_position, warp_look;
} ShPhoneView;

void sh_phone_motion_begin(ShPhoneMotion *motion, const ShPathPlayer *player);
bool sh_phone_path_step(ShPhoneMotion *motion, ShPathPlayer *player, int32_t delta);
bool sh_phone_camera_view(ShPhoneMotion *motion, const ShPathPlayer *player, int step, int32_t delta, ShPhoneView *view);
int sh_phone_held_bone(void);

typedef struct ShPhoneAudio {
    uint32_t flags;
    int intensity;
} ShPhoneAudio;
typedef struct ShPhoneSound {
    uint16_t id;
    int volume, pitch, flags;
} ShPhoneSound;
typedef struct ShPhoneAudioFrame {
    ShPhoneSound sounds[3];
    unsigned count;
} ShPhoneAudioFrame;
void sh_phone_audio_reset(ShPhoneAudio *audio);
void sh_phone_audio_step(ShPhoneAudio *audio, const ShPathPlayer *player, int ground_type, ShPhoneAudioFrame *frame);
int sh_phone_sound_no_distance_flag(void);

typedef struct ShPhoneScene {
    ShPhoneMotion motion;
    ShPhoneText text;
    bool held, hidden, locked, early_skip;
    double text_frames;
    int64_t text_delta;
    bool text_skip;
    ShPhoneAudio audio;
    double audio_frames;
    int ground_type;
} ShPhoneScene;
