struct tile_map_position
{
    // NOTE: These are fixed point tile locations. The high bits are
    // the tile chunk index, and the low bits are the tile index in
    // the chunk
    uint32 absTileX;
    uint32 absTileY;
    uint32 absTileZ;

    real32 tileRelX;
    real32 tileRelY;
};

struct tile_chunk_position
{
    uint32 tileChunkX;
    uint32 tileChunkY;
    uint32 tileChunkZ;

    // NOTE: This are the offsets from the tile center
    uint32 offsetX;
    uint32 offsetY;
};

struct tile_chunk
{
    uint32 *tiles;
};

struct tile_map
{
    uint32 chunkShift;
    uint32 chunkMask;
    uint32 chunkDim;
    
    real32 tileSideInMeters;
    
    uint32 tileChunkCountX;
    uint32 tileChunkCountY;
    uint32 tileChunkCountZ;
    
    tile_chunk *tileChunks;
};
