struct move_spec
{
    b32 isUnitMaxAccelVector;
    r32 speed;
    r32 drag;
};

enum entity_type
{
    EntityType_Null,
    EntityType_Hero,
    EntityType_Wall,
    EntityType_Familiar,
    EntityType_Monster,
    EntityType_Sword
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
    ui8 flags;
    ui8 filledAmount;
};

struct sim_entity;
union entity_reference
{
    sim_entity *ptr;
    ui32 index;
};

enum sim_entity_flags
{
    EntityFlag_Collides = (1 << 1),
    EntityFlag_Nonspatial = (1 << 2),
    EntityFlag_Simming = (1 << 30)
};

struct sim_entity
{
    ui32 storageIndex;

    entity_type type;
    ui32 flags;
    
    v2 pos;
    v2 dPos;
    
    r32 z;
    r32 dZ;

    ui32 chunkZ;
    
    r32 width;
    r32 height;

    ui32 facingDirection;
    r32 tBob;
    
    i32 deltaAbsTileZ;

    ui32 hitPointMax;
    hit_point hitPoint[16];

    entity_reference sword;
    r32 distanceRemaining;
};

struct sim_entity_hash
{
    sim_entity *ptr;
    ui32 index;
};

struct sim_region
{
    world *_world;
    
    world_position origin;
    rectangle2 bounds;
    
    ui32 maxEntityCount;
    ui32 entityCount;
    sim_entity *entities;

    // NOTE: Must be a power of two
    sim_entity_hash hash[4096];
};
