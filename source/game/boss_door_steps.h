/* Generated from map1_s02 func_800DC1E0 case7. */
void sh_boss_doors_step(ShBossDoors *doors, int32_t delta, bool first_valve, ShBossDoorFrame *frame)
{
    int32_t rotY0, rotY1;
    memset(frame, 0, sizeof(*frame));

            if (doors->progress[0] == 0)
            {
                (frame->start_sound = true);
            }

            if (doors->progress[0] < 1024)
            {
                doors->speed += sh_q12_mul(delta, 682);
            }
            else
            {
                doors->speed -= sh_q12_mul(delta, 682);
                doors->speed = door_max(doors->speed, 170); 
            }

            if (doors->reverse != 0)
            {
                if (first_valve)
                {
                    rotY0 = -sh_q12_mul(delta, door_min(doors->speed, 682));
                    if ((doors->progress[0] - rotY0) > 2048)
                    {
                        rotY0 = doors->progress[0] - 2048;
                        doors->progress[0] = 2048;
                    }
                    else
                    {
                        doors->progress[0] -= rotY0;
                    }

                    rotY1 = -sh_q12_mul(delta, door_min(doors->speed, 682)) >> 1;
                    if ((doors->progress[1] - rotY1) > 1024)
                    {
                        rotY1 = doors->progress[1] - 1024;
                        doors->progress[1] = 1024;
                    }
                    else
                    {
                        doors->progress[1] -= rotY1;
                    }
                }
                else
                {
                    rotY1 = -sh_q12_mul(delta, door_min(doors->speed, 682));
                    if ((doors->progress[0] - rotY1) > 2048)
                    {
                        rotY1 = doors->progress[0] - 2048;
                        doors->progress[0] = 2048;
                    }
                    else
                    {
                        doors->progress[0] -= rotY1;
                    }

                    rotY0 = -sh_q12_mul(delta, door_min(doors->speed, 682)) >> 1;
                    if ((doors->progress[1] - rotY0) > 1024)
                    {
                        rotY0 = doors->progress[1] - 1024;
                        doors->progress[1] = 1024;
                    }
                    else
                    {
                        doors->progress[1] -= rotY0;
                    }
                }
            }
            else if (first_valve)
            {
                rotY0 = sh_q12_mul(delta, door_min(doors->speed, 682));
                if ((doors->progress[0] + rotY0) > 2048)
                {
                    rotY0 = 2048 - doors->progress[0];
                    doors->progress[0] = 2048;
                }
                else
                {
                    doors->progress[0] += rotY0;
                }

                rotY1 = sh_q12_mul(delta, door_min(doors->speed, 682)) >> 1;
                if ((doors->progress[1] + rotY1) > 1024)
                {
                    rotY1 = 1024 - doors->progress[1];
                    doors->progress[1] = 1024;
                }
                else
                {
                    doors->progress[1] += rotY1;
                }
            }
            else
            {
                rotY1 = sh_q12_mul(delta, door_min(doors->speed, 682));
                if ((doors->progress[0] + rotY1) > 2048)
                {
                    rotY1 = 2048 - doors->progress[0];
                    doors->progress[0] = 2048;
                }
                else
                {
                    doors->progress[0] += rotY1;
                }

                rotY0 = sh_q12_mul(delta, door_min(doors->speed, 682)) >> 1;
                if ((doors->progress[1] + rotY0) > 1024)
                {
                    rotY0 = 1024 - doors->progress[1];
                    doors->progress[1] = 1024;
                }
                else
                {
                    doors->progress[1] += rotY0;
                }
            }

            doors->rotation[0] += rotY0;
            doors->rotation[1] += rotY1;

            (frame->volume = doors->speed >> 3);

            if (doors->progress[0] == 2048 && doors->progress[1] == 1024)
            {
                
                doors->rotation[0] = (doors->rotation[0] + 0x1040) & 0xC00;
                doors->rotation[1] = (doors->rotation[1] + 0x1040) & 0xC00;

                (frame->stop_sound = true);

                (frame->finished = true);
            }
}
