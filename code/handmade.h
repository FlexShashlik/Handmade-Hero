#include "handmade_platform.h"

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
