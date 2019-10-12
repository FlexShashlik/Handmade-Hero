struct tile_map_difference
{
    v2 dXY;
    real32 dZ;
};

struct tile_map_position
{
    // NOTE: These are fixed point tile locations. The high bits are
    // the tile chunk index, and the low bits are the tile index in
    // the chunk
    uint32 absTileX;
    uint32 absTileY;
    uint32 absTileZ;

    // NOTE: This are the offsets from the tile center
    v2 offset;
};

struct tile_chunk_position
{
    uint32 tileChunkX;
    uint32 tileChunkY;
    uint32 tileChunkZ;

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
    
    uint32 tileChunkCountX;
    uint32 tileChunkCountY;
    uint32 tileChunkCountZ;
    
    tile_chunk *tileChunks;
};
