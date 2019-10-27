internal sim_entity *
AddEntity(sim_region *simRegion)
{
    sim_entity *entity = 0;
    if(simRegion->entityCount < simRegion->maxEntityCount)
    {
        entity = simRegion->entities + simRegion->entityCount++;
        entity = {};
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
    sim_region *simRegion,
    low_entity *source, v2 *simPos
)
{
    sim_entity *dest = AddEntity(simRegion);
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
                            AddEntity(simRegion, low, &simSpacePos);
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
    sim_entity *_entity = region->entities;
    for(ui32 entityIndex = 0;
        entityIndex < region->entityCount;
        entityIndex++, _entity++)
    {
        low_entity *stored = gameState->lowEntities + _entity->storageIndex;

        world_position newPos = MapIntoChunkSpace
            (
                gameState->_world,
                region->origin, _entity->pos
            );
        ChangeEntityLocation
        (
            &gameState->worldArena,
            gameState->_world,
            _entity->storageIndex, stored,
            &stored->pos, &newPos
        );

        entity cameraFollowingEntity = ForceEntityIntoHigh
        (
            gameState,
            gameState->cameraFollowingEntityIndex
        );
    
        if(cameraFollowingEntity.high)
        {
            world_position newCameraPos = gameState->cameraPos;
            gameState->cameraPos.chunkZ = cameraFollowingEntity.low->pos.chunkZ;

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
            newCameraPos = cameraFollowingEntity.low->pos;
#endif

            SetCamera(gameState, newCameraPos);
        }
    }
}
