struct tile_map_position
{
    // NOTE: These are fixed point tile locations. The high bits are
    // the tile chunk index, and the low bits are the tile index in
    // the chunk
    uint32 absTileX;
    uint32 absTileY;

    real32 tileRelX;
    real32 tileRelY;
};

struct tile_chunk_position
{
    uint32 tileChunkX;
    uint32 tileChunkY;

    uint32 relTileX;
    uint32 relTileY;
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
    int32 tileSideInPixels;
    real32 metersToPixels;
    
    uint32 tileChunkCountX;
    uint32 tileChunkCountY;
    
    tile_chunk *tileChunks;
};
