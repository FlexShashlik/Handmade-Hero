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
    ui32 absTileX;
    ui32 absTileY;
    ui32 absTileZ;

    // NOTE: These are the offsets from the tile center
    v2 _offset;
};

struct tile_chunk_position
{
    ui32 tileChunkX;
    ui32 tileChunkY;
    ui32 tileChunkZ;

    ui32 relTileX;
    ui32 relTileY;
};

struct tile_chunk
{
    ui32 *tiles;
};

struct tile_map
{
    ui32 chunkShift;
    ui32 chunkMask;
    ui32 chunkDim;
    
    r32 tileSideInMeters;
    
    ui32 tileChunkCountX;
    ui32 tileChunkCountY;
    ui32 tileChunkCountZ;
    
    tile_chunk *tileChunks;
};
