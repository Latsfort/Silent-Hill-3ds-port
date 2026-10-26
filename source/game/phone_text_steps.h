/* Generated from Gfx_MapMsg_Draw and the original StringDraw J parser. */
static int phone_text_clamp(int value, int low, int high)
{ return value < low ? low : value > high ? high : value; }
static int phone_text_string(ShPhoneText *text)
{
    const ShPhonePage *page = school_timed_page(text->current);
    if (!page) { return 0; }
    const char *mapMsg = page->raw;
    int displayLength = text->display_length, returnCode = 0;
    int fractionDigits, isFraction, digit, i, arg;
    unsigned length = 0;
    text->visible[0] = 0;
    while (true) {
        switch (*mapMsg) {
        case '_': text->visible[length++] = ' '; mapMsg++; break;
        case ' ': case '\t': mapMsg++; break;
        case '~': {
            const char code = *++mapMsg;
            arg = *++mapMsg - '0';
            switch (code) {
            case 'J':

                        fractionDigits = 0;
                        isFraction     = 0;
                        digit          = 0;

                        if (text->timer == -1)
                        {
                            int32_t c;

                            mapMsg             = mapMsg + 2;
                            c                  = *mapMsg;
                            text->audio_type = arg + 1;

                            while (c != ')')
                            {
                                if (c == '.')
                                {
                                    isFraction = 1;
                                }
                                else
                                {
                                    if (isFraction != 0)
                                    {
                                        fractionDigits++;
                                    }

                                    digit *= 10;
                                    digit -= '0' - c;
                                }

                                mapMsg++;
                                c = *mapMsg;
                            }

                            digit = (digit * 4096);
                            for (i = 0; i < fractionDigits; i++)
                            {
                                digit /= 10;
                            }

                            text->timer = digit;
                            mapMsg                = mapMsg + 1;
                        }
                        else
                        {

                            while (arg != ' ' && arg != '\t')
                            {
                                arg = *++mapMsg;
                            }
                        }
                        break;

            case 'D': displayLength = 400;
                        break;
            case 'N': text->visible[length++] = '\n'; break;
            case 'E': returnCode = -1; goto finish;
            }
            mapMsg++; break;
        }
        case '\0': returnCode = 1; goto finish;
        default:
            displayLength--; text->visible[length++] = *mapMsg++;
            if (displayLength <= 0) { goto finish; }
        }
        if (length >= sizeof(text->visible) - 1) { goto finish; }
    }
finish:
    text->visible[length] = 0;
    return returnCode;
}
int sh_phone_text_step(ShPhoneText *text, int message, int32_t delta, bool skip, bool voice_dialog, uint8_t streaming)
{
    int curRolloutState, curMenuSelection;
    text->stop_audio = false;
    text->visible[0] = 0;

    #define ROLLOUT_ADVANCE 2
    #define FINISH_MAP_MSG  0xFF

    if (text->active_message != message)
    {
        text->active = false;
    }
    switch (text->active)
    {
        case false:
            text->timer            = -1;
            text->max_index           = -1;
            text->selected = 0;
            text->audio_type               = 0;
            text->current              = message;
            text->rollout                     = 0;
            text->menu                    = 0;
            text->active_message                  = message;
            text->display_length                    = 0;
            text->increment                 = ROLLOUT_ADVANCE;

            text->load_audio = true;
            text->active++;
            return -1;
        case true:
            if (voice_dialog)
            {
                if (streaming == 4)
                {
                    text->load_audio = false;
                    break;
                }
                if (text->load_audio)
                {
                    break;
                }
            }
            else
            {
                text->load_audio = false;
            }
            text->display_length += text->increment;
            text->display_length  = phone_text_clamp(text->display_length, 0, 400);
            if (text->audio_type != 0 && text->timer > 0)
            {
                text->timer -= delta;
                text->timer  = phone_text_clamp(text->timer, 0, INT_MAX);
            }
            curRolloutState = text->rollout;
            if (curRolloutState == -1)
            {
                curMenuSelection = text->menu;
                if (curMenuSelection == curRolloutState)
                {
                    if (text->max_index == curMenuSelection)
                    {
                        if (!((text->audio_type & 1) || !skip) ||
                            (text->audio_type != 0 && text->timer == 0))
                        {
                            text->menu = FINISH_MAP_MSG;
                            if (voice_dialog)
                            {
                                (text->stop_audio = true);
                            }
                            break;
                        }
                    }

                }
                else if ((!(text->audio_type & 1) && skip && text->max_index != 0) ||
                         (text->audio_type != 0 && text->timer == 0))
                {
                    if (text->max_index != -1)
                    {
                        text->max_index = -1;
                        text->menu          = FINISH_MAP_MSG;
                        break;
                    }
                    text->current++;
                    text->timer = text->max_index;

                    text->display_length = 0;
                    text->rollout  = 0;
                    if (text->audio_type == 3)
                    {
                        text->load_audio = false;
                        return 0;
                    }
                    if (voice_dialog)
                    {
                        (text->stop_audio = true);
                    }
                    text->load_audio = true;
                    return -1;
                }
            }
            else
            {
                if (skip)
                {
                    text->display_length = 400;
                }
            }
            text->rollout  = 0;
            text->menu = phone_text_string(text);
            if (text->menu != 0 && text->menu < 4)
            {
                text->rollout = -1;
            }
    }
    if (text->menu != FINISH_MAP_MSG)
    {
        return 0;
    }
    text->active         = false;
    text->display_length                    = 0;
    if (voice_dialog)
    {
        text->load_audio = true;
    }
    return text->selected + 1;
    #undef FINISH_MAP_MSG
    #undef ROLLOUT_ADVANCE
}
