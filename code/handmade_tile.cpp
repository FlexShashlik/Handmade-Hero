inline void
RecanonicalizeCoord
(
    tile_map *tileMap, uint32 *tile, real32 *tileRel
)
{
    // NOTE: World is assumed to be toroidal topology
    int32 offset = RoundReal32ToInt32
        (
            *tileRel / tileMap->tileSideInMeters
        );
    *tile += offset;
    
    *tileRel -= offset * tileMap->tileSideInMeters;
    
    Assert(*tileRel >= -0.5f * tileMap->tileSideInMeters);
    Assert(*tileRel <= 0.5f * tileMap->tileSideInMeters);
}

inline tile_map_position
RecanonicalizePosition(tile_map *tileMap, tile_map_position pos)
{
    tile_map_position result = pos;

    RecanonicalizeCoord
        (
            tileMap, &result.absTileX, &result.tileRelX
        );
    RecanonicalizeCoord
        (
            tileMap, &result.absTileY, &result.tileRelY
        );

    return result;
}

inline tile_chunk *
GetTileChunk(tile_map *tileMap, uint32 tileChunkX, uint32 tileChunkY)
{
    tile_chunk *tileChunk = 0;
    
    if(tileChunkX >= 0 && tileChunkX < tileMap->tileChunkCountX &&
       tileChunkY >= 0 && tileChunkY < tileMap->tileChunkCountY)
    {
        tileChunk = &tileMap->tileChunks[tileChunkY * tileMap->tileChunkCountX + tileChunkX];
    }

    return tileChunk;
}

inline uint32
GetTileValueUnchecked
(
    tile_map *tileMap,
    tile_chunk *tileChunk,
    uint32 tileX, uint32 tileY
)
{
    Assert(tileChunk);
    Assert(tileX < tileMap->chunkDim);
    Assert(tileY < tileMap->chunkDim);
    
    return tileChunk->tiles[tileY * tileMap->chunkDim + tileX];
}

inline void
SetTileValueUnchecked
(
    tile_map *tileMap,
    tile_chunk *tileChunk,
    uint32 tileX, uint32 tileY,
    uint32 tileValue
)
{
    Assert(tileChunk);
    Assert(tileX < tileMap->chunkDim);
    Assert(tileY < tileMap->chunkDim);
    
    tileChunk->tiles[tileY * tileMap->chunkDim + tileX] = tileValue;
}

inline uint32
GetTileValue
(
    tile_map *tileMap,
    tile_chunk *tileChunk,
    uint32 testTileX, uint32 testTileY
)
{
    uint32 tileChunkValue = 0;

    if(tileChunk)
    {
        tileChunkValue = GetTileValueUnchecked
            (
                tileMap,
                tileChunk, testTileX, testTileY
            );
    }

    return tileChunkValue;
}

inline void
SetTileValue
(
    tile_map *tileMap,
    tile_chunk *tileChunk,
    uint32 testTileX, uint32 testTileY,
    uint32 tileValue
)
{
    uint32 tileChunkValue = 0;

    if(tileChunk)
    {
        SetTileValueUnchecked
            (
                tileMap,
                tileChunk, testTileX, testTileY,
                tileValue
            );
    }
}

inline tile_chunk_position
GetChunkPosition(tile_map *tileMap, uint32 absTileX, uint32 absTileY)
{
    tile_chunk_position result;

    result.tileChunkX = absTileX >> tileMap->chunkShift;
    result.tileChunkY = absTileY >> tileMap->chunkShift;
    result.relTileX = absTileX & tileMap->chunkMask;
    result.relTileY = absTileY & tileMap->chunkMask;

    return result;
}

internal uint32
GetTileValue(tile_map *tileMap, uint32 absTileX, uint32 absTileY)
{
    tile_chunk_position chunkPos = GetChunkPosition
        (tileMap, absTileX, absTileY);
    
    tile_chunk *tileChunk = GetTileChunk
        (
            tileMap,
            chunkPos.tileChunkX, chunkPos.tileChunkY
        );

    uint32 tileChunkValue = GetTileValue
        (
            tileMap, tileChunk,
            chunkPos.relTileX, chunkPos.relTileY
        );

    return tileChunkValue;
}

internal bool32
IsTileMapPointEmpty(tile_map *tileMap, tile_map_position canPos)
{
    uint32 tileChunkValue = GetTileValue(tileMap, canPos.absTileX, canPos.absTileY);
    bool32 isEmpty = (tileChunkValue == 0);

    return isEmpty;
}

internal void
SetTileValue
(
    memory_arena *arena, tile_map *tileMap,
    uint32 absTileX, uint32 absTileY,
    uint32 tileValue
)
{
    tile_chunk_position chunkPos = GetChunkPosition
        (tileMap, absTileX, absTileY);
    
    tile_chunk *tileChunk = GetTileChunk
        (
            tileMap,
            chunkPos.tileChunkX, chunkPos.tileChunkY
        );

    // TODO: On-demand tile chunk creation
    Assert(tileChunk);

    SetTileValue
        (
            tileMap, tileChunk,
            chunkPos.relTileX, chunkPos.relTileY,
            tileValue
        );
}
