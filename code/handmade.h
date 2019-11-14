#include "handmade_platform.h"

#define Minimum(a, b) ((a < b) ? (a) : (b))
#define Maximum(a, b) ((a > b) ? (a) : (b))

struct memory_arena
{
    memory_index size;
    ui8 *base;
    memory_index used;
};

inline void
InitializeArena
(
    memory_arena *arena,
    memory_index size,
    void *base
)
{
    arena->size = size;
    arena->base = (ui8 *)base;
    arena->used = 0;
}

#define PushStruct(arena, type) (type *)PushSize_(arena, sizeof(type))
#define PushArray(arena, count, type) (type *)PushSize_(arena, (count) * sizeof(type))
inline void *
PushSize_(memory_arena *arena, memory_index size)
{
    Assert(arena->used + size <= arena->size)
    
    void *result = arena->base + arena->used;
    arena->used += size;

    return result;
}

#define ZeroStruct(instance) ZeroSize(sizeof(instance), &(instance))
inline void
ZeroSize(memory_index size, void *ptr)
{
    ui8 *byte = (ui8 *)ptr;
    while(size--)
    {
        *byte++ = 0;
    }
}

#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_world.h"
#include "handmade_sim_region.h"
#include "handmade_entity.h"

struct loaded_bitmap
{
    i32 width;
    i32 height;
    ui32 *pixels;
};

struct hero_bitmaps
{
    v2 align;
    loaded_bitmap head;
    loaded_bitmap cape;
    loaded_bitmap torso;
};

struct low_entity
{
    world_position pos;
    sim_entity sim;
};

struct entity_visible_piece
{
    loaded_bitmap *bitmap;
    v2 offset;
    r32 offsetZ;
    r32 entityZC;

    r32 r, g, b, a;
    v2 dim;
};

struct controlled_hero
{
    ui32 entityIndex;

    // NOTE: These are the controller requests for simulation
    v2 ddPos;
    v2 dPosSword;
    r32 dZ;
};

struct pairwise_collision_rule
{
    b32 canCollide;
    ui32 storageIndexA;
    ui32 storageIndexB;

    pairwise_collision_rule *nextInHash;
};
struct game_state;
internal void
AddCollisionRule
(
    game_state *gameState,
    ui32 storageIndexA, ui32 storageIndexB, b32 shouldCollide
);
internal void
ClearCollisionRulesFor(game_state *gameState, ui32 storageIndex);

struct game_state
{
    memory_arena worldArena;
    world *_world;

    ui32 cameraFollowingEntityIndex;
    world_position cameraPos;

    controlled_hero controlledHeroes[ArrayCount(((game_input *)0)->controllers)];

    ui32 lowEntityCount;
    low_entity lowEntities[100000];
    
    loaded_bitmap bmp;
    loaded_bitmap shadow;
    loaded_bitmap tree;
    loaded_bitmap sword;
    loaded_bitmap stairwell;

    hero_bitmaps heroBitmaps[4];
    r32 metersToPixels;

    // NOTE: Must be power of two
    pairwise_collision_rule *collisionRuleHash[256];
    pairwise_collision_rule *firstFreeCollisionRule;
};

struct entity_visible_piece_group
{
    game_state *gameState;
    ui32 pieceCount;
    entity_visible_piece pieces[32];
};

inline low_entity *
GetLowEntity(game_state *gameState, ui32 index)
{
    low_entity *result = 0;
    
    if(index > 0 && index < gameState->lowEntityCount)
    {
        result = gameState->lowEntities + index;
    }

    return result;
}
