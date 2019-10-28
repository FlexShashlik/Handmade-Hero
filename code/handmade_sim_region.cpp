internal sim_entity_hash *
GetHashFromStorageIndex(sim_region *simRegion, ui32 storageIndex)
{
    Assert(storageIndex);
    sim_entity_hash *result = 0;
    
    ui32 hashValue = storageIndex;
    for(ui32 offset = 0;
        offset < ArrayCount(simRegion->hash);
        offset++)
    {
        sim_entity_hash *entry = simRegion->hash +
            ((hashValue + offset) & (ArrayCount(simRegion->hash) - 1));

        if(entry->index == 0 || entry->index == storageIndex)
        {
            result = entry;
            break;
        }
    }

    return result;
}

internal void
MapStorageIndexToEntity
(
    sim_region *simRegion, ui32 storageIndex, sim_entity *entity
)
{
    sim_entity_hash *entry = GetHashFromStorageIndex
        (
            simRegion, storageIndex
        );
    Assert(entry->index == 0 || entry->index == storageIndex);
    entry->index = storageIndex;
    entry->ptr = entity;
}

inline sim_entity *
GetEntityByStorageIndex
(
    sim_region *simRegion, ui32 storageIndex, low_entity *source
)
{
    sim_entity_hash *entry = GetHashFromStorageIndex
        (
            simRegion, storageIndex
        );
    sim_entity *result = entry->ptr;
    return result;
}

internal sim_entity *
AddEntity
(
    game_state *gameState,
    sim_region *simRegion, ui32 storageIndex, low_entity *source
);
inline void
LoadEntityReference
(
    game_state *gameState,
    sim_region *simRegion, entity_reference *ref
)
{
    if(ref->index)
    {
        sim_entity_hash *entry = GetHashFromStorageIndex
            (
                simRegion, ref->index
            );
        if(entry->ptr == 0)
        {
            entry->ref = ref->index;
            entry->ptr = AddEntity
                (
                    gameState,
                    simRegion, ref->index,
                    GetLowEntity(gameState, ref->index)
                );
        }
    
        ref->ptr = entry->ptr;
    }
}

inline void
StoreEntityReference(entity_reference *ref)
{
    if(ref->ptr != 0)
    {
        ref->index = ref->ptr->storageIndex;
    }
}

internal sim_entity *
AddEntity
(
    game_state *gameState,
    sim_region *simRegion, ui32 storageIndex, low_entity *source
)
{
    Assert(storageIndex);
    sim_entity *entity = 0;
    if(simRegion->entityCount < simRegion->maxEntityCount)
    {
        entity = simRegion->entities + simRegion->entityCount++;
        MapStorageIndexToEntity(simRegion, storageIndex, entity);
        
        if(source)
        {
            *entity = source->sim;
            LoadEntityReference
                (
                    gameState, simRegion, &entity->sword
                );
        }
        
        entity->storageIndex = storageIndex;
    }
    else
    {
        InvalidCodePath;
    }

    return entity;
}

inline v2
GetSimSpacePos(sim_region *simRegion, low_entity *stored)
{
    world_difference diff = Subtract
        (
            simRegion->_world,
            &stored->pos,
            &simRegion->origin
        );
    v2 result = diff.dXY;

    return result;
}

internal sim_entity *
AddEntity
(
    game_state *gameState,
    sim_region *simRegion, ui32 storageIndex,
    low_entity *source, v2 *simPos
)
{
    sim_entity *dest = AddEntity
        (
            gameState,
            simRegion, storageIndex, source
        );
    if(dest)
    {
        if(simPos)
        {
            dest->pos = *simPos;
        }
        else
        {
            dest->pos = GetSimSpacePos
                (
                    simRegion, source
                );
        }
    }
}

internal sim_region *
BeginSim
(
    memory_arena *simArena, game_state *gameState, world *_world,
    world_position origin, rectangle2 bounds
)
{
    sim_region *simRegion = PushStruct(simArena, sim_region);
    
    simRegion->_world = _world;
    simRegion->origin = origin;
    simRegion->bounds = bounds;

    simRegion->maxEntityCount = 4096;
    simRegion->entityCount = 0;
    simRegion->entities = PushArray
        (
            simArena,
            simRegion->maxEntityCount, sim_entity
        );
    
    world_position minChunkPos = MapIntoChunkSpace
        (
            _world,
            simRegion->origin, GetMinCorner(simRegion->bounds)
        );
    world_position maxChunkPos = MapIntoChunkSpace
        (
            _world,
            simRegion->origin, GetMaxCorner(simRegion->bounds)
        );
    
    for(i32 chunkY = minChunkPos.chunkY;
        chunkY <= maxChunkPos.chunkY;
        chunkY++)
    {
        for(i32 chunkX = minChunkPos.chunkX;
            chunkX <= maxChunkPos.chunkX;
            chunkX++)
        {
            world_chunk *chunk = GetWorldChunk
                (
                    _world, chunkX, chunkY, simRegion->origin.chunkZ
                );

            if(chunk)
            {
                for(world_entity_block *block = &chunk->firstBlock;
                    block;
                    block = block->next)
                {
                    for(ui32 entityIndexIndex = 0;
                        entityIndexIndex < block->entityCount;
                        entityIndexIndex++)
                    {
                        ui32 lowEntityIndex = block->lowEntityIndex[entityIndexIndex];
                        low_entity *low = gameState->lowEntities + lowEntityIndex;
                        
                        v2 simSpacePos = GetSimSpacePos
                            (
                                simRegion, low
                            );
                        if(IsInRectangle(simRegion->bounds,
                                         simSpacePos))
                        {
                            AddEntity
                                (
                                    gameState, simRegion,
                                    lowEntityIndex, low, &simSpacePos
                                );
                        }
                    }
                }
            }
        }
    }
}

internal void
EndSim(sim_region *region, game_state *gameState)
{
    sim_entity *entity = region->entities;
    for(ui32 entityIndex = 0;
        entityIndex < region->entityCount;
        entityIndex++, entity++)
    {
        low_entity *stored = gameState->lowEntities + entity->storageIndex;

        stored->sim = *entity;
        StoreEntityReference(&stored->sim.sword);
        
        world_position newPos = MapIntoChunkSpace
            (
                gameState->_world,
                region->origin, entity->pos
            );
        ChangeEntityLocation
        (
            &gameState->worldArena,
            gameState->_world,
            entity->storageIndex, stored,
            &stored->pos, &newPos
        );
    
        if(entity->storageIndex == gameState->cameraFollowingEntityIndex)
        {
            world_position newCameraPos = gameState->cameraPos;
            gameState->cameraPos.chunkZ = stored->pos.chunkZ;

#if 0
            if(cameraFollowingEntity.high->pos.x >
               9.0f * _world->tileSideInMeters)
            {
                newCameraPos.absTileX += 17;
            }

            if(cameraFollowingEntity.high->pos.x <
               -9.0f * _world->tileSideInMeters)
            {
                newCameraPos.absTileX -= 17;
            }

            if(cameraFollowingEntity.high->pos.y >
               5.0f * _world->tileSideInMeters)
            {
                newCameraPos.absTileY += 9;
            }

            if(cameraFollowingEntity.high->pos.y <
               -5.0f * _world->tileSideInMeters)
            {
                newCameraPos.absTileY -= 9;
            }
#else
            newCameraPos = stored->pos;
#endif
        }
    }
}

internal b32
TestWall
(
    r32 wallX, r32 relX, r32 relY,
    r32 deltaPlayerX, r32 deltaPlayerY,
    r32 *tMin, r32 minY, r32 maxY
)
{
    b32 isHit = false;
    
    r32 tEpsilon = 0.001f;
    if(deltaPlayerX != 0.0f)
    {
        r32 tResult = (wallX - relX) / deltaPlayerX;
        r32 y = relY + tResult * deltaPlayerY;
                
        if(tResult >= 0.0f && *tMin > tResult)
        {
            if(y >= minY && y <= maxY)
            {
                *tMin = Maximum(0.0f, tResult - tEpsilon);
                isHit = true;
            }
        }
    }

    return isHit;
}

internal void
MoveEntity
(
    sim_region *simRegion,
    sim_entity *_entity, r32 deltaTime,
    move_spec *moveSpec, v2 ddPos
)
{
    world *_world = simRegion->_world;

    if(moveSpec->isUnitMaxAccelVector)
    {
        r32 ddPLength = LengthSq(ddPos);
        if(ddPLength > 1.0f)
        {
            ddPos *= 1.0f / SqRt(ddPLength);
        }
    }
    
    ddPos *= moveSpec->speed;

    // TODO: ODE!!!
    ddPos += -moveSpec->drag * _entity->dPos;

    v2 oldPlayerPos = _entity->pos;
    v2 deltaPlayerPos = 0.5f * ddPos *
        Square(deltaTime) + _entity->dPos * deltaTime;
    _entity->dPos = ddPos * deltaTime +
        _entity->dPos;
    
    v2 newPlayerPos = oldPlayerPos + deltaPlayerPos;
    
    for(ui32 i = 0; i < 4; i++)
    {
        r32 tMin = 1.0f;
        v2 wallNormal = {};
        sim_entity *hitEntity = 0;

        v2 desiredPos = _entity->pos + deltaPlayerPos;

        if(_entity->isCollides)
        {
            // TODO: Spatial partition here
            for(ui32 highEntityIndex = 0;
                highEntityIndex < simRegion->entityCount;
                highEntityIndex++)
            {
                sim_entity *testEntity = simRegion->entities + highEntityIndex;
                if(_entity != testEntity)
                {
                    if(testEntity->isCollides)
                    {
                        r32 diameterW = testEntity->width + _entity->width;
                        r32 diameterH = testEntity->height + _entity->height;
                
                        v2 minCorner = -0.5f * v2{diameterW, diameterH};                        
                        v2 maxCorner = 0.5f * v2{diameterW, diameterH};

                        v2 rel = _entity->pos - testEntity->pos;
                
                        if(TestWall(minCorner.x, rel.x, rel.y,
                                    deltaPlayerPos.x, deltaPlayerPos.y,
                                    &tMin, minCorner.y, maxCorner.y))
                        {
                            wallNormal = v2{-1, 0};
                            hitEntity = testEntity;
                        }                    
                
                        if(TestWall(maxCorner.x, rel.x, rel.y,
                                    deltaPlayerPos.x, deltaPlayerPos.y,
                                    &tMin, minCorner.y, maxCorner.y))
                        {
                            wallNormal = v2{1, 0};
                            hitEntity = testEntity;
                        }
                
                        if(TestWall(minCorner.y, rel.y, rel.x,
                                    deltaPlayerPos.y, deltaPlayerPos.x,
                                    &tMin, minCorner.x, maxCorner.x))
                        {
                            wallNormal = v2{0, -1};
                            hitEntity = testEntity;
                        }
                
                        if(TestWall(maxCorner.y, rel.y, rel.x,
                                    deltaPlayerPos.y, deltaPlayerPos.x,
                                    &tMin, minCorner.x, maxCorner.x))
                        {
                            wallNormal = v2{0, 1};
                            hitEntity = testEntity;
                        }
                    }
                }
            }
        }
        
        _entity->pos += tMin * deltaPlayerPos;
        if(hitEntity)
        {
            _entity->dPos = _entity->dPos - 1 *
                Inner(_entity->dPos, wallNormal) * wallNormal;

            deltaPlayerPos = desiredPos - _entity->pos;
            deltaPlayerPos = deltaPlayerPos - 1 *
                Inner(deltaPlayerPos, wallNormal) * wallNormal;

            // TODO: Stairs
        }
        else
        {
            break;
        }
    }

    if(_entity->dPos.x == 0.0f && _entity->dPos.y == 0.0f)
    {
        // NOTE: Leave facing direction whatever it was
    }
    else if(AbsoluteValue(_entity->dPos.x) >
            AbsoluteValue(_entity->dPos.y))
    {
        if(_entity->dPos.x > 0)
        {
            _entity->facingDirection = 0;
        }
        else
        {
            _entity->facingDirection = 2;
        }
    }
    else if(AbsoluteValue(_entity->dPos.x) <
            AbsoluteValue(_entity->dPos.y))
    {
        if(_entity->dPos.y > 0)
        {
            _entity->facingDirection = 1;
        }
        else
        {
            _entity->facingDirection = 3;
        }
    }
}
