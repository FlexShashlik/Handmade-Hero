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
    EntityType_Sword,
    EntityType_Stairwell
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
    EntityFlag_Collides = (1 << 0),
    EntityFlag_Nonspatial = (1 << 1),
    EntityFlag_Simming = (1 << 30)
};

struct sim_entity
{
    ui32 storageIndex;
    b32 isUpdatable;
    
    entity_type type;
    ui32 flags;
    
    v3 pos;
    v3 dPos;

    r32 distanceLimit;
    
    v3 dim;

    ui32 facingDirection;
    r32 tBob;
    
    i32 deltaAbsTileZ;

    ui32 hitPointMax;
    hit_point hitPoint[16];

    entity_reference sword;
};

struct sim_entity_hash
{
    sim_entity *ptr;
    ui32 index;
};

struct sim_region
{
    world *_world;
    r32 maxEntityRadius;
    r32 maxEntityVelocity;
    
    world_position origin;
    rectangle3 bounds;
    rectangle3 updatableBounds;
    
    ui32 maxEntityCount;
    ui32 entityCount;
    sim_entity *entities;

    // NOTE: Must be a power of two
    sim_entity_hash hash[4096];
};
