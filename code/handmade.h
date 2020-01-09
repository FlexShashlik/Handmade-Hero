#include "handmade_platform.h"

#define Minimum(a, b) ((a < b) ? (a) : (b))
#define Maximum(a, b) ((a > b) ? (a) : (b))

struct memory_arena
{
    memory_index size;
    ui8 *base;
    memory_index used;

    ui32 tempCount;
};

struct temporary_memory
{
    memory_arena *arena;
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
    arena->tempCount = 0;
}

#define PushStruct(arena, type) (type *)PushSize_(arena, sizeof(type))
#define PushArray(arena, count, type) (type *)PushSize_(arena, (count) * sizeof(type))
#define PushSize(arena, size) PushSize_(arena, size)
inline void *
PushSize_(memory_arena *arena, memory_index size)
{
    Assert(arena->used + size <= arena->size)
    
    void *result = arena->base + arena->used;
    arena->used += size;

    return result;
}

inline temporary_memory
BeginTemporaryMemory(memory_arena *arena)
{
    temporary_memory result;

    result.arena = arena;
    result.used = arena->used;

    arena->tempCount++;

    return result;
}

inline void
EndTemporaryMemory(temporary_memory tempMem)
{
    memory_arena *arena = tempMem.arena;
    Assert(arena->used >= tempMem.used);
    arena->used = tempMem.used;
    Assert(arena->tempCount > 0);
    arena->tempCount--;
}

inline void
CheckArena(memory_arena *arena)
{
    Assert(arena->tempCount == 0);
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
#include "handmade_render_group.h"

struct hero_bitmaps
{
    loaded_bitmap head;
    loaded_bitmap cape;
    loaded_bitmap torso;
};

struct low_entity
{
    world_position pos;
    sim_entity sim;
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

struct ground_buffer
{
    // NOTE: If pos is invalid then ground_buffer has not been filled
    world_position pos; // NOTE: This is the center of the bmp
    loaded_bitmap bitmap;
};

struct game_state
{
    memory_arena worldArena;
    world *_world;

    r32 typicalFloorHeight;
    
    ui32 cameraFollowingEntityIndex;
    world_position cameraPos;

    controlled_hero controlledHeroes[ArrayCount(((game_input *)0)->controllers)];

    ui32 lowEntityCount;
    low_entity lowEntities[100000];

    loaded_bitmap grass[2];
    loaded_bitmap stone[4];
    loaded_bitmap tuft[3];
    
    loaded_bitmap bmp;
    loaded_bitmap shadow;
    loaded_bitmap tree;
    loaded_bitmap sword;
    loaded_bitmap stairwell;

    hero_bitmaps heroBitmaps[4];
    
    r32 metersToPixels;
    r32 pixelsToMeters;
    
    // NOTE: Must be power of two
    pairwise_collision_rule *collisionRuleHash[256];
    pairwise_collision_rule *firstFreeCollisionRule;

    sim_entity_collision_volume_group *nullCollision;
    sim_entity_collision_volume_group *swordCollision;
    sim_entity_collision_volume_group *stairCollision;
    sim_entity_collision_volume_group *playerCollision;
    sim_entity_collision_volume_group *monsterCollision;
    sim_entity_collision_volume_group *familiarCollision;
    sim_entity_collision_volume_group *wallCollision;
    sim_entity_collision_volume_group *standardRoomCollision;

    r32 time;

    loaded_bitmap testDiffuse;
    loaded_bitmap testNormal;
};

struct transient_state
{
    b32 isInitialized;
    memory_arena tranArena;
    ui32 groundBufferCount;
    ground_buffer *groundBuffers;

    ui32 envMapWidth;
    ui32 envMapHeight;
    // NOTE: 0 is bottom, 1 is middle, 2 is top
    environment_map envMaps[3];
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
