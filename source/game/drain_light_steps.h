/* Generated from DrainageValveInteract case 9 light attributes. */
void sh_drain_camera_light(ShPathPoint lightPos, ShPathPoint lookAtPos, ShDrainLight *light)
{
light->rotation.vx   = -sh_fixed_atan2(lookAtPos.vy - lightPos.vy,
                                                       drain_magnitude_q6(lookAtPos.vx - lightPos.vx,
                                                                                 lookAtPos.vz - lightPos.vz));

                light->rotation.vy = sh_fixed_atan2(lookAtPos.vx - lightPos.vx, lookAtPos.vz - lightPos.vz);
                light->rotation.vz = 0;
                
                light->position.vx = lightPos.vx;
                light->position.vy = drain_max(-6144, lightPos.vy);
                light->position.vz = lightPos.vz;
}
