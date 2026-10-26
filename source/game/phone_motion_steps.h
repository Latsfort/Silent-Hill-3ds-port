/* Generated from MAP1_S02 MapEvent_CutscenePhoneCall path/camera calls. */
void sh_phone_motion_begin(ShPhoneMotion *motion, const ShPathPlayer *player)
{
    memset(motion, 0, sizeof(*motion));
    motion->waypoint = (ShPathPoint){83148, player->position.vy, 587776};
    motion->heading = -1024;
}
bool sh_phone_path_step(ShPhoneMotion *motion, ShPathPlayer *player, int32_t delta)
{
    return sh_path_player_step(&motion->path, player, 53, &motion->waypoint, motion->heading, 1, delta);
}
bool sh_phone_camera_view(ShPhoneMotion *motion, const ShPathPlayer *player, int step, int32_t delta, ShPhoneView *view)
{
    switch (step) {
    case 5:
        view->position = (ShPathPoint){73973, -8151, 584785};
        view->warp_position = true;
        view->look = (ShPathPoint){85811, 1105, 591339};
        view->warp_look = true;
        return true;
    case 9:
        view->position = (ShPathPoint){player->position.vx + 16384, -6144, player->position.vz};
        view->warp_position = true;
        view->look = (ShPathPoint){player->position.vx, -4096, player->position.vz};
        view->warp_look = true;
        return true;
    case 15:
        view->position = (ShPathPoint){player->position.vx + 16384 + phone_tween(motion, 0, -11468, 26624, delta), -6144, player->position.vz};
        view->warp_position = false;
        view->look = (ShPathPoint){player->position.vx + phone_tween(motion, 1, -11468, 26624, delta), -4096, player->position.vz};
        view->warp_look = false;
        return true;
    case 16:
        view->position = (ShPathPoint){71720, -13025, 590028};
        view->warp_position = true;
        view->look = (ShPathPoint){83066, -1720, 586629};
        view->warp_look = true;
        return true;
    case 28:
        view->position = (ShPathPoint){71720, -13025, 590028};
        view->warp_position = true;
        view->look = (ShPathPoint){83066, -1720, 586629};
        view->warp_look = true;
        return true;
    default: return false;
    }
}
