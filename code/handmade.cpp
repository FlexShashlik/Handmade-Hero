#include "handmade.h"
#include "handmade_world.cpp"
#include "handmade_random.h"

internal void
GameOutputSound(game_state *gameState, game_sound_output_buffer *soundBuffer)
{
    i16 toneVolume = 3000;
    i32 wavePeriod = soundBuffer->samplesPerSecond / 400;
    i16 *sampleOut = soundBuffer->samples;
    
    for(i32 sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; sampleIndex++)
    {
#if 0
        real32 sineValue = sinf(gameState->tSine);
        i16 sampleValue = (i16)(sineValue * toneVolume);
#else
        i16 sampleValue = 0;
#endif
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
#if 0
        gameState->tSine += 2.0f * Pi32 / (real32)wavePeriod;
        if(gameState->tSine > 2.0f * Pi32)
        {
            gameState->tSine -= 2.0f * Pi32;
        }
#endif
    }
}

internal void
DrawRectangle
(
    game_offscreen_buffer *buffer,
    v2 vMin, v2 vMax,
    r32 r, r32 g, r32 b
)
{
    i32 minX = RoundR32ToI32(vMin.x);
    i32 minY = RoundR32ToI32(vMin.y);
    i32 maxX = RoundR32ToI32(vMax.x);
    i32 maxY = RoundR32ToI32(vMax.y);

    if(minX < 0)
    {
        minX = 0;
    }

    if(minY < 0)
    {
        minY = 0;
    }

    if(maxX > buffer->width)
    {
        maxX = buffer->width;
    }

    if(maxY > buffer->height)
    {
        maxY = buffer->height;
    }
    
    ui8 *endOfBuffer = (ui8 *)buffer->memory + buffer->pitch * buffer->height;
    
    ui32 color =
        (
            RoundR32ToUI32(r * 255.0f) << 16 |
            RoundR32ToUI32(g * 255.0f) << 8 |
            RoundR32ToUI32(b * 255.0f) << 0
        );

    ui8 *row = (ui8 *)buffer->memory + minX * buffer->bytesPerPixel + minY * buffer->pitch;
    for(i32 y = minY; y < maxY; y++)
    {
        ui32 *pixel = (ui32 *)row;
        for(i32 x = minX; x < maxX; x++)
        {        
            *pixel++ = color;            
        }
        
        row += buffer->pitch;
    }
}

internal void
DrawBitmap
(
    game_offscreen_buffer *buffer,
    loaded_bitmap *bmp,
    r32 rX, r32 rY,
    r32 cAlpha = 1.0f
)
{
    i32 minX = RoundR32ToI32(rX);
    i32 minY = RoundR32ToI32(rY);
    i32 maxX = minX + bmp->width;
    i32 maxY = minY + bmp->height;

    i32 sourceOffsetX = 0;    
    if(minX < 0)
    {
        sourceOffsetX = -minX;
        minX = 0;
    }

    i32 sourceOffsetY = 0;
    if(minY < 0)
    {
        sourceOffsetY = -minY;
        minY = 0;
    }

    if(maxX > buffer->width)
    {
        maxX = buffer->width;
    }

    if(maxY > buffer->height)
    {
        maxY = buffer->height;
    }
    
    ui32 *sourceRow = bmp->pixels +
        bmp->width * (bmp->height - 1);

    sourceRow += -sourceOffsetY * bmp->width + sourceOffsetX;
    
    ui8 *destRow = (ui8 *)buffer->memory +
        minX * buffer->bytesPerPixel + minY * buffer->pitch;
    
    for(i32 y = minY; y < maxY; y++)
    {        
        ui32 *dest = (ui32 *)destRow;
        ui32 *source = sourceRow;

        for(i32 x = minX; x < maxX; x++)
        {
            r32 a = (r32)((*source >> 24) & 0xFF) / 255.0f;
            a *= cAlpha;
            
            r32 sr = (r32)((*source >> 16) & 0xFF);
            r32 sg = (r32)((*source >> 8) & 0xFF);
            r32 sb = (r32)((*source >> 0) & 0xFF);

            r32 dr = (r32)((*dest >> 16) & 0xFF);
            r32 dg = (r32)((*dest >> 8) & 0xFF);
            r32 db = (r32)((*dest >> 0) & 0xFF);

            r32 r = (1.0f - a) * dr + a * sr;
            r32 g = (1.0f - a) * dg + a * sg;
            r32 b = (1.0f - a) * db + a * sb;

            *dest = ((ui32)(r + 0.5f) << 16|
                     (ui32)(g + 0.5f) << 8 |
                     (ui32)(b + 0.5f) << 0);
            
            dest++;
            source++;
        }

        destRow += buffer->pitch;
        sourceRow -= bmp->width;
    }
}

#pragma pack(push, 1)
struct bitmap_header
{
    ui16 fileType;
	ui32 fileSize;
	ui16 reserved1;
	ui16 reserved2;
	ui32 bitmapOffset;
    ui32 size;
    i32 width;
	i32 height;
	ui16 planes;
	ui16 bitsPerPixel;
    ui32 compression;
    ui32 sizeOfBitmap;
    i32 horzResolution;
    i32 vertResolution;
    ui32 colorsUsed;
    ui32 colorsImportant;

    ui32 redMask;
    ui32 greenMask;
    ui32 blueMask;
};
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP
(
    thread_context *thread,
    debug_platform_read_entire_file *readEntireFile,
    char *fileName
)
{
    loaded_bitmap result = {};

    // NOTE: Byte order in memory is determined by the header itself
    
    debug_read_file_result readResult;
    readResult = readEntireFile(thread, fileName);

    if(readResult.contentsSize != 0)
    {
        bitmap_header *header = (bitmap_header *)readResult.contents;

        Assert(header->compression == 3);
        
        result.width = header->width;
        result.height = header->height;

        ui32 *pixels = (ui32 *)((ui8 *)readResult.contents + header->bitmapOffset);
        result.pixels = pixels;
        
        ui32 redMask = header->redMask;
        ui32 greenMask = header->greenMask;
        ui32 blueMask = header->blueMask;
        ui32 alphaMask = ~(redMask | greenMask | blueMask);
        
        bit_scan_result redScan = FindLeastSignificantSetBit(redMask);
        bit_scan_result greenScan = FindLeastSignificantSetBit(greenMask);
        bit_scan_result blueScan = FindLeastSignificantSetBit(blueMask);
        bit_scan_result alphaScan = FindLeastSignificantSetBit(alphaMask);

        Assert(redScan.isFound);
        Assert(greenScan.isFound);
        Assert(blueScan.isFound);
        Assert(alphaScan.isFound);

        i32 redShift = 16 - (i32)redScan.index;
        i32 greenShift = 8 - (i32)greenScan.index;
        i32 blueShift = 0 - (i32)blueScan.index;
        i32 alphaShift = 24 - (i32)alphaScan.index;
        
        ui32 *sourceDest = pixels;
        for(i32 y = 0; y < header->height; y++)
        {
            for(i32 x = 0; x < header->width; x++)
            {
                ui32 c = *sourceDest;
                *sourceDest++ = (RotateLeft(c & redMask, redShift) |
                                 RotateLeft(c & greenMask, greenShift) |
                                 RotateLeft(c & blueMask, blueShift) |
                                 RotateLeft(c & alphaMask, alphaShift));
            }
        }
    }

    return result;
}

inline low_entity *
GetLowEntity(game_state *gameState, ui32 index)
{
    low_entity *result = 0;
    
    if(index > 0 && index < gameState->lowEntityCount)
    {
        result = gameState->lowEntities + index;
    }

    return result;
}

inline v2
GetCameraSpacePos(game_state *gameState, low_entity *lowEntity)
{
    // NOTE: Map the entity into camera space
    world_difference diff = Subtract
        (
            gameState->_world,
            &lowEntity->pos, &gameState->cameraPos
        );

    v2 result = diff.dXY;

    return result;
}

inline high_entity *
MakeEntityHighFrequency
(
    game_state *gameState, low_entity *lowEntity,
    ui32 lowIndex, v2 cameraSpacePos
)
{
    high_entity *highEntity = 0;

    Assert(!lowEntity->highEntityIndex);
    if(!lowEntity->highEntityIndex)
    {
        if(gameState->highEntityCount < ArrayCount(gameState->_highEntities))
        {
            ui32 highIndex = gameState->highEntityCount++;
            highEntity = gameState->_highEntities + highIndex;
    
            highEntity->pos = cameraSpacePos;
            highEntity->dPos = v2{0, 0};
            highEntity->chunkZ = lowEntity->pos.chunkZ;
            highEntity->facingDirection = 0;
            highEntity->lowEntityIndex = lowIndex;

            lowEntity->highEntityIndex = highIndex;
        }
        else
        {
            InvalidCodePath;
        }
    }

    return highEntity;
}

inline high_entity *
MakeEntityHighFrequency(game_state *gameState, ui32 lowIndex)
{
    low_entity *lowEntity = gameState->lowEntities + lowIndex;
    high_entity *highEntity = 0;

    if(lowEntity->highEntityIndex)
    {
        highEntity = gameState->_highEntities + lowEntity->highEntityIndex;
    }
    else
    {
        v2 cameraSpacePos = GetCameraSpacePos
            (
                gameState, lowEntity 
            );
        highEntity = MakeEntityHighFrequency
            (
                gameState,
                lowEntity, lowIndex, cameraSpacePos
            );
    }
    
    return highEntity;
}

inline entity
ForceEntityIntoHigh(game_state *gameState, ui32 lowIndex)
{
    entity result = {};
    
    if(lowIndex > 0 && lowIndex < gameState->lowEntityCount)
    {
        result.lowIndex = lowIndex;
        result.low = gameState->lowEntities + lowIndex;
        result.high = MakeEntityHighFrequency(gameState, lowIndex);    
    }
    
    return result;
}

inline void
MakeEntityLowFrequency(game_state *gameState, ui32 lowIndex)
{
    low_entity *lowEntity = &gameState->lowEntities[lowIndex];
    ui32 highIndex = lowEntity->highEntityIndex;
    if(highIndex)
    {
        ui32 lastHighIndex = gameState->highEntityCount - 1;
        if(highIndex != lastHighIndex)
        {
            high_entity *lastEntity = gameState->_highEntities + lastHighIndex;
            high_entity *delEntity = gameState->_highEntities + highIndex;

            *delEntity = *lastEntity;
            gameState->lowEntities[lastEntity->lowEntityIndex]
                .highEntityIndex = highIndex;
        }
        
        gameState->highEntityCount--;
        lowEntity->highEntityIndex = 0;
    }
}

inline b32
ValidateEntityPairs(game_state *gameState)
{
    b32 isValid = true;

    for(ui32 highEntityIndex = 1;
        highEntityIndex < gameState->highEntityCount;
        highEntityIndex++)
    {
        high_entity *high = gameState->_highEntities + highEntityIndex;
        isValid = isValid && (gameState->lowEntities[high->lowEntityIndex].highEntityIndex == highEntityIndex);
    }

    return isValid;
}

inline void
OffsetAndCheckFrequencyByArea
(
    game_state *gameState, v2 offset, rectangle2 bounds
)
{
    for(ui32 highEntityIndex = 1;
        highEntityIndex < gameState->highEntityCount;
        )
    {
        high_entity *high = gameState->_highEntities + highEntityIndex;
        low_entity *low = gameState->lowEntities + high->lowEntityIndex;

        high->pos += offset;
        if(IsValid(low->pos) && IsInRectangle(bounds, high->pos))
        {
            highEntityIndex++;
        }
        else
        {
            Assert(low->highEntityIndex == highEntityIndex);
            MakeEntityLowFrequency(gameState, high->lowEntityIndex);
        }
    }
}

struct add_low_entity_result
{
    low_entity *low;
    ui32 lowIndex;
};

internal add_low_entity_result
AddLowEntity
(
    game_state *gameState,
    entity_type type, world_position *pos
)
{
    Assert(gameState->lowEntityCount < ArrayCount(gameState->lowEntities));
    ui32 entityIndex = gameState->lowEntityCount++;

    low_entity *lowEntity = gameState->lowEntities + entityIndex;
    *lowEntity = {};
    lowEntity->type = type;

    ChangeEntityLocation
        (
            &gameState->worldArena,
            gameState->_world,
            entityIndex, lowEntity,
            0, pos
        );

    add_low_entity_result result = {};
    result.low = lowEntity;
    result.lowIndex = entityIndex;
    
    return result;
}

internal void
InitHitPoints(low_entity *lowEntity, ui32 hitPointCount)
{
    Assert(hitPointCount < ArrayCount(lowEntity->hitPoint));
    lowEntity->hitPointMax = hitPointCount;

    for(ui32 hitPointIndex = 0;
        hitPointIndex < lowEntity->hitPointMax;
        hitPointIndex++)
    {
        hit_point *hitPoint = lowEntity->hitPoint + hitPointIndex;
        hitPoint->flags = 0;
        hitPoint->filledAmount = HIT_POINT_SUB_COUNT;
    }
}

internal add_low_entity_result
AddSword(game_state *gameState)
{
    add_low_entity_result _entity = AddLowEntity
        (
            gameState, EntityType_Sword, 0
        );

    _entity.low->height = 0.5f;
    _entity.low->width = 1.0f;
    _entity.low->isCollides = false;

    return _entity;
}

internal add_low_entity_result
AddPlayer(game_state *gameState)
{
    world_position pos = gameState->cameraPos;
    add_low_entity_result _entity = AddLowEntity
        (
            gameState, EntityType_Hero, &pos
        );

    _entity.low->height = 0.5f;
    _entity.low->width = 1.0f;
    _entity.low->isCollides = true;

    InitHitPoints(_entity.low, 3);

    add_low_entity_result sword = AddSword(gameState);
    _entity.low->swordIndex = sword.lowIndex;
    
    if(gameState->cameraFollowingEntityIndex == 0)
    {
        gameState->cameraFollowingEntityIndex = _entity.lowIndex;
    }

    return _entity;
}

internal add_low_entity_result
AddMonster
(
    game_state *gameState,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
)
{
    world_position pos = ChunkPosFromTilePos
        (
            gameState->_world,
            absTileX, absTileY, absTileZ
        );
    add_low_entity_result _entity = AddLowEntity
        (
            gameState, EntityType_Monster, &pos
        );

    _entity.low->height = 0.5f;
    _entity.low->width = 1.0f;
    _entity.low->isCollides = true;
    
    InitHitPoints(_entity.low, 3);

    return _entity;
}

internal add_low_entity_result
AddFamiliar
(
    game_state *gameState,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
)
{
    world_position pos = ChunkPosFromTilePos
        (
            gameState->_world,
            absTileX, absTileY, absTileZ
        );
    add_low_entity_result _entity = AddLowEntity
        (
            gameState, EntityType_Familiar, &pos
        );

    _entity.low->height = 0.5f;
    _entity.low->width = 1.0f;
    _entity.low->isCollides = true;

    return _entity;
}

internal add_low_entity_result
AddWall
(
    game_state *gameState,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
)
{
    world_position pos = ChunkPosFromTilePos
        (
            gameState->_world,
            absTileX, absTileY, absTileZ
        );
    add_low_entity_result _entity = AddLowEntity
        (
            gameState, EntityType_Wall, &pos
        );
    
    _entity.low->height = gameState->_world->tileSideInMeters;
    _entity.low->width = _entity.low->height;
    _entity.low->isCollides = true;

    return _entity;
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

struct move_spec
{
    b32 isUnitMaxAccelVector;
    r32 speed;
    r32 drag;
};

inline move_spec
DefaultMoveSpec(void)
{
    move_spec result = {};
    result.isUnitMaxAccelVector = false;
    result.speed = 1.0f;
    result.drag = 0.0f;

    return result;
}

internal void
MoveEntity
(
    game_state *gameState,
    entity _entity, r32 deltaTime,
    move_spec *moveSpec, v2 ddPos
)
{
    world *_world = gameState->_world;

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
    ddPos += -moveSpec->drag * _entity.high->dPos;

    v2 oldPlayerPos = _entity.high->pos;
    v2 deltaPlayerPos = 0.5f * ddPos *
        Square(deltaTime) + _entity.high->dPos * deltaTime;
    _entity.high->dPos = ddPos * deltaTime +
        _entity.high->dPos;
    
    v2 newPlayerPos = oldPlayerPos + deltaPlayerPos;
    
    for(ui32 i = 0; i < 4; i++)
    {
        r32 tMin = 1.0f;
        v2 wallNormal = {};
        ui32 hitHighEntityIndex = 0;

        v2 desiredPos = _entity.high->pos + deltaPlayerPos;

        if(_entity.low->isCollides)
        {
            for(ui32 highEntityIndex = 1;
                highEntityIndex < gameState->highEntityCount;
                highEntityIndex++)
            {
                if(highEntityIndex != _entity.low->highEntityIndex)
                {
                    entity testEntity = {};
                    testEntity.high = gameState->_highEntities + highEntityIndex;
                    testEntity.lowIndex = testEntity.high->lowEntityIndex;
                    testEntity.low = gameState->lowEntities + testEntity.lowIndex;
                    if(testEntity.low->isCollides)
                    {
                        r32 diameterW = testEntity.low->width + _entity.low->width;
                        r32 diameterH = testEntity.low->height + _entity.low->height;
                
                        v2 minCorner = -0.5f * v2{diameterW, diameterH};                        
                        v2 maxCorner = 0.5f * v2{diameterW, diameterH};

                        v2 rel = _entity.high->pos - testEntity.high->pos;
                
                        if(TestWall(minCorner.x, rel.x, rel.y,
                                    deltaPlayerPos.x, deltaPlayerPos.y,
                                    &tMin, minCorner.y, maxCorner.y))
                        {
                            wallNormal = v2{-1, 0};
                            hitHighEntityIndex = highEntityIndex;
                        }                    
                
                        if(TestWall(maxCorner.x, rel.x, rel.y,
                                    deltaPlayerPos.x, deltaPlayerPos.y,
                                    &tMin, minCorner.y, maxCorner.y))
                        {
                            wallNormal = v2{1, 0};
                            hitHighEntityIndex = highEntityIndex;
                        }
                
                        if(TestWall(minCorner.y, rel.y, rel.x,
                                    deltaPlayerPos.y, deltaPlayerPos.x,
                                    &tMin, minCorner.x, maxCorner.x))
                        {
                            wallNormal = v2{0, -1};
                            hitHighEntityIndex = highEntityIndex;
                        }
                
                        if(TestWall(maxCorner.y, rel.y, rel.x,
                                    deltaPlayerPos.y, deltaPlayerPos.x,
                                    &tMin, minCorner.x, maxCorner.x))
                        {
                            wallNormal = v2{0, 1};
                            hitHighEntityIndex = highEntityIndex;
                        }
                    }
                }
            }
        }
        
        _entity.high->pos += tMin * deltaPlayerPos;
        if(hitHighEntityIndex)
        {
            _entity.high->dPos = _entity.high->dPos - 1 *
                Inner(_entity.high->dPos, wallNormal) * wallNormal;

            deltaPlayerPos = desiredPos - _entity.high->pos;
            deltaPlayerPos = deltaPlayerPos - 1 *
                Inner(deltaPlayerPos, wallNormal) * wallNormal;
            
            high_entity *hitHigh = gameState->_highEntities + hitHighEntityIndex;
            low_entity *hitLow = gameState->lowEntities + hitHigh->lowEntityIndex;
            
            //_entity.high->absTileZ += hitLow->deltaAbsTileZ;
        }
        else
        {
            break;
        }
    }

    if(_entity.high->dPos.x == 0.0f && _entity.high->dPos.y == 0.0f)
    {
        // NOTE: Leave facing direction whatever it was
    }
    else if(AbsoluteValue(_entity.high->dPos.x) >
            AbsoluteValue(_entity.high->dPos.y))
    {
        if(_entity.high->dPos.x > 0)
        {
            _entity.high->facingDirection = 0;
        }
        else
        {
            _entity.high->facingDirection = 2;
        }
    }
    else if(AbsoluteValue(_entity.high->dPos.x) <
            AbsoluteValue(_entity.high->dPos.y))
    {
        if(_entity.high->dPos.y > 0)
        {
            _entity.high->facingDirection = 1;
        }
        else
        {
            _entity.high->facingDirection = 3;
        }
    }

    world_position newPos = MapIntoChunkSpace
        (
            gameState->_world,
            gameState->cameraPos, _entity.high->pos
        );
    ChangeEntityLocation
        (
            &gameState->worldArena,
            gameState->_world,
            _entity.lowIndex, _entity.low,
            &_entity.low->pos, &newPos
        );
}

internal void
SetCamera
(
    game_state *gameState,
    world_position newCameraPos
)
{
    Assert(ValidateEntityPairs(gameState));
        
    world *_world = gameState->_world;
    world_difference deltaCameraPos = Subtract
        (
            _world, &newCameraPos, &gameState->cameraPos
        );
    gameState->cameraPos = newCameraPos;

    ui32 tileSpanX = 17 * 3;
    ui32 tileSpanY = 9 * 3;
    rectangle2 cameraBounds = RectCenterDim
        (
            v2{0, 0},
            _world->tileSideInMeters * v2{(r32)tileSpanX, (r32)tileSpanY}
         );

    v2 entityOffsetForFrame = -deltaCameraPos.dXY;
    OffsetAndCheckFrequencyByArea
        (
            gameState,
            entityOffsetForFrame, cameraBounds
        );

    world_position minChunkPos = MapIntoChunkSpace
        (
            _world,
            newCameraPos, GetMinCorner(cameraBounds)
        );
    world_position maxChunkPos = MapIntoChunkSpace
        (
            _world,
            newCameraPos, GetMaxCorner(cameraBounds)
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
                    _world, chunkX, chunkY, newCameraPos.chunkZ
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
                        if(!low->highEntityIndex)
                        {
                            v2 cameraSpacePos = GetCameraSpacePos
                                (
                                    gameState, low
                                );
                            if(IsInRectangle(cameraBounds,
                                             cameraSpacePos))
                            {
                                MakeEntityHighFrequency
                                    (
                                        gameState,
                                        low, lowEntityIndex,
                                        cameraSpacePos
                                    );
                            }
                        }
                    }
                }
            }
        }
    }

    Assert(ValidateEntityPairs(gameState));
}

inline void
PushPiece
(
    entity_visible_piece_group *group,
    loaded_bitmap *bitmap,
    v2 offset, r32 offsetZ,
    v2 align, v2 dim,
    v4 color,
    r32 entityZC = 1.0f
)
{
    Assert(group->pieceCount < ArrayCount(group->pieces));
    entity_visible_piece *piece = group->pieces + group->pieceCount++;
    piece->bitmap = bitmap;
    piece->offset = group->gameState->metersToPixels * v2{offset.x, -offset.y} - align;
    piece->offsetZ = group->gameState->metersToPixels * offsetZ;
    piece->r = color.r;
    piece->g = color.g;
    piece->b = color.b;
    piece->a = color.a;
    piece->entityZC = entityZC;
    piece->dim = dim;
}

inline void
PushBitmap
(
    entity_visible_piece_group *group,
    loaded_bitmap *bitmap,
    v2 offset, r32 offsetZ,
    v2 align, r32 alpha = 1.0f,
    r32 entityZC = 1.0f
)
{
    PushPiece
        (
            group, bitmap,
            offset, offsetZ,
            align, v2{0, 0},
            v4{1.0f, 1.0f, 1.0f, alpha},
            entityZC
        );
}

inline void
PushRect
(
    entity_visible_piece_group *group,
    v2 offset, r32 offsetZ,
    v2 dim,
    v4 color,
    r32 entityZC = 1.0f
)
{
    PushPiece
        (
            group, 0,
            offset, offsetZ,
            v2{0, 0}, dim,
            color,
            entityZC
        );
}

inline entity
EntityFromHighIndex(game_state *gameState, ui32 highIndex)
{
    entity result = {};

    if(highIndex)
    {
        Assert(highIndex < ArrayCount(gameState->_highEntities));
        result.high = gameState->_highEntities + highIndex;
        result.lowIndex = result.high->lowEntityIndex;
        result.low = gameState->lowEntities + result.lowIndex;
    }

    return result;
}

inline void
UpdateFamiliar
(
    game_state *gameState, entity _entity, r32 deltaTime
)
{
    entity closestHero = {};
    r32 closestHeroDSq = Square(10.0f);
    for(ui32 highEntityIndex = 1;
        highEntityIndex < gameState->highEntityCount;
        highEntityIndex++)
    {
        entity testEntity = EntityFromHighIndex
            (
                gameState, highEntityIndex
            );

        if(testEntity.low->type == EntityType_Hero)
        {
            r32 testDSq = LengthSq(testEntity.high->pos - _entity.high->pos) * 0.75f;
            if(closestHeroDSq > testDSq)
            {
                closestHero = testEntity;
                closestHeroDSq = testDSq;
            }
        }
    }

    v2 ddPos = {};
    if(closestHero.high && closestHeroDSq > Square(3.0f))
    {
        v2 deltaPos = closestHero.high->pos - _entity.high->pos;
        ddPos = (0.5f / SqRt(closestHeroDSq)) * deltaPos;
    }
    
    move_spec moveSpec = DefaultMoveSpec();
    moveSpec.isUnitMaxAccelVector = true;
    moveSpec.speed = 50.0f;
    moveSpec.drag = 8.0f;
    MoveEntity(gameState, _entity, deltaTime, &moveSpec, ddPos);
}

inline void
UpdateMonster
(
    game_state *gameState, entity _entity, r32 deltaTime
)
{
}

inline void
UpdateSword
(
    game_state *gameState, entity _entity, r32 deltaTime
)
{
    move_spec moveSpec = DefaultMoveSpec();
    moveSpec.isUnitMaxAccelVector = false;
    moveSpec.speed = 0.0f;
    moveSpec.drag = 0.0f;

    v2 oldPos = _entity.high->pos;
    MoveEntity(gameState, _entity, deltaTime, &moveSpec, v2{0, 0});
    r32 distanceTraveled = Length(_entity.high->pos - oldPos);
    _entity.low->distanceRemaining -= distanceTraveled;
    if(_entity.low->distanceRemaining < 0.0f)
    {
        ChangeEntityLocation
            (
                &gameState->worldArena, gameState->_world,
                _entity.lowIndex, _entity.low,
                &_entity.low->pos, 0
            );
    }
}

internal void
DrawHitPoints
(
    low_entity *lowEntity,
    entity_visible_piece_group *pieceGroup
)
{
    if(lowEntity->hitPointMax >= 1)
    {
        v2 healthDim = {0.2f, 0.2f};
        r32 spacingX = 1.5f * healthDim.x;
        v2 hitPos =
            {
                (lowEntity->hitPointMax - 1) * -0.5f * spacingX,
                -0.25f
            };
        v2 deltaHitPos = {spacingX, 0.0f};
                
        for(ui32 healthIndex = 0;
            healthIndex < lowEntity->hitPointMax;
            healthIndex++)
        {
            hit_point *hitPoint = lowEntity->hitPoint + healthIndex;
            v4 color = {1.0f, 0.0f, 0.0f, 1.0f};
            if(hitPoint->filledAmount == 0)
            {
                color = v4{0.2f, 0.2f, 0.2f, 1.0f};
            }
                        
            PushRect
                (
                    pieceGroup,
                    hitPos, 0, healthDim,
                    color, 0.0f
                );
            hitPos += deltaHitPos;
        }
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == ArrayCount(input->controllers[0].buttons));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);
        
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        // NOTE: Reserve the null entity slot
        AddLowEntity(gameState, EntityType_Null, 0);
        gameState->highEntityCount = 1;
        
        gameState->bmp = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_background.bmp"
            );

        gameState->shadow = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_shadow.bmp"
            );

        gameState->tree = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/tree00.bmp"
            );

        gameState->sword = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/rock03.bmp"
            );
        
        hero_bitmaps *heroBMP = gameState->heroBitmaps;

        heroBMP->head = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_right_head.bmp"
            );
        
        heroBMP->cape = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_right_cape.bmp"
            );
            
        heroBMP->torso = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_right_torso.bmp"
            );

        heroBMP->align = v2{72, 182};

        heroBMP++;

        heroBMP->head = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_back_head.bmp"
            );
        
        heroBMP->cape = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_back_cape.bmp"
            );
            
        heroBMP->torso = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_back_torso.bmp"
            );

        heroBMP->align = v2{72, 182};

        heroBMP++;

        heroBMP->head = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_left_head.bmp"
            );
        
        heroBMP->cape = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_left_cape.bmp"
            );
            
        heroBMP->torso = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_left_torso.bmp"
            );

        heroBMP->align = v2{72, 182};
        
        heroBMP++;

        heroBMP->head = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_front_head.bmp"
            );
        
        heroBMP->cape = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_front_cape.bmp"
            );
            
        heroBMP->torso = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_front_torso.bmp"
            );

        heroBMP->align = v2{72, 182};
        
        heroBMP++;
        
        InitializeArena
            (
                &gameState->worldArena,
                memory->permanentStorageSize - sizeof(game_state),
                (ui8 *)memory->permanentStorage + sizeof(game_state)
            );

        gameState->_world = PushStruct
            (
                &gameState->worldArena,
                world
            );
        world *_world = gameState->_world;
        
        InitializeWorld(_world, 1.4f);
        
        i32 tileSideInPixels = 60;
        gameState->metersToPixels = (r32)tileSideInPixels / (r32)_world->tileSideInMeters;
        
        ui32 randomNumberIndex = 0;
        
        ui32 tilesPerWidth = 17;
        ui32 tilesPerHeight = 9;
        ui32 screenBaseX = 0;
        ui32 screenBaseY = 0;
        ui32 screenBaseZ = 0;
        ui32 screenX = screenBaseX;
        ui32 screenY = screenBaseY;
        ui32 absTileZ = screenBaseZ;

        b32 isDoorTop = false;
        b32 isDoorLeft = false;
        b32 isDoorBottom = false;
        b32 isDoorRight = false;
        b32 isDoorUp = false;
        b32 isDoorDown = false;
        
        for(ui32 screenIndex = 0;
            screenIndex < 2000;
            screenIndex++)
        {
            // TODO: Random number generator
            Assert(randomNumberIndex < ArrayCount(randomNumberTable));
            ui32 randomChoice;

            //if(isDoorUp || isDoorDown)
            {
                randomChoice = randomNumberTable[randomNumberIndex++] % 2;
            }
#if 0
            else
            {
                randomChoice = randomNumberTable[randomNumberIndex++] % 3;
            }
#endif
            
            b32 isCreatedZDoor = false;
            if(randomChoice == 2)
            {
                isCreatedZDoor = true;
                
                if(absTileZ == screenBaseZ)
                {
                    isDoorUp = true;
                }
                else
                {
                    isDoorDown = true;
                }
            }
            else if(randomChoice == 1)
            {
                isDoorRight = true;
            }
            else
            {
                isDoorTop = true;
            }
            
            for(ui32 tileY = 0;
                tileY < tilesPerHeight;
                tileY++)
            {
                for(ui32 tileX = 0;
                    tileX < tilesPerWidth;
                    tileX++)
                {
                    ui32 absTileX = screenX * tilesPerWidth
                        + tileX;
                        
                    ui32 absTileY = screenY * tilesPerHeight
                        + tileY;

                    ui32 tileValue = 1;
                    if(tileX == 0 &&
                       (!isDoorLeft || tileY != tilesPerHeight / 2))
                    {
                        tileValue = 2;
                    }

                    if(tileX == tilesPerWidth - 1 &&
                       (!isDoorRight || tileY != tilesPerHeight / 2))
                    {
                        tileValue = 2;
                    }

                    if(tileY == 0 &&
                       (!isDoorBottom || tileX != tilesPerWidth / 2))
                    {
                        tileValue = 2;
                    }

                    if(tileY == tilesPerHeight - 1 &&
                       (!isDoorTop || tileX != tilesPerWidth / 2))
                    {
                        tileValue = 2;
                    }

                    if(tileX == 10 && tileY == 6)
                    {
                        if(isDoorUp)
                        {
                            tileValue = 3;
                        }
                        else if(isDoorDown)
                        {
                            tileValue = 4;
                        }
                    }
                    
                    if(tileValue == 2)
                    {
                        AddWall(gameState, absTileX, absTileY, absTileZ);
                    }
                }
            }

            isDoorLeft = isDoorRight;
            isDoorBottom = isDoorTop;

            if(isCreatedZDoor)
            {
                isDoorUp = !isDoorUp;
                isDoorDown = !isDoorDown;
            }
            else
            {
                isDoorUp = false;
                isDoorDown = false;
            }
            
            isDoorTop = false;
            isDoorRight = false;

            if(randomChoice == 2)
            {
                if(absTileZ == screenBaseZ)
                {
                    absTileZ = screenBaseZ + 1;
                }
                else
                {
                    absTileZ = screenBaseZ;
                }
            }
            else if(randomChoice == 1)
            {
                screenX++;
            }
            else
            {
                screenY++;
            }
        }
        
        world_position newCameraPos = {};
        ui32 cameraTileX = screenBaseX * tilesPerWidth + 17/2;
        ui32 cameraTileY = screenBaseY * tilesPerHeight + 9/2;
        ui32 cameraTileZ = screenBaseZ;
        newCameraPos = ChunkPosFromTilePos
            (
                gameState->_world,
                cameraTileX, cameraTileY, cameraTileZ
            );

        AddMonster
            (
                gameState,
                cameraTileX + 2, cameraTileY + 2, cameraTileZ
            );

        for(i32 familiarIndex = 0;
            familiarIndex < 1;
            familiarIndex++)
        {
            i32 familiarOffsetX = (randomNumberTable[randomNumberIndex++] % 10) - 7;
            i32 familiarOffsetY = (randomNumberTable[randomNumberIndex++] % 10) - 3;
            if((familiarOffsetX != 0) ||
               (familiarOffsetY != 0))
            {
                AddFamiliar
                    (
                        gameState,
                        cameraTileX + familiarOffsetX,
                        cameraTileY + familiarOffsetY,
                        cameraTileZ
                    );
            }
        }
        
        SetCamera(gameState, newCameraPos);
        
        memory->isInitialized = true;
    }
    
    world *_world = gameState->_world;
    
    r32 metersToPixels = gameState->metersToPixels;
    
    for(i32 controllerIndex = 0;
        controllerIndex < ArrayCount(input->controllers);
        controllerIndex++)
    {
        game_controller_input *controller = GetController(input, controllerIndex);
        ui32 lowIndex = gameState->playerIndexForController[controllerIndex];
        if(!lowIndex)
        {
            if(controller->start.endedDown)
            {
                ui32 entityIndex = AddPlayer(gameState).lowIndex;
                gameState->playerIndexForController[controllerIndex] = entityIndex;
            }
        }
        else
        {
            entity controllingEntity = ForceEntityIntoHigh(gameState, lowIndex);
            v2 ddPos = {};
            
            if(controller->isAnalog)
            {
                // NOTE: Analog movement
                ddPos = v2
                    {
                        controller->stickAverageX,
                        controller->stickAverageY
                    };
            }
            else
            {
                // NOTE: Digital movement
                if(controller->moveUp.endedDown)
                {        
                    ddPos.y = 1.0f;
                }
            
                if(controller->moveDown.endedDown)
                {
                    ddPos.y = -1.0f;
                }
            
                if(controller->moveLeft.endedDown)
                {
                    ddPos.x = -1.0f;
                }
            
                if(controller->moveRight.endedDown)
                {
                    ddPos.x = 1.0f;
                }
            }

            if(controller->start.endedDown)
            {
                controllingEntity.high->dZ = 3.0f;
            }

            v2 dSword = {};
            if(controller->actionUp.endedDown)
            {
                dSword = v2{0.0f, 1.0f};
            }

            if(controller->actionDown.endedDown)
            {
                dSword = v2{0.0f, -1.0f};
            }

            if(controller->actionLeft.endedDown)
            {
                dSword = v2{-1.0f, 0.0f};
            }

            if(controller->actionRight.endedDown)
            {
                dSword = v2{1.0f, 0.0f};
            }

            move_spec moveSpec = DefaultMoveSpec();
            moveSpec.isUnitMaxAccelVector = false;
            moveSpec.speed = 50.0f;
            moveSpec.drag = 8.0f;
            MoveEntity
                (
                    gameState,
                    controllingEntity, input->deltaTime,
                    &moveSpec, ddPos
                );

            if(dSword.x != 0 || dSword.y != 0)
            {
                low_entity *lowSword = GetLowEntity
                    (
                        gameState,
                        controllingEntity.low->swordIndex
                    );
                if(lowSword && !IsValid(lowSword->pos))
                {
                    world_position swordPos = controllingEntity.low->pos;
                    ChangeEntityLocation
                        (
                            &gameState->worldArena,
                            gameState->_world,
                            controllingEntity.low->swordIndex,
                            lowSword,
                            0, &swordPos
                        );

                    entity sword = ForceEntityIntoHigh
                        (
                            gameState,
                            controllingEntity.low->swordIndex
                        );
                    sword.low->distanceRemaining = 5.0f;
                    sword.high->dPos = 5.0f * dSword;
                }
            }
        }
    }

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
    
    //
    // NOTE: Render
    //
    DrawRectangle
        (
            buffer,
            v2{0.0f, 0.0f},
            v2{(r32)buffer->width, (r32)buffer->height},
            0.5f, 0.5f, 0.5f
        );
    
    r32 screenCenterX = 0.5f * (r32)buffer->width;
    r32 screenCenterY = 0.5f * (r32)buffer->height;

    entity_visible_piece_group pieceGroup = {};
    pieceGroup.gameState = gameState;
    for(ui32 highEntityIndex = 1;
        highEntityIndex < gameState->highEntityCount;
        highEntityIndex++)
    {
        pieceGroup.pieceCount = 0;
        
        high_entity *highEntity = gameState->_highEntities + highEntityIndex;
        low_entity *lowEntity = gameState->lowEntities + highEntity->lowEntityIndex;

        entity _entity = {};
        _entity.lowIndex = highEntity->lowEntityIndex;
        _entity.low = lowEntity;
        _entity.high = highEntity;
        
        r32 deltaTime = input->deltaTime;
        // TODO: Should be computed after update
        r32 shadowAlpha = 1.0f - 0.5f * highEntity->z;
        if(shadowAlpha < 0.0f)
        {
            shadowAlpha = 0.0f;
        }
        
        hero_bitmaps *heroBitmaps = &gameState->
            heroBitmaps[highEntity->facingDirection];
        switch(lowEntity->type)
        {
            case EntityType_Hero:
            {
                PushBitmap
                    (
                        &pieceGroup, &gameState->shadow,
                        v2{0, 0}, 0,
                        heroBitmaps->align,
                        shadowAlpha, 0.0f
                    );
            
                PushBitmap
                    (
                        &pieceGroup, &heroBitmaps->torso,
                        v2{0, 0}, 0,
                        heroBitmaps->align
                    );

                PushBitmap
                    (
                        &pieceGroup, &heroBitmaps->cape,
                        v2{0, 0}, 0,
                        heroBitmaps->align
                    );
    
                PushBitmap
                    (
                        &pieceGroup, &heroBitmaps->head,
                        v2{0, 0}, 0,
                        heroBitmaps->align
                    );

                DrawHitPoints(lowEntity, &pieceGroup);
            } break;
            
            case EntityType_Wall:
            {
                PushBitmap
                    (
                        &pieceGroup, &gameState->tree,
                        v2{0, 0}, 0,
                        v2{40, 80}
                    );
            } break;

            case EntityType_Sword:
            {
                UpdateSword(gameState, _entity, deltaTime);
                PushBitmap
                    (
                        &pieceGroup, &gameState->shadow,
                        v2{0, 0}, 0,
                        heroBitmaps->align,
                        shadowAlpha, 0.0f
                    );
                PushBitmap
                    (
                        &pieceGroup, &gameState->sword,
                        v2{0, 0}, 0,
                        v2{29, 10}
                    );
            } break;
            
            case EntityType_Familiar:
            {
                UpdateFamiliar(gameState, _entity, deltaTime);
                _entity.high->tBob += deltaTime;
                if(_entity.high->tBob > 2.0f * Pi32)
                {
                    _entity.high->tBob -= 2.0f * Pi32;
                }
                r32 bobSin = Sin(2.0f * _entity.high->tBob);
                
                PushBitmap
                    (
                        &pieceGroup, &gameState->shadow,
                        v2{0, 0}, 0,
                        heroBitmaps->align,
                        0.5f * shadowAlpha + 0.2f * bobSin, 0.0f
                    );
                PushBitmap
                    (
                        &pieceGroup, &heroBitmaps->head,
                        v2{0, 0},
                        0.25f * bobSin,
                        heroBitmaps->align
                    );
            } break;

            case EntityType_Monster:
            {
                UpdateMonster(gameState, _entity, deltaTime);
                PushBitmap
                    (
                        &pieceGroup, &gameState->shadow,
                        v2{0, 0}, 0,
                        heroBitmaps->align,
                        shadowAlpha, 0.0f
                    );
                PushBitmap
                    (
                        &pieceGroup, &heroBitmaps->torso,
                        v2{0, 0}, 0,
                        heroBitmaps->align
                    );
                
                DrawHitPoints(lowEntity, &pieceGroup);
            } break;

            default:
            {
                InvalidCodePath;
            } break;
        }
        
        r32 ddZ = -9.8f;
        highEntity->z = 0.5f*ddZ*Square(deltaTime) + highEntity->dZ * deltaTime + highEntity->z;
        highEntity->dZ = ddZ * deltaTime + highEntity->dZ;
        if(highEntity->z < 0)
        {
            highEntity->z = 0;
        }
        
        r32 entityGroundX = screenCenterX + metersToPixels *
            highEntity->pos.x;
        r32 entityGroundY = screenCenterY - metersToPixels *
            highEntity->pos.y;

        r32 entityZ = -metersToPixels*highEntity->z;
        
        for(ui32 pieceIndex = 0;
            pieceIndex < pieceGroup.pieceCount;
            pieceIndex++)
        {
            entity_visible_piece *piece = pieceGroup.pieces + pieceIndex;
            v2 center =
                {
                    entityGroundX + piece->offset.x,
                    entityGroundY + piece->offset.y + piece->offsetZ + piece->entityZC * entityZ
                };
            if(piece->bitmap)
            {
                DrawBitmap
                    (
                        buffer, piece->bitmap,
                        center.x, center.y,
                        piece->a
                    );
            }
            else
            {
                v2 halfDim = 0.5f * metersToPixels * piece->dim;
                DrawRectangle
                    (
                        buffer,
                        center - halfDim, center + halfDim,
                        piece->r, piece->g, piece->b
                    );
            }
        }
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
