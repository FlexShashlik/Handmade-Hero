struct tile_map_difference
{
    v2 dXY;
    r32 dZ;
};

struct tile_map_position
{
    // NOTE: These are fixed point tile locations. The high bits are
    // the tile chunk index, and the low bits are the tile index in
    // the chunk
    u32 absTileX;
    u32 absTileY;
    u32 absTileZ;

    // NOTE: This are the offsets from the tile center
    v2 offset;
};

struct tile_chunk_position
{
    u32 tileChunkX;
    u32 tileChunkY;
    u32 tileChunkZ;

    u32 relTileX;
    u32 relTileY;
};

struct tile_chunk
{
    u32 *tiles;
};

struct tile_map
{
    u32 chunkShift;
    u32 chunkMask;
    u32 chunkDim;
    
    r32 tileSideInMeters;
    
    u32 tileChunkCountX;
    u32 tileChunkCountY;
    u32 tileChunkCountZ;
    
    tile_chunk *tileChunks;
};
