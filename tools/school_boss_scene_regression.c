#include "sh3ds/school_boss_scene.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    ShSchoolBossScene *scene;
    ShSavegame *save;
    int camera_calls, returns, autosaves, gameplay_calls, echo_calls, sounds, attributes;
    bool frozen;
} Fixture;
static void control(void *u, bool frozen) { Fixture *f = u; f->frozen = frozen; }
static void border(void *u) { (void)u; }
static void camera(void *u, bool look, const ShPathPoint *p)
{
    Fixture *f = u;
    assert(p->vx == SH_Q12(19.72f));
    assert(p->vy == (look ? SH_Q12(-3.3f) : SH_Q12(-5.2f)));
    assert(p->vz == (look ? SH_Q12(-24) : SH_Q12(-27.6f)));
    f->camera_calls++;
}
static void camera_return(void *u, bool immediate) { Fixture *f = u; (void)immediate; f->returns++; }
static bool fade(void *u, int command, bool out, int mode, int32_t speed, bool substep)
{
    Fixture *f = u;
    assert(command == 0 || command == 1);
    assert(!substep && (mode == 0 || mode == 2));
    if (f->scene->step == 4 && out) {
        assert(mode == 0 && speed == SH_Q12(2.5f));
        assert(!sh_save_event_get(f->save, 132));
    }
    return command == 1;
}
static void echo(void *u) { Fixture *f = u; assert(f->scene->step > 0 && f->scene->step < 5); f->echo_calls++; }
static void gameplay(void *u) { Fixture *f = u; f->gameplay_calls++; }
static void autosave(void *u) { Fixture *f = u; assert(sh_save_event_get(f->save, 130)); f->autosaves++; }
static void sound(void *u, int id) { Fixture *f = u; assert(id == 1359); f->sounds++; }
static void attributes(void *u, int id, int pan, int volume, int pitch)
{ Fixture *f = u; assert(id == 1359 && pan == 0 && volume >= 0 && pitch == 0); f->attributes++; }

int main(void)
{
    unsigned paths = 0;
    for (int delta = 68; delta <= 136; delta += 68) {
        for (int skip_step = -1; skip_step <= 5; ++skip_step) {
            ShSavegame save = {0};
            ShSchoolBossScene scene;
            sh_school_boss_scene_begin(&scene);
            uint8_t particles[500];
            ShSchoolBossParticleConfig config = {0};
            for (int i = 0; i < 500; ++i) { particles[i] = i % 16; }
            Fixture f = {.scene = &scene, .save = &save};
            ShSchoolBossSceneContext ctx = {.save = &save, .delta = delta, .raw_delta = delta,
                .particle_states = particles, .particle_count = 500, .particle_stride = 1, .particles = &config, .user = &f, .control = control,
                .border_show = border, .camera = camera, .camera_return = camera_return, .fade = fade,
                .echo = echo, .gameplay = gameplay, .autosave = autosave, .sound = sound, .sound_attributes = attributes};
            unsigned frames = 0;
            while (!scene.finished && frames++ < 4000) {
                ctx.skip = scene.step == skip_step;
                assert(sh_school_boss_intro(&scene, &ctx));
            }
            assert(scene.finished && frames < 4000 && !f.frozen);
            assert(f.camera_calls == 2 && f.autosaves == 1 && f.gameplay_calls == 1);
            assert(sh_save_event_get(&save, 130) && scene.light == SH_Q12(1.3843f));
            if (skip_step >= 3) {
                assert(f.returns == 2 && !config.field_1 && !config.field_10);
                for (int i = 0; i < 500; ++i) {
                    int expected = i % 16;
                    if (expected >= 8 && expected < 12) { expected = 12; }
                    assert(particles[i] == expected);
                }
            } else {
                assert(f.returns == 1 && sh_save_event_get(&save, 129));
            }
            sh_school_boss_scene_begin(&scene);
            scene.light = SH_Q12(1.3843f);
            ctx.skip = false;
            frames = 0;
            while (!scene.finished && frames++ < 4000) {
                assert(sh_school_boss_outro(&scene, &ctx));
            }
            assert(scene.finished && frames < 4000 && scene.light == 0);
            assert(!sh_save_event_get(&save, 129) && sh_save_event_get(&save, 132));
            assert(f.echo_calls > 0 && f.sounds >= 1 && f.attributes > 0 && f.gameplay_calls == 2);
            assert(config.field_1 == 1 && config.field_2 == 3);
            paths++;
        }
    }
    printf("PASS: %u boss intro/skip/outro paths, exact cameras, particle fade requests, light ramps, sound, late flags and autosave\n", paths);
    return 0;
}
