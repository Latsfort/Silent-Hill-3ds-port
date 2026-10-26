/* Generated from Player_FootstepSfxGet/Play, func_80071620 and shared player states. */
void sh_phone_audio_reset(ShPhoneAudio *audio) { audio->flags = 48; audio->intensity = 0; }
int sh_phone_sound_no_distance_flag(void) { return 2; }
static void phone_audio_emit(ShPhoneAudioFrame *frame, int id, int volume, int pitch, int flags)
{ if (frame->count < sizeof(frame->sounds) / sizeof(frame->sounds[0])) { frame->sounds[frame->count++] = (ShPhoneSound){id, volume, pitch, flags}; } }
static void phone_ground_sound(int groundType, int *sfxId, int *pitch0, int *pitch1)
{


    switch (groundType)
    {
        case 8:
            *sfxId = 1330;





                        break;

        case 3:
            *sfxId = 1312;
            break;

        case 4:
            *sfxId = 1313;
            break;

        case 5:
            if (5 == 24)
            {
                *sfxId = 1543;
            }
            else
            {
                *sfxId = 1557;
            }
            break;

        case 6:
        case 10:
        case 11:
            *sfxId = 1332;
            break;

        case 9:
            if (5 == 2)
            {
                *sfxId = 1388;
            }
            else
            {
                *sfxId = 1331;
            }
            break;

        case 2:
            *sfxId = 1389;
            break;

        default:
        case 0:
        case 1:
        case 7:
            *sfxId = 1311;
            break;
    }

    switch (groundType)
    {
        case 5:
        case 6:
        case 10:
        case 11:
            *pitch0 = (sh_rng_rand16() % 8)  - 4;
            *pitch1 = (sh_rng_rand16() % 16) + 56;
            break;

        default:
            *pitch0 = (sh_rng_rand16() % 32) - 16;
            *pitch1 = (sh_rng_rand16() % 64) + 32;
            break;
    }

}
static bool phone_footstep(ShPhoneAudio *audio, ShPhoneAudioFrame *frame, int animStatus, const ShPathPlayer *player, int keyframe0, int keyframe1, int sfx, int pitch)
{

    if (player->model.animation_status != animStatus)
    {
        return false;
    }

    if (player->model.keyframe >= keyframe1)
    {
        if (!(audio->flags & 16))
        {
            if (pitch < 0x20)
            {
                switch (animStatus)
                {
                    case 13:
                    case 11:
                    case 25:
                    case 27:
                        phone_audio_emit(frame, sfx, 24, pitch, 0);
                        audio->intensity = pitch;
                        break;

                    default:
                        phone_audio_emit(frame, sfx, 64, pitch, 0);
                        audio->intensity = pitch + 0x10;
                        break;
                }
            }
            else
            {
                phone_audio_emit(frame, sfx, 128, pitch, 0);
                audio->intensity = pitch + 0x40;
            }

            audio->flags |= 16;
            return true;
        }
    }
    else
    {
        audio->flags &= ~16;
    }

    if (player->model.keyframe >= keyframe0)
    {
        if (!(audio->flags & 32))
        {
            if (pitch < 32)
            {
                switch (animStatus)
                {
                    case 13:
                    case 11:
                    case 25:
                    case 27:
                        phone_audio_emit(frame, sfx, 24, pitch, 0);
                        audio->intensity = pitch;
                        break;

                    default:
                        phone_audio_emit(frame, sfx, 64, pitch, 0);
                        audio->intensity = pitch + 16;
                        break;
                }
            }
            else
            {
                phone_audio_emit(frame, sfx, 128, pitch, 0);
                audio->intensity = pitch + 64;
            }

            audio->flags |= 32;
            return true;
        }
    }
    else
    {
        audio->flags &= ~32;
    }

    return false;

}
static bool phone_event_sound(ShPhoneAudio *audio, ShPhoneAudioFrame *frame, int animStatus, const ShPathPlayer *player, int keyframeIdx, int sfxId)
{

    if (player->model.animation_status != animStatus)
    {
        return false;
    }

    if (player->model.keyframe >= keyframeIdx)
    {
        if (audio->flags & 8192)
        {
            return false;
        }

        switch (sfxId)
        {
            case 1314:
            case 1316:
            case 1318:
            case 1319:
            case 1333:
                phone_audio_emit(frame, sfxId, 0x80, 0, 0);
                audio->intensity = 0x40;
                break;

            case 1283:
                phone_audio_emit(frame, sfxId, 0xC8, 0, 2);
                audio->intensity = 0;
                break;

            case 1628:
                phone_audio_emit(frame, sfxId, 0x40, 0, 1);
                break;

            case 1626:
                phone_audio_emit(frame, sfxId, 0xFF, 0, 1);
                break;

            case 1638:
                phone_audio_emit(frame, sfxId, 0xFF, 0, 2);
                break;

            default:
            case 1320:
            case 1334:
                phone_audio_emit(frame, sfxId, 0x40, 0, 2);
                audio->intensity = 0;
                break;
        }

        audio->flags |= 8192;
        return true;
    }
    else
    {
        audio->flags &= ~8192;

        do {} while (false);

        return false;
    }

}
void sh_phone_audio_step(ShPhoneAudio *audio, const ShPathPlayer *player, int ground_type, ShPhoneAudioFrame *frame)
{
    int footstepSfxId, pitch0, pitch1;
    frame->count = 0;
    phone_ground_sound(ground_type, &footstepSfxId, &pitch0, &pitch1);
    switch (player->state) {
case 52:

            if (audio->flags & 32768)
            {
                if (player->model.animation_status == 52)
                {
                    phone_audio_emit(frame, footstepSfxId, 64, pitch0, 0);

                    audio->intensity                  = pitch0 + 16;
                    audio->flags &= ~32768;
                }
            }
            break;

case 53:

            phone_footstep(audio, frame, 5, player, 18, 6, footstepSfxId, pitch0);
            audio->flags |= 32768;
            break;

case 56:

            phone_footstep(audio, frame, 27, player, 204, 200, footstepSfxId, pitch0);
            audio->flags &= ~32768;
            break;

case 57:

            phone_footstep(audio, frame, 25, player, 187, 191, footstepSfxId, pitch0);
            audio->flags &= ~32768;
            break;

case 97:

            if ((1062 + 4) >= player->model.keyframe)
            {
                phone_footstep(audio, frame, player->model.animation_status, player, 1062 + 10, 1062 + 4, footstepSfxId, pitch0);
            }
            else
            {
                phone_footstep(audio, frame, player->model.animation_status, player, 1062 + 10, 1062 + 13, footstepSfxId, pitch0);
            }

            phone_event_sound(audio, frame, player->model.animation_status, player, 1062 + 26, 1456);
            break;

    default: break;
    }
}
