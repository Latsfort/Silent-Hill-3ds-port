/* Generated from retail scripted walk/turn speed and yaw calculations. */
void sh_path_locomotion(ShPath *path, ShPathPlayer *player, int keyframe, int32_t dt)
{
    int32_t yaw_step = 0, headingAngle = 0, speed = player->move_speed;
    switch (player->state) {
    case 53:

            if (player->move_speed > 5734)
            {
                player->move_speed -= PATH_TIMESTEP(dt, 1638) * 2;
                if (player->move_speed < 5734)
                {
                    player->move_speed = 5734;
                }
            }
            else if (player->move_speed < 5734)
            {
                if (keyframe >= 2)
                {
                    player->move_speed += PATH_TIMESTEP(dt, 1638);
                }

                player->move_speed = sh_q12_clamp(player->move_speed,
                                              0,
                                              5734);
            }

            speed = player->move_speed;

            path_model_status(&player->extra_model, 2, false);
            path_model_status(&player->model, 2, false);

            if (player->extra_model.control_state == 0)
            {
                player->extra_model.control_state++;
            }

            switch (player->cutscene_state)
            {
                case 0:
                    sh_path_state_set(player, 54);
                    break;

                case 3:
                    sh_path_state_set(player, 56);
                    break;

                case 4:
                    sh_path_state_set(player, 57);
                    break;
            }
            
            path_animation_reset(player);

            headingAngle = player->heading_angle = 0;
            break;
    case 56:

            if (player->move_speed != 0)
            {
                player->move_speed -= PATH_TIMESTEP(dt, 1638) * 2;
                if (player->move_speed < 0)
                {
                    player->move_speed = 0;
                }
            }

            speed = player->move_speed;
            yaw_step = dt * 7;

            path_model_status(&player->extra_model, 13, false);
            path_model_status(&player->model, 13, false);

            if (player->extra_model.control_state == 0)
            {
                player->extra_model.control_state++;
            }

            switch (player->cutscene_state)
            {
                case 0:
                    sh_path_state_set(player, 54);
                    break;

                case 1:
                    sh_path_state_set(player, 53);
                    break;

                case 4:
                    sh_path_state_set(player, 57);
                    break;
            }
            
            path_animation_reset(player);

            headingAngle             = 
            player->heading_angle = 0;
            path->flex_y   = -320;
            break;
    case 57:

            if (player->move_speed != 0)
            {
                player->move_speed -= PATH_TIMESTEP(dt, 1638) * 2;
                if (player->move_speed < 0)
                {
                    player->move_speed = 0;
                }
            }

            speed = player->move_speed;
            yaw_step = dt * -7;

            path_model_status(&player->extra_model, 12, false);
            path_model_status(&player->model, 12, false);

            if (player->extra_model.control_state == 0)
            {
                player->extra_model.control_state++;
            }

            switch (player->cutscene_state)
            {
                case 0:
                    sh_path_state_set(player, 54);
                    break;

                case 1:
                    sh_path_state_set(player, 53);
                    break;

                case 3:
                    sh_path_state_set(player, 56);
                    break;
            }
            
            path_animation_reset(player);

            headingAngle             = 
            player->heading_angle = 0;
            path->flex_y   = 320;
            break;
    default: break;
    }
    player->move_speed = speed;
    player->rotation.vy = path_angle(player->rotation.vy + (yaw_step >> 4) + 4096);
    player->heading_angle = path_angle(player->rotation.vy + headingAngle);
}
