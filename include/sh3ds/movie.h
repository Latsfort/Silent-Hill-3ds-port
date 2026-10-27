#pragma once

#include "sh3ds/tim.h"
#include <stdio.h>

#define SH_MOVIE_MAX_WIDTH 320
#define SH_MOVIE_MAX_HEIGHT 240

typedef struct ShMovie {
    FILE *file;
    uint32_t frame_count, fps_num, fps_den;
    uint32_t frame_index, revision;
    bool failed;
    ShTim image;
    uint8_t pixels[SH_MOVIE_MAX_WIDTH * SH_MOVIE_MAX_HEIGHT * 2];
} ShMovie;

bool sh_movie_open(ShMovie *movie, const char *path);
/* True when every frame has elapsed or the retail skip button was pressed. */
bool sh_movie_step(ShMovie *movie, uint32_t elapsed_ms, bool skip);
void sh_movie_close(ShMovie *movie);
