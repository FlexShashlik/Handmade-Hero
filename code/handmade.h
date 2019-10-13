#include "handmade_platform.h"

struct memory_arena
{
    memory_index size;
    u8 *base;
    memory_index used;
};

internal void
InitializeArena
(
    memory_arena *arena,
    memory_index size,
    u8 *base
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

#include "handmade_math.h"
#include "handmade_intrinsics.h"
#include "handmade_tile.h"

struct world
{
    tile_map *tileMap;
};

struct loaded_bitmap
{
    i32 width;
    i32 height;
    u32 *pixels;
};

struct hero_bitmaps
{
    i32 alignX;
    i32 alignY;
    loaded_bitmap head;
    loaded_bitmap cape;
    loaded_bitmap torso;
};

struct entity
{
    b32 isExists;
    tile_map_position pos;
    v2 dPos;
    u32 facingDirection;
    r32 width;
    r32 height;
};

struct game_state
{
    memory_arena worldArena;
    world *worldMap;

    u32 cameraFollowingEntityIndex;
    tile_map_position cameraPos;

    u32 playerIndexForController[ArrayCount(((game_input *)0)->controllers)];

    u32 entityCount;
    entity entities[256];
    
    loaded_bitmap bmp;

    hero_bitmaps heroBitmaps[4];
};
