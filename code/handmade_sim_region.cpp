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
                AddFlags(&source->sim, EntityFlag_Simming);
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

inline b32
EntityOverlapsRectangle
(
    v3 pos,
    v3 dim,
    rectangle3 rect
)
{
    rectangle3 grown = AddRadiusTo(rect, 0.5f * dim);
    b32 result = IsInRectangle(grown, pos);
    return result;
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
            dest->isUpdatable = EntityOverlapsRectangle
                (
                    dest->pos,
                    dest->dim,
                    simRegion->updatableBounds
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
    world_position origin, rectangle3 bounds, r32 deltaTime
)
{
    sim_region *simRegion = PushStruct(simArena, sim_region);
    ZeroStruct(simRegion->hash);

    // TODO: IMPORTANT: Calculate this from the max value of all
    // entities radius + their speed
    simRegion->maxEntityRadius = 5.0f;
    simRegion->maxEntityVelocity = 30.0f;
    r32 updateSafetyMargin = simRegion->maxEntityRadius +
        deltaTime * simRegion->maxEntityVelocity;
    r32 updateSafetyMarginZ = 1.0f;
    
    simRegion->_world = _world;
    simRegion->origin = origin;
    simRegion->updatableBounds = AddRadiusTo
        (
            bounds,
            v3{simRegion->maxEntityRadius,
               simRegion->maxEntityRadius,
               simRegion->maxEntityRadius}
        );
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
    
    for(i32 chunkZ = minChunkPos.chunkZ;
        chunkZ <= maxChunkPos.chunkZ;
        chunkZ++)
    {
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
                        _world, chunkX, chunkY, chunkZ
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
                            
                                if(EntityOverlapsRectangle
                                   (simSpacePos,
                                    low->sim.dim,
                                    simRegion->bounds))
                                {
                                    AddEntity
                                        (
                                            gameState, simRegion,
                                            lowEntityIndex, low,
                                            &simSpacePos
                                        );
                                }
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
            r32 camZOffset = newCameraPos._offset.z;
            newCameraPos = stored->pos;
            newCameraPos._offset.z = camZOffset;
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
CanCollide
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
                result = rule->canCollide;
                break;
            }
        }
    }
    
    return result;
}

internal b32
HandleCollision
(
    game_state *gameState,
    sim_entity *a, sim_entity *b
)
{
    b32 stopsOnCollision = false;

    if(a->type == EntityType_Sword)
    {   
        AddCollisionRule
            (
                gameState,
                a->storageIndex,
                b->storageIndex,
                false
            );
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
    return stopsOnCollision;
}

internal b32
CanOverlap
(
    game_state *gameState,
    sim_entity *mover, sim_entity *region
)
{
    b32 result = false;

    if(mover != region)
    {
        if(region->type == EntityType_Stairwell)
        {
            result = true;
        }
    }
    
    return result;
}

internal void
HandleOverlap
(
    game_state *gameState,
    sim_entity *mover, sim_entity *region,
    r32 deltaTime, r32 *ground
)
{
    if(region->type == EntityType_Stairwell)
    {
        rectangle3 regionRect = RectCenterDim
            (
                region->pos, region->dim
            );
        
        v3 bary = Clamp01(GetBarycentric
            (
                regionRect,
                mover->pos
            ));

        *ground = Lerp
            (
                regionRect.min.z, bary.y, regionRect.max.z
            );
    }
}

internal b32
SpeculativeCollide(sim_entity *mover, sim_entity *region)
{
    b32 result = true;
    
    if(region->type == EntityType_Stairwell)
    {
        rectangle3 regionRect = RectCenterDim
            (
                region->pos, region->dim
            );
        
        v3 bary = Clamp01(GetBarycentric
            (
                regionRect,
                mover->pos
            ));

        r32 ground = Lerp
            (
                regionRect.min.z, bary.y, regionRect.max.z
            );
        r32 stepHeight = 0.1f;
        result = (AbsoluteValue(mover->pos.z - ground) > stepHeight ||
                  (bary.y > 0.1f && bary.y < 0.9f));
    }

    return result;
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
    if(!IsSet(_entity, EntityFlag_ZSupported))
    {
        ddPos += v3{0, 0, -9.8f}; // NOTE: Gravity
    }
    
    v3 oldPlayerPos = _entity->pos;
    v3 deltaPlayerPos = 0.5f * ddPos *
        Square(deltaTime) + _entity->dPos * deltaTime;
    
    _entity->dPos = ddPos * deltaTime + _entity->dPos;
    Assert(LengthSq(_entity->dPos) <= Square(simRegion->maxEntityVelocity));
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
                    if(CanCollide(gameState, _entity, testEntity))
                    {                        
                        v3 minkowskiDiameter =
                            {
                                testEntity->dim.x + _entity->dim.x,
                                testEntity->dim.y + _entity->dim.y,
                                testEntity->dim.z + _entity->dim.z
                            };
                        
                        v3 minCorner = -0.5f * minkowskiDiameter;                        
                        v3 maxCorner = 0.5f * minkowskiDiameter;

                        v3 rel = _entity->pos - testEntity->pos;

                        r32 tMinTest = tMin;
                        v3 testWallNormal = {};
                        b32 hitThis = false;
                        if(TestWall(minCorner.x, rel.x, rel.y,
                                    deltaPlayerPos.x, deltaPlayerPos.y,
                                    &tMinTest, minCorner.y, maxCorner.y))
                        {
                            testWallNormal = v3{-1, 0, 0};
                            hitThis = true;
                        }                    
                
                        if(TestWall(maxCorner.x, rel.x, rel.y,
                                    deltaPlayerPos.x, deltaPlayerPos.y,
                                    &tMinTest, minCorner.y, maxCorner.y))
                        {
                            testWallNormal = v3{1, 0, 0};
                            hitThis = true;
                        }
                
                        if(TestWall(minCorner.y, rel.y, rel.x,
                                    deltaPlayerPos.y, deltaPlayerPos.x,
                                    &tMinTest, minCorner.x, maxCorner.x))
                        {
                            testWallNormal = v3{0, -1, 0};
                            hitThis = true;
                        }
                
                        if(TestWall(maxCorner.y, rel.y, rel.x,
                                    deltaPlayerPos.y, deltaPlayerPos.x,
                                    &tMinTest, minCorner.x, maxCorner.x))
                        {
                            testWallNormal = v3{0, 1, 0};
                            hitThis = true;
                        }

                        if(hitThis)
                        {
                            v3 testPos = _entity->pos + tMinTest * deltaPlayerPos;
                            if(SpeculativeCollide(_entity, testEntity))
                            {
                                tMin = tMinTest;
                                wallNormal = testWallNormal;
                                hitEntity = testEntity;
                            }
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
                        gameState,
                        _entity, hitEntity
                    );
                if(stopsOnCollision)
                {
                    _entity->dPos = _entity->dPos - 1 *
                        Inner(_entity->dPos, wallNormal) * wallNormal;

                    deltaPlayerPos = deltaPlayerPos - 1 *
                        Inner(deltaPlayerPos, wallNormal) * wallNormal;
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

    r32 ground = 0.0f; 
    
    // NOTE: Handle events based on area overlapping
    {
        rectangle3 entityRect = RectCenterDim
            (
                _entity->pos,
                _entity->dim
            );
        for(ui32 highEntityIndex = 0;
            highEntityIndex < simRegion->entityCount;
            highEntityIndex++)
        {
            sim_entity *testEntity = simRegion->entities + highEntityIndex;
            if(CanOverlap(gameState, _entity, testEntity))
            {
                rectangle3 testEntityRect = RectCenterDim
                    (
                        testEntity->pos,
                        testEntity->dim
                    );
                if(RectanglesIntersect(entityRect,
                                       testEntityRect))
                {
                    HandleOverlap
                        (
                            gameState,
                            _entity, testEntity,
                            deltaTime, &ground
                        );
                }
            }
        }
    }

    if(_entity->pos.z <= ground ||
       (IsSet(_entity, EntityFlag_ZSupported) &&
        _entity->dPos.z == 0.0f))
    {
        _entity->pos.z = ground;
        _entity->dPos.z = 0;
        AddFlags(_entity, EntityFlag_ZSupported);
    }
    else
    {
        ClearFlags(_entity, EntityFlag_ZSupported);
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
