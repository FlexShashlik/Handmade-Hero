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
#include "handmade_world.h"

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

enum entity_type
{
    EntityType_Null,
    EntityType_Hero,
    EntityType_Wall
};

struct high_entity
{
    v2 pos; // NOTE: Relative to the camera
    ui32 chunkZ;
    v2 dPos;
    ui32 facingDirection;

    r32 z;
    r32 dZ;

    ui32 lowEntityIndex;
};

struct low_entity
{
    entity_type type;
    
    world_position pos;
    r32 width;
    r32 height;

    // NOTE: This is for "stairs"
    b32 isCollides;
    i32 deltaAbsTileZ;

    ui32 highEntityIndex;
};

struct entity
{
    ui32 lowIndex;
    low_entity *low;
    high_entity *high;
};

struct game_state
{
    memory_arena worldArena;
    world *worldMap;

    ui32 cameraFollowingEntityIndex;
    world_position cameraPos;

    ui32 playerIndexForController[ArrayCount(((game_input *)0)->controllers)];

    ui32 lowEntityCount;
    low_entity lowEntities[100000];

    ui32 highEntityCount;
    high_entity _highEntities[256];
    
    loaded_bitmap bmp;
    loaded_bitmap shadow;
    loaded_bitmap tree;

    hero_bitmaps heroBitmaps[4];
};
