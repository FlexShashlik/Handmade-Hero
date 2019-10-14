inline void
RecanonicalizeCoord
(
    tile_map *tileMap, ui32 *tile, r32 *tileRel
)
{
    // NOTE: World is assumed to be toroidal topology
    i32 offset = RoundR32ToI32
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
            tileMap, &result.absTileX, &result.offset.x
        );
    RecanonicalizeCoord
        (
            tileMap, &result.absTileY, &result.offset.y
        );

    return result;
}

inline tile_chunk *
GetTileChunk
(
    tile_map *tileMap,
    ui32 tileChunkX, ui32 tileChunkY, ui32 tileChunkZ
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

inline ui32
GetTileValueUnchecked
(
    tile_map *tileMap,
    tile_chunk *tileChunk,
    ui32 tileX, ui32 tileY
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
    ui32 tileX, ui32 tileY,
    ui32 tileValue
)
{
    Assert(tileChunk);
    Assert(tileX < tileMap->chunkDim);
    Assert(tileY < tileMap->chunkDim);
    
    tileChunk->tiles[tileY * tileMap->chunkDim + tileX] = tileValue;
}

inline ui32
GetTileValue
(
    tile_map *tileMap,
    tile_chunk *tileChunk,
    ui32 testTileX, ui32 testTileY
)
{
    ui32 tileChunkValue = 0;

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
    ui32 testTileX, ui32 testTileY,
    ui32 tileValue
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
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
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

internal ui32
GetTileValue
(
    tile_map *tileMap,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
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

    ui32 tileChunkValue = GetTileValue
        (
            tileMap, tileChunk,
            chunkPos.relTileX, chunkPos.relTileY
        );

    return tileChunkValue;
}

internal ui32
GetTileValue
(
    tile_map *tileMap,
    tile_map_position pos
)
{
    ui32 tileChunkValue = GetTileValue
        (
            tileMap,
            pos.absTileX, pos.absTileY, pos.absTileZ
        );

    return tileChunkValue;
}

internal b32
IsTileValueEmpty(ui32 tileValue)
{
    b32 isEmpty = ((tileValue == 1) ||
                   (tileValue == 3) ||
                   (tileValue == 4));
    
    return isEmpty;
}

internal b32
IsTileMapPointEmpty(tile_map *tileMap, tile_map_position pos)
{
    ui32 tileChunkValue = GetTileValue(tileMap, pos);
    b32 isEmpty = IsTileValueEmpty(tileChunkValue);

    return isEmpty;
}

internal void
SetTileValue
(
    memory_arena *arena, tile_map *tileMap,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ,
    ui32 tileValue
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
        ui32 tileCount = tileMap->chunkDim * tileMap->chunkDim;
        tileChunk->tiles = PushArray
            (
                arena,
                tileCount,
                ui32
            );

        for(ui32 tileIndex = 0;
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

inline b32
AreOnSameTile(tile_map_position *a, tile_map_position *b)
{
    b32 result = (a->absTileX == b->absTileX &&
                  a->absTileY == b->absTileY &&
                  a->absTileZ == b->absTileZ);
    return result;
}

inline tile_map_difference
Subtract
(
    tile_map *tileMap,
    tile_map_position *a, tile_map_position *b
)
{
    tile_map_difference result;

    v2 dTileXY = {(r32)a->absTileX - (r32)b->absTileX,
                  (r32)a->absTileY - (r32)b->absTileY};
    
    r32 dTileZ = (r32)a->absTileZ - (r32)b->absTileZ;
    
    result.dXY = tileMap->tileSideInMeters * dTileXY + a->offset - b->offset;
    result.dZ = tileMap->tileSideInMeters * dTileZ;
 
    return result;
}

inline tile_map_position
CenteredTilePoint(ui32 absTileX, ui32 absTileY, ui32 absTileZ)
{
    tile_map_position result = {};

    result.absTileX = absTileX;
    result.absTileY = absTileY;
    result.absTileZ = absTileZ;

    return result;
}
