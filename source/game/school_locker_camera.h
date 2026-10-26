/* Generated from map1_s03 Map_WorldObjectsUpdate locker camera branches. */
static void locker_room_camera(ShTown *town, const ShSavegame *save, float x, float z, float dt)
{
    ShLockerCameraTarget target = {0};
if (roof_locker_in_room(x, z)) {
if (SH_Q12(x) < -235520 &&
                SH_Q12(-z) < 88473)
            {
                if (!sh_save_event_get(save, 100))
                {
                    if (SH_Q12(x) > -245760)
                    {
                        if (town->school.locker_camera_mode == 2)
                        {
                            town->school.locker_camera_mode = 0;
                        }

                        
                        locker_camera_target(&target, false, -250593, -12288, SH_Q12(-z) + 2007, town->school.locker_camera_mode == 0);
                        locker_camera_target(&target, true, -239534, -1269, SH_Q12(-z) - 2949, town->school.locker_camera_mode == 0);

                        town->school.locker_camera_mode = 1;
                    }
                    else
                    {
                        locker_camera_target(&target, false, -239534, -7700, 91340, true);
                        locker_camera_target(&target, true, -249364, -5038, 78479, true);
                        town->school.locker_camera_mode = 2;
                    }
                }
                else if (sh_save_event_get(save, 107))
                {
                    if (SH_Q12(-z) > 76185)
                    {
                        
                        locker_camera_target(&target, false, -246865, -1679, SH_Q12(-z) - 9216, town->school.locker_camera_mode == 0);
                        locker_camera_target(&target, true, -244736, -4464, SH_Q12(-z) + 6758, town->school.locker_camera_mode == 0);

                        town->school.locker_camera_mode = 1;
                    }
                    else
                    {
                        town->school.locker_camera_mode = 0;

                        
                        locker_camera_target(&target, false, -246374, -13516, 84049, true);
                        locker_camera_target(&target, true, -245678, -3481, 71106, true);
                    }
                }
            }
            else
            {
                if (town->school.locker_camera_mode == 2)
                {
                    town->school.locker_camera_mode = 1;
                }

                locker_camera_return(town, town->school.locker_camera_mode);
                town->school.locker_camera_mode = 0;
            }
    }

    if ( sh_save_event_get(save, 104) &&
        !sh_save_event_get(save, 105))
    {
        
        locker_camera_target(&target, false, -248340, -16957, 71352, true);
        if (SH_Q12(-z) < 75776)
        {
            locker_camera_target(&target, true, -245760, 0, 75776, false);
        }
        else
        {
            locker_camera_target(&target, true, -245760, 0, SH_Q12(-z), false);
        }

        town->school.locker_camera_mode = 0;
    }

    
    if (target.set) { locker_camera_apply(town, &target, NULL, dt); }
}
