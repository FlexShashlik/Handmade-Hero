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
    i32 absTileX;
    i32 absTileY;
    i32 absTileZ;

    // NOTE: These are the offsets from the tile center
    v2 _offset;
};

struct tile_chunk_position
{
    i32 tileChunkX;
    i32 tileChunkY;
    i32 tileChunkZ;

    i32 relTileX;
    i32 relTileY;
};

struct tile_chunk
{
    i32 tileChunkX;
    i32 tileChunkY;
    i32 tileChunkZ;
    
    ui32 *tiles;

    tile_chunk *nextInHash;
};

struct tile_map
{
    i32 chunkShift;
    i32 chunkMask;
    i32 chunkDim;
    
    r32 tileSideInMeters;
    
    tile_chunk tileChunkHash[4096];
};
