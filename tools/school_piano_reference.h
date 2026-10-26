/* Generated retail PianoPuzzle_Control, unchanged control flow. */
static void reference_control(bool playNote)
{

    s16    cursorX;
    s16    cursorY;
    q19_12 curCursorX;
    s32    keyIdx;
    s32    i;

    if (!playNote)
    {
        g_PianoKeyCounter = 0;
        g_PianoCursorY    = 0;
        g_PianoCursorX    = 0;
        D_800DD594        = false;
        return;
    }

    if (g_SysWork.sysStateSteps[1] & 1)
    {
        if (g_SysWork.sysStateSteps[1] == 1)
        {
            Event_WaitTimer(Q12(0.3f), true);
        }
        if (g_SysWork.sysStateSteps[1] == 3)
        {
            Event_WaitTimer(Q12(0.2f), true);
        }
        if (g_SysWork.sysStateSteps[1] == 5)
        {
            Event_WaitTimer(Q12(0.3f), true);
        }
        return;
    }

    if (g_SysWork.sysStateSteps[1] == 4)
    {
        SD_Call(1421);
        SysWork_StateStepSet(1, 5);
        return;
    }

    g_SysWork.sysStateSteps[1] = 0;

    if (g_PianoKeyCounter == 5)
    {
        Savegame_EventFlagSet(75);
        SysWork_StateStepIncrement(0);
        return;
    }

    if (g_Controller0->buttonFlags.clicked & g_GameWorkPtr->config.controllerConfig.cancel)
    {
        SysWork_StateStepIncrement(0);
        return;
    }

    g_PianoCursorX += (g_Controller0->normalizedSticks.sticks_0.leftX * Q12(4.0f)) / 75;
    g_PianoCursorX  = CLAMP_RANGE(g_PianoCursorX, Q12(-89.0f), Q12(85.0f));

    g_PianoCursorY += (g_Controller0->normalizedSticks.sticks_0.leftY * Q12(4.0f)) / 75;
    g_PianoCursorY  = CLAMP_RANGE(g_PianoCursorY, Q12(-71.0f), Q12(84.0f));

    Game_TimerUpdate();

    cursorX = FP_FROM(g_PianoCursorX, Q12_SHIFT) + 8;
    cursorY = FP_FROM(g_PianoCursorY, Q12_SHIFT) + 8;
    Gfx_CursorDraw(cursorX, cursorY, 8, 8, 0, 64, 32, 32, 128, 192, 0, 12);

    if (g_Controller0->buttonFlags.clicked & g_GameWorkPtr->config.controllerConfig.enter)
    {
        if (FP_FROM(g_PianoCursorY, Q12_SHIFT) >= 34)
        {
            curCursorX = FP_FROM(g_PianoCursorX, Q12_SHIFT);
            if (curCursorX < -64)
            {
                keyIdx = 0;
            }
            else if (curCursorX < -39)
            {
                keyIdx = 2;
            }
            else if (curCursorX < -14)
            {
                keyIdx = 4;
            }
            else if (curCursorX < 11)
            {
                keyIdx = 5;
            }
            else if (curCursorX < 36)
            {
                keyIdx = 7;
            }
            else if (curCursorX < 61)
            {
                keyIdx = 9;
            }
            else
            {
                keyIdx = ARRAY_SIZE(g_PianoKeys);
            }
        }
        else
        {
            for (i = 0; i < ARRAY_SIZE(g_PianoKeys); i++)
            {
                if (FP_FROM(g_PianoCursorX, Q12_SHIFT) < g_PianoKeys[i])
                {
                    break;
                }
            }

            keyIdx = i;
        }

        switch (keyIdx)
        {
            case 0:
                SysWork_StateStepSet(1, 1);
                SD_Call(1438);
                g_PianoCursorX = Q12(-77.0f);
                break;

            case 1:
                SysWork_StateStepSet(1, 3);
                SD_Call(1445);
                g_PianoCursorX = Q12(-64.0f);
                break;

            case 2:
                SysWork_StateStepSet(1, 3);
                SD_Call(1445);
                g_PianoCursorX = Q12(-52.0f);
                break;

            case 3:
                SysWork_StateStepSet(1, 1);
                SD_Call(1439);
                g_PianoCursorX = Q12(-39.0f);
                break;

            case 4:
                SysWork_StateStepSet(1, 1);
                SD_Call(1440);
                g_PianoCursorX = Q12(-27.0f);
                break;

            case 5:
                SysWork_StateStepSet(1, 1);
                SD_Call(1441);
                g_PianoCursorX = Q12(-2.0f);
                break;

            case 6:
                SysWork_StateStepSet(1, 1);
                SD_Call(1442);
                g_PianoCursorX = Q12(8.0f);
                break;

            case 7:
                SysWork_StateStepSet(1, 3);
                SD_Call(1445);
                g_PianoCursorX = Q12(23.0f);
                break;

            case 8:
                SysWork_StateStepSet(1, 1);
                SD_Call(1443);
                g_PianoCursorX = Q12(36.0f);
                break;

            case 9:
                SysWork_StateStepSet(1, 3);
                SD_Call(1445);
                g_PianoCursorX = Q12(48.0f);
                break;

            case 10:
                SysWork_StateStepSet(1, 3);
                SD_Call(1445);
                g_PianoCursorX = Q12(64.0f);
                break;

            case 11:
                SysWork_StateStepSet(1, 1);
                SD_Call(1444);
                g_PianoCursorX = Q12(73.0f);
                break;
        }

        switch (keyIdx)
        {
            case 0:
            case 3:
            case 4:
            case 5:
            case 6:
            case 8:
            case 11:
                g_PianoKeyCounter = 0;
                return;

            default:
                if (keyIdx == g_PianoKeySequence[0])
                {
                    g_PianoKeyCounter = 1;
                    return;
                }

                if (g_PianoKeySequence[g_PianoKeyCounter] == keyIdx)
                {
                    g_PianoKeyCounter++;
                    return;
                }

                g_PianoKeyCounter = 0;
                break;
        }
    }

}
