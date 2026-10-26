#include "sh3ds/vc.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const float initial[3] = {0, -2, 0}, look[3] = {0, -2, 10};
    ShVc vc;
    ShCamera camera;
    sh_vc_init(&vc, NULL, 0);
    sh_vc_script_update(&vc, initial, look, NULL, true, true, 1.0f / 30, &camera);
    assert(camera.pos[0] == 0 && camera.pos[1] == 2 && camera.pos[2] == 0);
    assert(camera.yaw == 0 && camera.pitch == 0 && camera.fov_deg == SH_VC_FOV_DEG);
    ShVc zero = vc, defaults = vc;
    ShVcScriptParams params = {0};
    const float target[3] = {10, -4, 20}, target_look[3] = {20, -8, 30};
    for (int frame = 0; frame < 300; frame++) {
        sh_vc_script_update(&zero, target, target_look, &params, false, false, 1.0f / 30, &camera);
        sh_vc_script_update(&defaults, target, target_look, NULL, false, false, 1.0f / 30, &camera);
        assert(memcmp(&zero, &defaults, sizeof(zero)) == 0);
    }
    for (int rate = 30; rate <= 60; rate += 30) {
        ShVc slow = vc, fast = vc;
        params = (ShVcScriptParams){{0, 0, 0.75f, 0.25f}, {0, 0, 0, 0}};
        for (int frame = 0; frame < rate; frame++) {
            sh_vc_script_update(&slow, target, target_look, &params, false, false, 1.0f / rate, &camera);
            assert(hypotf(slow.cam_vel[0], slow.cam_vel[2]) <= 0.75001f);
            assert(fabsf(slow.cam_vel[1]) <= 0.25001f);
            assert(isfinite(camera.yaw) && isfinite(camera.pitch));
            sh_vc_script_update(&fast, target, target_look, NULL, false, false, 1.0f / rate, &camera);
        }
        assert(fast.cam_pos[2] > slow.cam_pos[2]);
        const float old_yaw = slow.ang[1];
        sh_vc_script_update(&slow, initial, look, NULL, true, false, 0, &camera);
        assert(slow.cam_pos[0] == initial[0] && slow.cam_pos[1] == initial[1]);
        assert(slow.ang[1] == old_yaw);
        sh_vc_script_update(&slow, initial, look, NULL, false, true, 0, &camera);
        assert(camera.yaw == 0 && camera.pitch == 0);
    }
    for (int rate = 30; rate <= 60; rate += 30) {
        ShVc position_only;
        sh_vc_init(&position_only, NULL, 0);
        const float player[3] = {0, 0, 0};
        sh_vc_update(&position_only, player, 0, false, 1.0f / rate, &camera);
        const ShVcScriptParams movement = {{2.5f, 0.75f, 1.5f, 0.5f}, {0}};
        sh_vc_camera_target(&position_only, target, &movement, false);
        ShVc reference = position_only;
        const float old_watch = position_only.ang[1];
        for (int frame = 0; frame < rate * 3; frame++) {
            sh_vc_update(&position_only, player, 0, false, 1.0f / rate, &camera);
            sh_vc_script_update(&reference, target, target_look, &movement, false, false, 1.0f / rate, &camera);
            assert(!memcmp(position_only.cam_pos, reference.cam_pos, sizeof(reference.cam_pos)));
            assert(!memcmp(position_only.cam_vel, reference.cam_vel, sizeof(reference.cam_vel)));
        }
        assert(position_only.user_camera && position_only.ang[1] != old_watch);
        const float angle = position_only.ang[1];
        sh_vc_camera_target(&position_only, initial, NULL, true);
        sh_vc_update(&position_only, player, 0, false, 0, &camera);
        assert(!memcmp(position_only.cam_pos, initial, sizeof(initial)));
        assert(position_only.ang[1] == angle && !position_only.user_camera_warp);
        sh_vc_camera_release(&position_only, false);
        assert(!position_only.user_camera && !position_only.warp);
        sh_vc_camera_release(&position_only, true);
        assert(position_only.warp);
    }
    puts("PASS: scripted camera warps/defaults/speed caps and position-only override retaining automatic watch at30/60fps");
    return 0;
}
