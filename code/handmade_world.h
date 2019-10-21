struct world_difference
{
    v2 dXY;
    r32 dZ;
};

struct world_position
{
    // NOTE: These are fixed point tile locations. The high bits are
    // the tile chunk index, and the low bits are the tile index in
    // the chunk
    i32 absTileX;
    i32 absTileY;
    i32 absTileZ;

    // NOTE: These are the offsets from the tile center
    v2 _offset;
};

struct world_entity_block
{
    ui32 entityCount;
    ui32 lowIndexEntity[16];
    world_entity_block *next;
};

struct world_chunk
{
    i32 chunkX;
    i32 chunkY;
    i32 chunkZ;
    
    world_entity_block firstBlock;
    
    world_chunk *nextInHash;
};

struct world
{    
    r32 tileSideInMeters;
    
    i32 chunkShift;
    i32 chunkMask;
    i32 chunkDim;
    world_chunk chunkHash[4096];
};
