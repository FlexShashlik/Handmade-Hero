inline void
RecanonicalizeCoord
(
    world *_world, i32 *tile, r32 *tileRel
)
{
    // NOTE: World is assumed to be toroidal topology
    i32 offset = RoundR32ToI32
        (
            *tileRel / _world->tileSideInMeters
        );
    *tile += offset;
    
    *tileRel -= offset * _world->tileSideInMeters;
    
    Assert(*tileRel >= -0.5f * _world->tileSideInMeters);
    Assert(*tileRel <= 0.5f * _world->tileSideInMeters);
}

inline world_position
MapIntoTileSpace
(
    world *_world,
    world_position basePos, v2 offset
)
{
    world_position result = basePos;
    result._offset += offset;

    RecanonicalizeCoord
        (
            _world, &result.absTileX, &result._offset.x
        );
    RecanonicalizeCoord
        (
            _world, &result.absTileY, &result._offset.y
        );

    return result;
}

#define WORLD_CHUNK_SAFE_MARGIN (INT32_MAX / 64)
#define WORLD_CHUNK_UNINITIALIZED INT32_MAX
inline world_chunk *
GetTileChunk
(
    world *_world,
    i32 chunkX, i32 chunkY, i32 chunkZ,
    memory_arena *arena = 0
)
{
    Assert(chunkX > -WORLD_CHUNK_SAFE_MARGIN);
    Assert(chunkY > -WORLD_CHUNK_SAFE_MARGIN);
    Assert(chunkZ > -WORLD_CHUNK_SAFE_MARGIN);
    Assert(chunkX < WORLD_CHUNK_SAFE_MARGIN);
    Assert(chunkY < WORLD_CHUNK_SAFE_MARGIN);
    Assert(chunkZ < WORLD_CHUNK_SAFE_MARGIN);
    
    // TODO: Better hash function ;D
    ui32 hashValue = 19 * chunkX + 7 * chunkY + 3 * chunkZ;
    ui32 hashSlot = hashValue & (ArrayCount(_world->chunkHash) - 1);
    Assert(hashSlot < ArrayCount(_world->chunkHash));

    world_chunk *chunk = _world->chunkHash + hashSlot;
    
    do
    {
        if(chunkX == chunk->chunkX &&
           chunkY == chunk->chunkY &&
           chunkZ == chunk->chunkZ)
        {
            break;
        }

        if(arena && chunk->chunkX != WORLD_CHUNK_UNINITIALIZED && !chunk->nextInHash)
        {
            chunk->nextInHash = PushStruct(arena, world_chunk);
            chunk = chunk->nextInHash;
            chunk->chunkX = WORLD_CHUNK_UNINITIALIZED;
        }

        if(arena && chunk->chunkX == WORLD_CHUNK_UNINITIALIZED)
        {
            ui32 tileCount = _world->chunkDim * _world->chunkDim;

            chunk->chunkX = chunkX;
            chunk->chunkY = chunkY;
            chunk->chunkZ = chunkZ;
            
            chunk->nextInHash = 0;
            
            break;
        }

        chunk = chunk->nextInHash;
    } while (chunk);
    
    return chunk;
}

#if 0
inline world_chunk_position
GetChunkPosition
(
    world *_world,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
)
{
    world_chunk_position result;

    result.chunkX = absTileX >> _world->chunkShift;
    result.chunkY = absTileY >> _world->chunkShift;
    result.chunkZ = absTileZ;
    result.relTileX = absTileX & _world->chunkMask;
    result.relTileY = absTileY & _world->chunkMask;

    return result;
}
#endif

internal void
InitializeTileMap(world *_world, r32 tileSideInMeters)
{
    _world->chunkShift = 4;
    _world->chunkMask = (1 << _world->chunkShift) - 1;
    _world->chunkDim = 1 << _world->chunkShift;    
    _world->tileSideInMeters = 1.4f;

    for(ui32 chunkIndex = 0;
        chunkIndex < ArrayCount(_world->chunkHash);
        chunkIndex++)
    {
        _world->chunkHash[chunkIndex].chunkX = WORLD_CHUNK_UNINITIALIZED;
    }
}

inline b32
AreOnSameTile(world_position *a, world_position *b)
{
    b32 result = (a->absTileX == b->absTileX &&
                  a->absTileY == b->absTileY &&
                  a->absTileZ == b->absTileZ);
    return result;
}

inline world_difference
Subtract
(
    world *_world,
    world_position *a, world_position *b
)
{
    world_difference result;

    v2 dTileXY =
        {
            (r32)a->absTileX - (r32)b->absTileX,
            (r32)a->absTileY - (r32)b->absTileY
        };
    
    r32 dTileZ = (r32)a->absTileZ - (r32)b->absTileZ;
    
    result.dXY = _world->tileSideInMeters * dTileXY +
        a->_offset - b->_offset;
    result.dZ = _world->tileSideInMeters * dTileZ;
 
    return result;
}

inline world_position
CenteredTilePoint(ui32 absTileX, ui32 absTileY, ui32 absTileZ)
{
    world_position result = {};

    result.absTileX = absTileX;
    result.absTileY = absTileY;
    result.absTileZ = absTileZ;

    return result;
}
