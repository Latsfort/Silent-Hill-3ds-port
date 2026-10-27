#include "sh3ds/movie.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void run(unsigned fps)
{
    ShMovie movie = {0};
    assert(sh_movie_open(&movie, "romfs/game/movie/016.shv"));
    assert(movie.image.width == 320 && movie.image.height == 208 && movie.frame_count == 119);
    assert(movie.fps_num == 15 && movie.fps_den == 1);
    bool seen[119] = {0};
    seen[0] = true;
    FILE *reference = fopen("romfs/game/movie/016.shv", "rb");
    assert(reference);
    uint8_t expected[320 * 208 * 2];
    unsigned final_time = 0;
    for (unsigned frame = 0; frame < fps * 10; frame++) {
        const unsigned milliseconds = frame * 1000 / fps;
        if (sh_movie_step(&movie, milliseconds, false)) {
            final_time = milliseconds;
            break;
        }
        assert(movie.frame_index < 119);
        seen[movie.frame_index] = true;
        assert(fseek(reference, 32 + (long)movie.frame_index * sizeof(expected), SEEK_SET) == 0);
        assert(fread(expected, 1, sizeof(expected), reference) == sizeof(expected));
        assert(memcmp(expected, movie.image.pixels, sizeof(expected)) == 0);
    }
    for (unsigned i = 0; i < 119; i++) { assert(seen[i]); }
    assert(final_time >= 7934 && final_time < 7934 + 1000 / fps + 1 && !movie.failed);
    assert(fclose(reference) == 0);
    sh_movie_close(&movie);
    assert(!movie.file);
    assert(sh_movie_open(&movie, "romfs/game/movie/016.shv"));
    assert(sh_movie_step(&movie, 0, true) && movie.frame_index == 0 && !movie.failed);
    sh_movie_close(&movie);
}

int main(void)
{
    run(30);
    run(60);
    ShMovie movie = {0};
    assert(!sh_movie_open(&movie, "build/nonexistent-movie.shv") && movie.failed && !movie.file);
    FILE *bad = fopen("build/invalid-movie.shv", "wb");
    assert(bad && fwrite("SH3DV001", 1, 8, bad) == 8 && fclose(bad) == 0);
    assert(!sh_movie_open(&movie, "build/invalid-movie.shv") && movie.failed && !movie.file);
    remove("build/invalid-movie.shv");
    puts("PASS: all 119 source frames, 15-fps timing at 30/60 fps, retail skip and invalid assets");
    return 0;
}
