#include "sh3ds/boss_doors.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned cases = 0;
    const int deltas[] = {136, 68};
    for (unsigned rate = 0; rate < sizeof(deltas) / sizeof(deltas[0]); rate++) {
        for (int first = 0; first < 2; first++) {
            for (int reverse = 0; reverse < 2; reverse++) {
                for (int a = 0; a < 4; a++) {
                    for (int b = 0; b < 4; b++) {
                        ShBossDoors doors = {.rotation = {a * 1024, b * 1024}, .reverse = reverse};
                        ShBossDoorFrame frame;
                        unsigned ticks = 0, starts = 0, stops = 0;
                        do {
                            const int progress = doors.progress[0];
                            sh_boss_doors_step(&doors, deltas[rate], first, &frame);
                            assert(frame.start_sound == (progress == 0));
                            starts += frame.start_sound; stops += frame.stop_sound;
                            assert(doors.progress[0] >= progress && doors.progress[0] <= 2048);
                            assert(doors.progress[1] >= 0 && doors.progress[1] <= 1024);
                            assert(frame.volume == doors.speed >> 3);
                            assert(++ticks < 5000);
                        } while (!frame.finished);
                        assert(starts >= 1 && stops == 1 && doors.progress[0] == 2048 && doors.progress[1] == 1024);
                        const int sign = reverse ? -1 : 1;
                        const int expected0 = (a * 1024 + sign * (first ? 2048 : 1024) + 0x1040) & 0xC00;
                        const int expected1 = (b * 1024 + sign * (first ? 1024 : 2048) + 0x1040) & 0xC00;
                        assert(doors.rotation[0] == expected0 && doors.rotation[1] == expected1);
                        cases++;
                    }
                }
            }
        }
    }
    printf("PASS: %u original door combinations, both valves/directions,30/60Hz, progress clamps, sound requests and final quarter turns\n", cases);
    return 0;
}
