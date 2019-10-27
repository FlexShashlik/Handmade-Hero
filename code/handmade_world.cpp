#define WORLD_CHUNK_SAFE_MARGIN (INT32_MAX / 64)
#define WORLD_CHUNK_UNINITIALIZED INT32_MAX

#define TILES_PER_CHUNK 16

inline world_position
NullPosition(void)
{
    world_position result = {};
    result.chunkX = WORLD_CHUNK_UNINITIALIZED;

    return result;
}

inline b32
IsValid(world_position pos)
{
    b32 result = (pos.chunkX != WORLD_CHUNK_UNINITIALIZED);
    return result;
}

inline b32
IsCanonical(world *_world, r32 tileRel)
{
    b32 result = (tileRel >= -0.5f * _world->chunkSideInMeters &&
                  tileRel <= 0.5f * _world->chunkSideInMeters);

    return result;
}

inline b32
IsCanonical(world *_world, v2 offset)
{
    b32 result = (IsCanonical(_world, offset.x) &&
                  IsCanonical(_world, offset.y));

    return result;
}

inline b32
AreInSameChunk(world *_world, world_position *a, world_position *b)
{
    Assert(IsCanonical(_world, a->_offset));
    Assert(IsCanonical(_world, b->_offset));
           
    b32 result = (a->chunkX == b->chunkX &&
                  a->chunkY == b->chunkY &&
                  a->chunkZ == b->chunkZ);
    
    return result;
}

inline void
RecanonicalizeCoord
(
    world *_world, i32 *tile, r32 *tileRel
)
{
    i32 offset = RoundR32ToI32
        (
            *tileRel / _world->chunkSideInMeters
        );
    *tile += offset;
    
    *tileRel -= offset * _world->chunkSideInMeters;

    Assert(IsCanonical(_world, *tileRel));
}

inline world_position
MapIntoChunkSpace
(
    world *_world,
    world_position basePos, v2 offset
)
{
    world_position result = basePos;
    result._offset += offset;

    RecanonicalizeCoord
        (
            _world, &result.chunkX, &result._offset.x
        );
    RecanonicalizeCoord
        (
            _world, &result.chunkY, &result._offset.y
        );

    return result;
}

inline world_position
ChunkPosFromTilePos
(
    world *_world,
    i32 absTileX, i32 absTileY, i32 absTileZ
)
{
    world_position result = {};
    
    result.chunkX = absTileX / TILES_PER_CHUNK;
    result.chunkY = absTileY / TILES_PER_CHUNK;
    result.chunkZ = absTileZ / TILES_PER_CHUNK;

    if(absTileX < 0)
    {
        result.chunkX--;
    }
    if(absTileY < 0)
    {
        result.chunkY--;
    }
    if(absTileZ < 0)
    {
        result.chunkZ--;
    }
    
    result._offset.x = (r32)((absTileX - TILES_PER_CHUNK / 2) - (result.chunkX * TILES_PER_CHUNK)) * _world->tileSideInMeters;
    result._offset.y = (r32)((absTileY - TILES_PER_CHUNK / 2) - (result.chunkY * TILES_PER_CHUNK)) * _world->tileSideInMeters;

    Assert(IsCanonical(_world, result._offset));
    
    return result;
}

inline world_chunk *
GetWorldChunk
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

internal void
InitializeWorld(world *_world, r32 tileSideInMeters)
{
    _world->tileSideInMeters = tileSideInMeters;
    _world->chunkSideInMeters = (r32)TILES_PER_CHUNK * tileSideInMeters;
    _world->firstFree = 0;

    for(ui32 chunkIndex = 0;
        chunkIndex < ArrayCount(_world->chunkHash);
        chunkIndex++)
    {
        _world->chunkHash[chunkIndex].chunkX = WORLD_CHUNK_UNINITIALIZED;
        _world->chunkHash[chunkIndex].firstBlock.entityCount = 0;
    }
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
            (r32)a->chunkX - (r32)b->chunkX,
            (r32)a->chunkY - (r32)b->chunkY
        };
    
    r32 dTileZ = (r32)a->chunkZ - (r32)b->chunkZ;
    
    result.dXY = _world->chunkSideInMeters * dTileXY +
        a->_offset - b->_offset;
    result.dZ = _world->chunkSideInMeters * dTileZ;
 
    return result;
}

inline world_position
CenteredChunkPoint(ui32 chunkX, ui32 chunkY, ui32 chunkZ)
{
    world_position result = {};

    result.chunkX = chunkX;
    result.chunkY = chunkY;
    result.chunkZ = chunkZ;

    return result;
}

inline void
ChangeEntityLocationRaw
(
    memory_arena *arena, world *_world,
    ui32 lowEntityIndex,
    world_position *oldPos, world_position *newPos
)
{
    Assert(!oldPos || IsValid(*oldPos));
    Assert(!newPos || IsValid(*newPos));
    
    if(!oldPos || !newPos || !AreInSameChunk(_world, oldPos, newPos))
    {
        if(oldPos)
        {
            // NOTE: Pull the entity out of its old entity_block
            world_chunk *chunk = GetWorldChunk
            (
                _world,
                oldPos->chunkX, oldPos->chunkY, oldPos->chunkZ
            );
            Assert(chunk);
            
            if(chunk)
            {
                b32 isFound = false;
                world_entity_block *firstBlock = &chunk->firstBlock;
                for(world_entity_block *block = firstBlock;
                    block && !isFound;
                    block = block->next)
                {
                    for(ui32 index = 0;
                        index < block->entityCount && !isFound;
                        index++)
                    {
                        if(block->lowEntityIndex[index] == lowEntityIndex)
                        {
                            Assert(firstBlock->entityCount > 0);
                            block->lowEntityIndex[index] =
                                block->lowEntityIndex[--firstBlock->entityCount];

                            if(firstBlock->entityCount == 0)
                            {
                                if(firstBlock->next)
                                {
                                    world_entity_block *nextBlock = firstBlock->next;
                                    *firstBlock = *nextBlock;

                                    nextBlock->next = _world->firstFree;
                                    _world->firstFree = nextBlock;
                                }
                            }

                            isFound = true;
                        }
                    }
                }
            }
        }

        if(newPos)
        {
            // NOTE: Insert the entity into its new entity_block 
            world_chunk *chunk = GetWorldChunk
                (
                    _world,
                    newPos->chunkX, newPos->chunkY, newPos->chunkZ,
                    arena
                );
            Assert(chunk);
        
            world_entity_block *block = &chunk->firstBlock;
            if(block->entityCount == ArrayCount(block->lowEntityIndex))
            {
                // NOTE: We're out of room, get a new block
                world_entity_block *oldBlock = _world->firstFree;
                if(oldBlock)
                {
                    _world->firstFree = oldBlock->next;
                }
                else
                {
                    oldBlock = PushStruct(arena, world_entity_block);
                }
            
                *oldBlock = *block;
                block->next = oldBlock;
                block->entityCount = 0;
            }

            Assert(block->entityCount < ArrayCount(block->lowEntityIndex));
            block->lowEntityIndex[block->entityCount++] = lowEntityIndex;
        }
    }
}

internal void
ChangeEntityLocation
(
    memory_arena *arena, world *_world,
    ui32 lowEntityIndex, low_entity *lowEntity,
    world_position *oldPos, world_position *newPos
)
{
    ChangeEntityLocationRaw
        (
            arena, _world,
            lowEntityIndex,
            oldPos, newPos
        );

    if(newPos)
    {
        lowEntity->pos = *newPos;
    }
    else
    {
        lowEntity->pos = NullPosition();
    }
}
