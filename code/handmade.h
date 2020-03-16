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

inline memory_index
GetAlignmentOffset(memory_arena *arena, memory_index alignment)
{
    memory_index alignmentOffset = 0;
    memory_index resultPointer = (memory_index)arena->base + arena->used;
    memory_index alignmentMask = alignment - 1;
    if(resultPointer & alignmentMask)
    {
        alignmentOffset = alignment - (resultPointer & alignmentMask); 
    }

    return alignmentOffset;
}

inline memory_index
GetArenaSizeRemaining(memory_arena *arena, memory_index alignment = 4)
{
    memory_index result = arena->size - (arena->used + GetAlignmentOffset(arena, alignment));

    return result;
}

// TODO: Optional "clear" parameter
#define PushStruct(arena, type, ...) (type *)PushSize_(arena, sizeof(type), ## __VA_ARGS__)
#define PushArray(arena, count, type, ...) (type *)PushSize_(arena, (count) * sizeof(type), ## __VA_ARGS__)
#define PushSize(arena, size, ...) PushSize_(arena, size, ## __VA_ARGS__)
inline void *
PushSize_(memory_arena *arena, memory_index sizeInit, memory_index alignment = 4)
{
    memory_index size = sizeInit;
    
    memory_index alignmentOffset = GetAlignmentOffset(arena, alignment);
    size += alignmentOffset;
    
    Assert(arena->used + size <= arena->size);
    void *result = arena->base + arena->used + alignmentOffset;
    arena->used += size;

    Assert(size >= sizeInit);
    
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

inline void
SubArena(memory_arena *result, memory_arena *arena, memory_index size, memory_index alignment = 16)
{
    result->size = size;
    result->base = (ui8 *)PushSize_(arena, size, alignment);
    result->used = 0;
    result->tempCount = 0;
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
#include "handmade_asset.h"

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

struct hero_bitmap_ids
{
    bitmap_id head;
    bitmap_id cape;
    bitmap_id torso;
};

struct game_state
{
    b32 isInitialized;
    
    memory_arena worldArena;
    world *_world;

    r32 typicalFloorHeight;
    
    ui32 cameraFollowingEntityIndex;
    world_position cameraPos;

    controlled_hero controlledHeroes[ArrayCount(((game_input *)0)->controllers)];

    ui32 lowEntityCount;
    low_entity lowEntities[100000];
    
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

    loaded_bitmap testDiffuse; // TODO: Re-fill this with gray
    loaded_bitmap testNormal;

    loaded_sound testSound;
    r32 tSine;
    ui32 testSampleIndex;
};

struct task_with_memory
{
    b32 beingUsed;
    memory_arena arena;

    temporary_memory memoryFlush;
};

struct transient_state
{
    b32 isInitialized;
    memory_arena tranArena;

    task_with_memory tasks[4];

    game_assets *assets;
    
    ui32 groundBufferCount;
    ground_buffer *groundBuffers;

    platform_work_queue *lowPriorityQueue;
    platform_work_queue *highPriorityQueue;

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

global_variable platform_add_entry *PlatformAddEntry;
global_variable platform_complete_all_work *PlatformCompleteAllWork;
global_variable debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;

internal task_with_memory *BeginTaskWithMemory(transient_state *tranState);
internal void EndTaskWithMemory(task_with_memory *task);
