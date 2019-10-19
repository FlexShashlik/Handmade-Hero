#include "handmade_platform.h"

#define Minimum(a, b) ((a < b) ? (a) : (b))
#define Maximum(a, b) ((a > b) ? (a) : (b))

struct memory_arena
{
    memory_index size;
    ui8 *base;
    memory_index used;
};

internal void
InitializeArena
(
    memory_arena *arena,
    memory_index size,
    ui8 *base
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
    ui32 *pixels;
};

struct hero_bitmaps
{
    i32 alignX;
    i32 alignY;
    loaded_bitmap head;
    loaded_bitmap cape;
    loaded_bitmap torso;
};

struct high_entity
{
    v2 pos; // NOTE: Relative to the camera
    ui32 absTileZ;
    v2 dPos;
    ui32 facingDirection;

    r32 z;
    r32 dZ;
};

struct low_entity
{
};

enum entity_type
{
    EntityType_Null,
    EntityType_Hero,
    EntityType_Wall
};

struct dormant_entity
{
    entity_type type;
    
    tile_map_position pos;
    r32 width;
    r32 height;

    // NOTE: This is for "stairs"
    b32 isCollides;
    i32 deltaAbsTileZ;
};

struct entity
{
    ui32 residence;
    dormant_entity *dormant;
    low_entity *low;
    high_entity *high;
};

enum entity_residence
{
    EntityResidence_Nonexistent,
    EntityResidence_Dormant,
    EntityResidence_Low,
    EntityResidence_High,
};

struct game_state
{
    memory_arena worldArena;
    world *worldMap;

    ui32 cameraFollowingEntityIndex;
    tile_map_position cameraPos;

    ui32 playerIndexForController[ArrayCount(((game_input *)0)->controllers)];

    ui32 entityCount;
    entity_residence entityResidence[256];
    high_entity highEntities[256];
    low_entity lowEntities[256];
    dormant_entity dormantEntities[256];
    
    loaded_bitmap bmp;
    loaded_bitmap shadow;

    hero_bitmaps heroBitmaps[4];
};
