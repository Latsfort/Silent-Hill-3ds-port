/* Generated from map1_s02 func_800DBB7C case3. */
void sh_bathroom_motion_step(ShBathroomMotion *motion, int32_t delta, ShBathroomFrame *frame)
{
    int16_t angleAdd;
    memset(frame, 0, sizeof(*frame));

            
            if (motion->elapsed > 4096)
            {
                frame->position = (ShPathPoint){bathroom_tween(motion, -6144, 20480, 0, delta) + 86016, -2048, 245760};
                frame->params.camera[0] = 0 / 4096.0f;
                frame->params.camera[1] = 0 / 4096.0f;
                frame->params.camera[2] = 0 / 4096.0f;
                frame->params.camera[3] = 0 / 4096.0f;
                frame->camera = true;
                frame->look = (ShPathPoint){bathroom_tween(motion, -6144, 20480, 0, delta) + 69632, bathroom_tween(motion, -6144, 20480, 1, delta) - 2048, 245760};
                frame->params.watch[0] = 12288 / 4096.0f;
                frame->params.watch[1] = 0 / 4096.0f;
                frame->params.watch[2] = 1228 / 4096.0f;
                frame->params.watch[3] = 0 / 4096.0f;
                frame->camera = true;
            }

            motion->elapsed += delta;
            if (motion->elapsed < 1228)
            {
                angleAdd = 0;
            }
            else if (motion->elapsed < 2048)
            {
                angleAdd = -512;
            }
            else if (motion->elapsed < 4096)
            {
                angleAdd = 56;
            }
            else if (motion->elapsed < 34816)
            {
                if (motion->elapsed - 4096 <= 24576)
                {
                    angleAdd = sh_q12_mul(motion->elapsed - 4096, 56) - 398;
                }
                else
                {
                    angleAdd = -62;
                }
            }
            else
            {
                angleAdd = 0;
                (frame->finished = true);
            }

            motion->rotation += sh_q12_mul(delta, angleAdd);
            if (motion->rotation < -1280)
            {
                motion->rotation = -1280;
            }
}
