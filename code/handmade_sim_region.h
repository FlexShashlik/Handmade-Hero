struct sim_entity
{
    ui32 storageIndex;
    
    v2 pos;
    ui32 chunkZ;
    
    r32 z;
    r32 dZ;
};

struct sim_region
{
    world *_world;
    
    world_position origin;
    rectangle2 bounds;
    
    ui32 maxEntityCount;
    ui32 entityCount;
    sim_entity *entities;
};
