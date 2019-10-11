/*
  NOTE:

  HANDMADE_INTERNAL:
      0 - for public release
      1 - for developer only

  HANDMADE_SLOW:
      0 - no slow code
      1 - slow code welcome
 */

#include "handmade_platform.h"

#define internal static
#define local_persist static
#define global_variable static

#if HANDMADE_SLOW
#define Assert(expression) if(!(expression)) { *(int *)0 = 0; }
#else
#define Assert(expression)
#endif

#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) ((uint64)Megabytes(value) * 1024)
#define Terabytes(value) ((uint64)Gigabytes(value) * 1024)

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

inline uint32
SafeTruncateUInt64(uint64 value)
{
    Assert(value <= 0xFFFFFFFF);
    uint32 result = (uint32)value;

    return result;
}

inline game_controller_input *GetController(game_input *input, int unsigned controllerIndex)
{
    Assert(controllerIndex < ArrayCount(input->controllers));

    return &input->controllers[controllerIndex];
}

struct memory_arena
{
    memory_index size;
    uint8 *base;
    memory_index used;
};

internal void
InitializeArena
(
    memory_arena *arena,
    memory_index size,
    uint8 *base
)
{
    arena->size = size;
    arena->base = base;
    arena->used = 0;
}

#define PushStruct(arena, type) (type *)PushSize_(arena, sizeof(type))
#define PushArray(arena, count, type) (type *)PushSize_(arena, (count) * sizeof(type))
void *
PushSize_(memory_arena *arena, memory_index size)
{
    Assert(arena->used + size <= arena->size)
    
    void *result = arena->base + arena->used;
    arena->used += size;

    return result;
}

#include "handmade_intrinsics.h"
#include "handmade_tile.h"

struct world
{
    tile_map *tileMap;
};

struct loaded_bitmap
{
    int32 width;
    int32 height;
    uint32 *pixels;
};

struct hero_bitmaps
{
    int32 alignX;
    int32 alignY;
    loaded_bitmap head;
    loaded_bitmap cape;
    loaded_bitmap torso;
};

struct game_state
{
    memory_arena worldArena;
    world *worldMap;

    tile_map_position cameraPos;
    tile_map_position playerPos;
    
    loaded_bitmap bmp;

    uint32 heroFacingDirection;
    hero_bitmaps heroBitmaps[4];
};
