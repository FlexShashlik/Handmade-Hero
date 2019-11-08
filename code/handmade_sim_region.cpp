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
        ui32 hashMask = (ArrayCount(simRegion->hash) - 1);
        ui32 hashIndex = ((hashValue + offset) & hashMask);
        sim_entity_hash *entry = simRegion->hash + hashIndex;

        if(entry->index == 0 || entry->index == storageIndex)
        {
            result = entry;
            break;
        }
    }

    return result;
}

inline sim_entity *
GetEntityByStorageIndex
(
    sim_region *simRegion, ui32 storageIndex
)
{
    sim_entity_hash *entry = GetHashFromStorageIndex
        (
            simRegion, storageIndex
        );
    sim_entity *result = entry->ptr;
    return result;
}

inline v3
GetSimSpacePos(sim_region *simRegion, low_entity *stored)
{
    v3 result = InvalidPos;
    if(!IsSet(&stored->sim, EntityFlag_Nonspatial))
    {
        result = Subtract
            (
                simRegion->_world,
                &stored->pos,
                &simRegion->origin
            );
    }
    
    return result;
}

internal sim_entity *
AddEntity
(
    game_state *gameState,
    sim_region *simRegion, ui32 storageIndex,
    low_entity *source, v3 *simPos
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
            entry->index = ref->index;
            low_entity *lowEntity = GetLowEntity(gameState, ref->index);
            v3 pos = GetSimSpacePos(simRegion, lowEntity);
            entry->ptr = AddEntity
                (
                    gameState,
                    simRegion, ref->index,
                    lowEntity,
                    &pos
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
AddEntityRaw
(
    game_state *gameState,
    sim_region *simRegion, ui32 storageIndex, low_entity *source
)
{
    Assert(storageIndex);
    sim_entity *_entity = 0;

    sim_entity_hash *entry = GetHashFromStorageIndex
            (
                simRegion, storageIndex
            );
    if(entry->ptr == 0)
    {
        if(simRegion->entityCount < simRegion->maxEntityCount)
        {
            _entity = simRegion->entities + simRegion->entityCount++;
            entry->index = storageIndex;
            entry->ptr = _entity;
            
            if(source)
            {
                *_entity = source->sim;
                LoadEntityReference
                    (
                        gameState, simRegion, &_entity->sword
                    );

                Assert(!IsSet(&source->sim, EntityFlag_Simming));
                AddFlag(&source->sim, EntityFlag_Simming);
            }
        
            _entity->storageIndex = storageIndex;
            _entity->isUpdatable = false;
        }
        else
        {
            InvalidCodePath;
        }
    }
    
    return _entity;
}

internal sim_entity *
AddEntity
(
    game_state *gameState,
    sim_region *simRegion, ui32 storageIndex,
    low_entity *source, v3 *simPos
)
{
    sim_entity *dest = AddEntityRaw
        (
            gameState,
            simRegion, storageIndex, source
        );
    if(dest)
    {
        if(simPos)
        {
            dest->pos = *simPos;
            dest->isUpdatable = IsInRectangle
                (
                    simRegion->updatableBounds,
                    dest->pos
                );
        }
        else
        {
            dest->pos = GetSimSpacePos
                (
                    simRegion, source
                );
        }
    }

    return dest;
}

internal sim_region *
BeginSim
(
    memory_arena *simArena, game_state *gameState, world *_world,
    world_position origin, rectangle3 bounds
)
{
    sim_region *simRegion = PushStruct(simArena, sim_region);
    ZeroStruct(simRegion->hash);

    // TODO: IMPORTANT: Calculate this from the max value of all
    // entities radius + their speed
    r32 updateSafetyMargin = 1.0f;
    r32 updateSafetyMarginZ = 1.0f;
    
    simRegion->_world = _world;
    simRegion->origin = origin;
    simRegion->updatableBounds = bounds;
    simRegion->bounds = AddRadiusTo
        (
            simRegion->updatableBounds,
            v3{updateSafetyMargin,
               updateSafetyMargin,
               updateSafetyMarginZ}
        );

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
                        if(!IsSet(&low->sim, EntityFlag_Nonspatial))
                        {
                            v3 simSpacePos = GetSimSpacePos
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

    return simRegion;
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
        
        Assert(IsSet(&stored->sim, EntityFlag_Simming));    
        stored->sim = *_entity;
        Assert(!IsSet(&stored->sim, EntityFlag_Simming));
        
        StoreEntityReference(&stored->sim.sword);
        
        world_position newPos = IsSet(_entity, EntityFlag_Nonspatial) ?
            NullPosition() :
            MapIntoChunkSpace
            (
                gameState->_world,
                region->origin, _entity->pos
            );
        
        ChangeEntityLocation
        (
            &gameState->worldArena,
            gameState->_world,
            _entity->storageIndex, stored,
            newPos
        );
    
        if(_entity->storageIndex == gameState->cameraFollowingEntityIndex)
        {
            world_position newCameraPos = gameState->cameraPos;
			newCameraPos.chunkZ = stored->pos.chunkZ;

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

            gameState->cameraPos = newCameraPos;
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

internal b32
ShouldCollide
(
    game_state *gameState, sim_entity *a, sim_entity *b
)
{
    b32 result = false;

    if(a != b)
    {
        if(a->storageIndex > b->storageIndex)
        {
            sim_entity *temp = a;
            a = b;
            b = temp;
        }
    
        if(!IsSet(a, EntityFlag_Nonspatial) &&
           !IsSet(b, EntityFlag_Nonspatial))
        {
            result = true;
        }

        // TODO: BETTER HASH FUNCTION
        ui32 hashBucket = a->storageIndex & (ArrayCount(gameState->collisionRuleHash) - 1);
        for(pairwise_collision_rule *rule = gameState->collisionRuleHash[hashBucket];
            rule;
            rule = rule->nextInHash)
        {
            if(rule->storageIndexA == a->storageIndex &&
               rule->storageIndexB == b->storageIndex)
            {
                result = rule->shouldCollide;
                break;
            }
        }
    }
    
    return result;
}

internal b32
HandleCollision(sim_entity *a, sim_entity *b)
{
    b32 stopsOnCollision = false;

    if(a->type == EntityType_Sword)
    {
        stopsOnCollision = false;
    }
    else
    {
        stopsOnCollision = true;
    }
    
    if(a->type > b->type)
    {
        sim_entity *temp = a;
        a = b;
        b = temp;
    }
    
    if(a->type == EntityType_Monster &&
       b->type == EntityType_Sword)
    {
        if(a->hitPointMax > 0)
        {
            a->hitPointMax--;
        }
    }
                
    // TODO: Stairs
    // TODO: Implement this for real
    return stopsOnCollision;
}

internal void
MoveEntity
(
    game_state *gameState, sim_region *simRegion,
    sim_entity *_entity, r32 deltaTime,
    move_spec *moveSpec, v3 ddPos
)
{
    Assert(!IsSet(_entity, EntityFlag_Nonspatial));
        
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
    ddPos += v3{0, 0, -9.8f};

    v3 oldPlayerPos = _entity->pos;
    v3 deltaPlayerPos = 0.5f * ddPos *
        Square(deltaTime) + _entity->dPos * deltaTime;
    
    _entity->dPos = ddPos * deltaTime + _entity->dPos;
    v3 newPlayerPos = oldPlayerPos + deltaPlayerPos;

    r32 distanceRemaining = _entity->distanceLimit;
    if(distanceRemaining == 0.0f)
    {
        // TODO: Maybe formalize this number?
        distanceRemaining = 10000.0f;
    }
    
    for(ui32 i = 0; i < 4; i++)
    {
        r32 tMin = 1.0f;
        r32 playerDeltaLength = Length(deltaPlayerPos);
        // TODO: Epsilons??
        if(playerDeltaLength > 0.0f)
        {
            if(playerDeltaLength > distanceRemaining)
            {
                tMin = distanceRemaining / playerDeltaLength;
            }
        
            v3 wallNormal = {};
            sim_entity *hitEntity = 0;

            v3 desiredPos = _entity->pos + deltaPlayerPos;

            if(!IsSet(_entity, EntityFlag_Nonspatial))
            {
                // TODO: Spatial partition here
                for(ui32 highEntityIndex = 0;
                    highEntityIndex < simRegion->entityCount;
                    highEntityIndex++)
                {
                    sim_entity *testEntity = simRegion->entities + highEntityIndex;
                    if(ShouldCollide(gameState, _entity, testEntity))
                    {                        
                        v3 minkowskiDiameter =
                            {
                                testEntity->width + _entity->width,
                                testEntity->height + _entity->height,
                                2.0f * _world->tileDepthInMeters
                            };
                        
                        v3 minCorner = -0.5f * minkowskiDiameter;                        
                        v3 maxCorner = 0.5f * minkowskiDiameter;

                        v3 rel = _entity->pos - testEntity->pos;
                
                        if(TestWall(minCorner.x, rel.x, rel.y,
                                    deltaPlayerPos.x, deltaPlayerPos.y,
                                    &tMin, minCorner.y, maxCorner.y))
                        {
                            wallNormal = v3{-1, 0, 0};
                            hitEntity = testEntity;
                        }                    
                
                        if(TestWall(maxCorner.x, rel.x, rel.y,
                                    deltaPlayerPos.x, deltaPlayerPos.y,
                                    &tMin, minCorner.y, maxCorner.y))
                        {
                            wallNormal = v3{1, 0, 0};
                            hitEntity = testEntity;
                        }
                
                        if(TestWall(minCorner.y, rel.y, rel.x,
                                    deltaPlayerPos.y, deltaPlayerPos.x,
                                    &tMin, minCorner.x, maxCorner.x))
                        {
                            wallNormal = v3{0, -1, 0};
                            hitEntity = testEntity;
                        }
                
                        if(TestWall(maxCorner.y, rel.y, rel.x,
                                    deltaPlayerPos.y, deltaPlayerPos.x,
                                    &tMin, minCorner.x, maxCorner.x))
                        {
                            wallNormal = v3{0, 1, 0};
                            hitEntity = testEntity;
                        }                   
                    }
                }
            }
        
            _entity->pos += tMin * deltaPlayerPos;
            distanceRemaining -= tMin * playerDeltaLength;
            if(hitEntity)
            {
                deltaPlayerPos = desiredPos - _entity->pos;
                                
                b32 stopsOnCollision = HandleCollision
                    (
                        _entity, hitEntity
                    );
                if(stopsOnCollision)
                {
                    _entity->dPos = _entity->dPos - 1 *
                        Inner(_entity->dPos, wallNormal) * wallNormal;

                    deltaPlayerPos = deltaPlayerPos - 1 *
                        Inner(deltaPlayerPos, wallNormal) * wallNormal;
                }
                else
                {
                    AddCollisionRule
                        (
                            gameState,
                            _entity->storageIndex,
                            hitEntity->storageIndex,
                            false
                        );
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    if(_entity->pos.z < 0)
    {
        _entity->pos.z = 0;
    }
    
    if(_entity->distanceLimit != 0.0f)
    {
        _entity->distanceLimit = distanceRemaining;
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
