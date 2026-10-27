#include "sh3ds/movie.h"

#include <limits.h>
#include <string.h>

static uint32_t s_revision;

static uint32_t movie_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

void sh_movie_close(ShMovie *movie)
{
    if (movie->file) { fclose(movie->file); }
    movie->file = NULL;
}

static bool movie_read(ShMovie *movie, uint32_t index)
{
    const size_t size = (size_t)movie->image.stride * (size_t)movie->image.height;
    const uint64_t offset = 32u + (uint64_t)index * size;
    if (offset > LONG_MAX || fseek(movie->file, (long)offset, SEEK_SET) != 0 ||
        fread(movie->pixels, 1, size, movie->file) != size) {
        movie->failed = true;
        return false;
    }
    movie->frame_index = index;
    movie->revision = ++s_revision;
    return true;
}

bool sh_movie_open(ShMovie *movie, const char *path)
{
    sh_movie_close(movie);
    memset(movie, 0, sizeof(*movie));
    movie->file = fopen(path, "rb");
    if (!movie->file) { movie->failed = true; return false; }
    uint8_t header[32];
    if (fread(header, 1, sizeof(header), movie->file) != sizeof(header) || memcmp(header, "SH3DV001", 8)) {
        sh_movie_close(movie);
        movie->failed = true;
        return false;
    }
    const uint32_t width = movie_u32(header + 8);
    const uint32_t height = movie_u32(header + 12);
    movie->frame_count = movie_u32(header + 16);
    movie->fps_num = movie_u32(header + 20);
    movie->fps_den = movie_u32(header + 24);
    const uint64_t expected = 32u + (uint64_t)width * height * 2 * movie->frame_count;
    if (!width || width > SH_MOVIE_MAX_WIDTH || !height || height > SH_MOVIE_MAX_HEIGHT ||
        !movie->frame_count || !movie->fps_num || !movie->fps_den || movie_u32(header + 28) ||
        expected > LONG_MAX || fseek(movie->file, 0, SEEK_END) != 0 || ftell(movie->file) != (long)expected) {
        sh_movie_close(movie);
        movie->failed = true;
        return false;
    }
    movie->image = (ShTim){.bpp = 16, .width = (int)width, .height = (int)height,
        .pixels = movie->pixels, .stride = (int)width * 2};
    if (!movie_read(movie, 0)) { sh_movie_close(movie); return false; }
    return true;
}

bool sh_movie_step(ShMovie *movie, uint32_t elapsed_ms, bool skip)
{
    if (!movie->file || movie->failed || skip) { return true; }
    const uint64_t index = (uint64_t)elapsed_ms * movie->fps_num / ((uint64_t)movie->fps_den * 1000u);
    if (index >= movie->frame_count) { return true; }
    if (index != movie->frame_index && !movie_read(movie, (uint32_t)index)) { return true; }
    return false;
}
