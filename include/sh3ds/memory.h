#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShArena {
    uint8_t *base;
    size_t capacity;
    size_t used;
} ShArena;

bool sh_memory_init(void);
void sh_memory_shutdown(void);

ShArena *sh_memory_frame_arena(void);
/* Allocated once at startup by the map streamer, never reset. */
ShArena *sh_memory_stream_arena(void);
ShArena *sh_memory_level_arena(void);

void sh_arena_reset(ShArena *arena);
void *sh_arena_alloc(ShArena *arena, size_t size, size_t align);
size_t sh_arena_used(const ShArena *arena);
size_t sh_arena_capacity(const ShArena *arena);

#ifdef __cplusplus
}
#endif
