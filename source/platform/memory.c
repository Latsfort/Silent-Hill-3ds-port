#include "sh3ds/memory.h"
#include "sh3ds/config.h"

#include <string.h>

static uint8_t s_storage[SH3DS_FRAME_ARENA_BYTES + SH3DS_STREAM_ARENA_BYTES + SH3DS_LEVEL_ARENA_BYTES];
static ShArena s_frame;
static ShArena s_stream;
static ShArena s_level;
static bool s_ready;

static void arena_bind(ShArena *arena, uint8_t *base, size_t capacity)
{
    arena->base = base;
    arena->capacity = capacity;
    arena->used = 0;
}

bool sh_memory_init(void)
{
    if (s_ready) {
        return true;
    }

    uint8_t *cursor = s_storage;
    arena_bind(&s_frame, cursor, SH3DS_FRAME_ARENA_BYTES);
    cursor += SH3DS_FRAME_ARENA_BYTES;
    arena_bind(&s_stream, cursor, SH3DS_STREAM_ARENA_BYTES);
    cursor += SH3DS_STREAM_ARENA_BYTES;
    arena_bind(&s_level, cursor, SH3DS_LEVEL_ARENA_BYTES);
    s_ready = true;
    return true;
}

void sh_memory_shutdown(void)
{
    s_frame.used = 0;
    s_stream.used = 0;
    s_level.used = 0;
    s_ready = false;
}

ShArena *sh_memory_frame_arena(void)
{
    return &s_frame;
}

ShArena *sh_memory_stream_arena(void)
{
    return &s_stream;
}

ShArena *sh_memory_level_arena(void)
{
    return &s_level;
}

void sh_arena_reset(ShArena *arena)
{
    if (arena) {
        arena->used = 0;
    }
}

void *sh_arena_alloc(ShArena *arena, size_t size, size_t align)
{
    if (!arena || size == 0) {
        return NULL;
    }
    if (align < 1) {
        align = 1;
    }

    uintptr_t ptr = (uintptr_t)(arena->base + arena->used);
    uintptr_t aligned = (ptr + (align - 1)) & ~(uintptr_t)(align - 1);
    size_t padding = (size_t)(aligned - ptr);
    size_t total = padding + size;

    if (arena->used + total > arena->capacity) {
        return NULL;
    }

    arena->used += total;
    memset((void *)aligned, 0, size);
    return (void *)aligned;
}

size_t sh_arena_used(const ShArena *arena)
{
    return arena ? arena->used : 0;
}

size_t sh_arena_capacity(const ShArena *arena)
{
    return arena ? arena->capacity : 0;
}
