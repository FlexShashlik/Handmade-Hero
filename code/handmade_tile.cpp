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
GetTileChunk
(
    tile_map *tileMap,
    uint32 tileChunkX, uint32 tileChunkY, uint32 tileChunkZ
)
{
    tile_chunk *tileChunk = 0;
    
    if(tileChunkX >= 0 && tileChunkX < tileMap->tileChunkCountX &&
       tileChunkY >= 0 && tileChunkY < tileMap->tileChunkCountY &&
       tileChunkZ >= 0 && tileChunkZ < tileMap->tileChunkCountZ)
    {
        tileChunk = &tileMap->tileChunks
            [
                tileChunkZ * tileMap->tileChunkCountY * tileMap->tileChunkCountX +
                tileChunkY * tileMap->tileChunkCountX +
                tileChunkX
            ];
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

    if(tileChunk && tileChunk->tiles)
    {
        tileChunkValue = GetTileValueUnchecked
            (
                tileMap,
                tileChunk,
                testTileX, testTileY
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
    if(tileChunk && tileChunk->tiles)
    {
        SetTileValueUnchecked
            (
                tileMap,
                tileChunk,
                testTileX, testTileY,
                tileValue
            );
    }
}

inline tile_chunk_position
GetChunkPosition
(
    tile_map *tileMap,
    uint32 absTileX, uint32 absTileY, uint32 absTileZ
)
{
    tile_chunk_position result;

    result.tileChunkX = absTileX >> tileMap->chunkShift;
    result.tileChunkY = absTileY >> tileMap->chunkShift;
    result.tileChunkZ = absTileZ;
    result.relTileX = absTileX & tileMap->chunkMask;
    result.relTileY = absTileY & tileMap->chunkMask;

    return result;
}

internal uint32
GetTileValue
(
    tile_map *tileMap,
    uint32 absTileX, uint32 absTileY, uint32 absTileZ
)
{
    tile_chunk_position chunkPos = GetChunkPosition
        (tileMap, absTileX, absTileY, absTileZ);
    
    tile_chunk *tileChunk = GetTileChunk
        (
            tileMap,
            chunkPos.tileChunkX,
            chunkPos.tileChunkY,
            chunkPos.tileChunkZ
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
    uint32 tileChunkValue = GetTileValue
        (
            tileMap,
            canPos.absTileX, canPos.absTileY, canPos.absTileZ
        );
    bool32 isEmpty = (tileChunkValue == 1);

    return isEmpty;
}

internal void
SetTileValue
(
    memory_arena *arena, tile_map *tileMap,
    uint32 absTileX, uint32 absTileY, uint32 absTileZ,
    uint32 tileValue
)
{
    tile_chunk_position chunkPos = GetChunkPosition
        (tileMap, absTileX, absTileY, absTileZ);
    
    tile_chunk *tileChunk = GetTileChunk
        (
            tileMap,
            chunkPos.tileChunkX,
            chunkPos.tileChunkY,
            chunkPos.tileChunkZ
        );
    
    Assert(tileChunk);
    if(!tileChunk->tiles)
    {
        uint32 tileCount = tileMap->chunkDim * tileMap->chunkDim;
        tileChunk->tiles = PushArray
            (
                arena,
                tileCount,
                uint32
            );

        for(uint32 tileIndex = 0;
            tileIndex < tileCount;
            tileIndex++)
        {
            tileChunk->tiles[tileIndex] = 1;
        }
    }

    SetTileValue
        (
            tileMap, tileChunk,
            chunkPos.relTileX, chunkPos.relTileY,
            tileValue
        );
}
