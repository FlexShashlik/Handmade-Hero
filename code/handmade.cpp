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
    i32 alignX = 0, i32 alignY = 0,
    r32 cAlpha = 1.0f
)
{
    rX -= (r32)alignX;
    rY -= (r32)alignY;
    
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

inline high_entity *
MakeEntityHighFrequency(game_state *gameState, ui32 lowIndex)
{
    high_entity *highEntity = 0;
    low_entity *lowEntity = &gameState->lowEntities[lowIndex];
    if(lowEntity->highEntityIndex)
    {
        highEntity = gameState->_highEntities + lowEntity->highEntityIndex;
    }
    else
    {
        if(gameState->highEntityCount < ArrayCount(gameState->_highEntities))
        {
            ui32 highIndex = gameState->highEntityCount++;
            highEntity = gameState->_highEntities + highIndex;
    
            // NOTE: Map the entity into camera space
            world_difference diff = Subtract
                (
                    gameState->worldMap,
                    &lowEntity->pos, &gameState->cameraPos
                );

            highEntity->pos = diff.dXY;
            highEntity->dPos = v2{0, 0};
            highEntity->absTileZ = lowEntity->pos.absTileZ;
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

inline entity
GetHighEntity(game_state *gameState, ui32 lowIndex)
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

inline void
OffsetAndCheckFrequencyByArea
(
    game_state *gameState, v2 offset, rectangle2 bounds
)
{
    for(ui32 entityIndex = 1;
        entityIndex < gameState->highEntityCount;
        )
    {
        high_entity *high = gameState->_highEntities + entityIndex;

        high->pos += offset;
        if(IsInRectangle(bounds, high->pos))
        {
            entityIndex++;
        }
        else
        {
            MakeEntityLowFrequency(gameState, high->lowEntityIndex);
        }
    }
}

internal ui32
AddLowEntity(game_state *gameState, entity_type type)
{
    Assert(gameState->lowEntityCount < ArrayCount(gameState->lowEntities));
    ui32 entityIndex = gameState->lowEntityCount++;
    
    gameState->lowEntities[entityIndex] = {};
    gameState->lowEntities[entityIndex].type = type;

    return entityIndex;
}

internal ui32
AddPlayer(game_state *gameState)
{
    ui32 entityIndex = AddLowEntity(gameState, EntityType_Hero);
    low_entity *lowEntity = GetLowEntity
        (
            gameState, entityIndex
        );
    
    lowEntity->pos = gameState->cameraPos;
    lowEntity->height = 0.5f;
    lowEntity->width = 1.0f;
    lowEntity->isCollides = true;

    if(gameState->cameraFollowingEntityIndex == 0)
    {
        gameState->cameraFollowingEntityIndex = entityIndex;
    }

    return entityIndex;
}

internal ui32
AddWall
(
    game_state *gameState,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
)
{
    ui32 entityIndex = AddLowEntity(gameState, EntityType_Wall);
    low_entity *lowEntity = GetLowEntity
        (
            gameState, entityIndex
        );
    
    lowEntity->pos.absTileX = absTileX;
    lowEntity->pos.absTileY = absTileY;
    lowEntity->pos.absTileZ = absTileZ;
    lowEntity->height = gameState->worldMap->tileSideInMeters;
    lowEntity->width = lowEntity->height;
    lowEntity->isCollides = true;

    return entityIndex;
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
MovePlayer
(
    game_state *gameState, entity _entity,
    r32 deltaTime, v2 ddPlayerPos
)
{
    world *worldMap = gameState->worldMap;
    r32 ddPLength = LengthSq(ddPlayerPos);
    if(ddPLength > 1.0f)
    {
        ddPlayerPos *= 1.0f / SqRt(ddPLength);
    }
    
    r32 playerSpeed = 50.0f;
    ddPlayerPos *= playerSpeed;

    // TODO: ODE!!!
    ddPlayerPos += -8.0f * _entity.high->dPos;

    v2 oldPlayerPos = _entity.high->pos;
    v2 deltaPlayerPos = 0.5f * ddPlayerPos *
        Square(deltaTime) + _entity.high->dPos * deltaTime;
    _entity.high->dPos = ddPlayerPos * deltaTime +
        _entity.high->dPos;
    
    v2 newPlayerPos = oldPlayerPos + deltaPlayerPos;

    /*
    ui32 minTileX = Minimum(oldPlayerPos.absTileX, newPlayerPos.absTileX);
    ui32 minTileY = Minimum(oldPlayerPos.absTileY, newPlayerPos.absTileY);
    ui32 maxTileX = Maximum(oldPlayerPos.absTileX, newPlayerPos.absTileX);
    ui32 maxTileY = Maximum(oldPlayerPos.absTileY, newPlayerPos.absTileY);

    ui32 entityTileWidth = CeilR32ToI32(_entity.high->width / tileMap->tileSideInMeters);
    ui32 entityTileHeight = CeilR32ToI32(_entity.high->height / tileMap->tileSideInMeters);
    
    minTileX -= entityTileWidth;
    minTileY -= entityTileHeight;
    maxTileX += entityTileWidth;
    maxTileY += entityTileHeight;
    
    ui32 absTileZ = _entity.high->pos.absTileZ;
    */
    
    for(ui32 i = 0; i < 4; i++)
    {
        r32 tMin = 1.0f;
        v2 wallNormal = {};
        ui32 hitHighEntityIndex = 0;

        v2 desiredPos = _entity.high->pos + deltaPlayerPos;
        
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
            
            _entity.high->absTileZ += hitLow->deltaAbsTileZ;
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

    _entity.low->pos = MapIntoTileSpace
        (
            gameState->worldMap,
            gameState->cameraPos, _entity.high->pos
        );
}

internal void
SetCamera
(
    game_state *gameState,
    world_position newCameraPos
)
{
    world *worldMap = gameState->worldMap;
    world_difference deltaCameraPos = Subtract
        (
            worldMap, &newCameraPos, &gameState->cameraPos
        );
    gameState->cameraPos = newCameraPos;

    ui32 tileSpanX = 17 * 3;
    ui32 tileSpanY = 9 * 3;
    rectangle2 cameraBounds = RectCenterDim
        (
            v2{0, 0},
            worldMap->tileSideInMeters * v2{(r32)tileSpanX, (r32)tileSpanY}
         );

    v2 entityOffsetForFrame = -deltaCameraPos.dXY;
    OffsetAndCheckFrequencyByArea
        (
            gameState,
            entityOffsetForFrame, cameraBounds
        );
    
    i32 minTileX = newCameraPos.absTileX - tileSpanX / 2;
    i32 minTileY = newCameraPos.absTileY - tileSpanY / 2;
    i32 maxTileX = newCameraPos.absTileX + tileSpanX / 2;
    i32 maxTileY = newCameraPos.absTileY + tileSpanY / 2;
    
    for(ui32 entityIndex = 1;
        entityIndex < gameState->lowEntityCount;
        entityIndex++)
    {
        low_entity *low = gameState->lowEntities + entityIndex;
        if(!low->highEntityIndex)
        {
            if(low->pos.absTileZ == newCameraPos.absTileZ &&
               low->pos.absTileX >= minTileX &&
               low->pos.absTileX <= maxTileX &&
               low->pos.absTileY >= minTileY &&
               low->pos.absTileY <= maxTileY)
            {
                MakeEntityHighFrequency(gameState, entityIndex);
            }
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
        AddLowEntity(gameState, EntityType_Null);
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

        heroBMP->alignX = 72;
        heroBMP->alignY = 182;

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

        heroBMP->alignX = 72;
        heroBMP->alignY = 182;

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

        heroBMP->alignX = 72;
        heroBMP->alignY = 182;

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

        heroBMP->alignX = 72;
        heroBMP->alignY = 182;

        heroBMP++;
        
        InitializeArena
            (
                &gameState->worldArena,
                memory->permanentStorageSize - sizeof(game_state),
                (ui8 *)memory->permanentStorage + sizeof(game_state)
            );

        gameState->worldMap = PushStruct
            (
                &gameState->worldArena,
                world
            );
        world *worldMap = gameState->worldMap;
        
        InitializeTileMap(worldMap, 1.4f);

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
            screenIndex < 2;
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

#if 0
        while(gameState->lowEntityCount < ArrayCount(gameState->lowEntities) - 16)
        {
            ui32 coord = 1024 + gameState->lowEntityCount;
            AddWall(gameState, coord, coord, coord);
        }
#endif
        
        world_position newCameraPos = {};
        newCameraPos.absTileX = screenBaseX * tilesPerWidth + 17/2;
        newCameraPos.absTileY = screenBaseY * tilesPerHeight + 9/2;
        newCameraPos.absTileZ = screenBaseZ;
        SetCamera(gameState, newCameraPos);
        
        memory->isInitialized = true;
    }
    
    world *worldMap = gameState->worldMap;
    
    i32 tileSideInPixels = 60;
    r32 metersToPixels = (r32)tileSideInPixels / (r32)worldMap->tileSideInMeters;
    
    r32 lowerLeftX = -(r32)tileSideInPixels/2;
    r32 lowerLeftY = (r32)buffer->height;
    
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
                ui32 entityIndex = AddPlayer(gameState);
                gameState->playerIndexForController[controllerIndex] = entityIndex;
            }
        }
        else
        {
            entity controllingEntity = GetHighEntity(gameState, lowIndex);
            v2 ddPlayerPos = {};
            
            if(controller->isAnalog)
            {
                // NOTE: Analog movement
                ddPlayerPos = v2
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
                    ddPlayerPos.y = 1.0f;
                }
            
                if(controller->moveDown.endedDown)
                {
                    ddPlayerPos.y = -1.0f;
                }
            
                if(controller->moveLeft.endedDown)
                {
                    ddPlayerPos.x = -1.0f;
                }
            
                if(controller->moveRight.endedDown)
                {
                    ddPlayerPos.x = 1.0f;
                }
            }

            if(controller->actionUp.endedDown)
            {
                controllingEntity.high->dZ = 3.0f;
            }

            MovePlayer
                (
                    gameState, controllingEntity,
                    input->deltaTime, ddPlayerPos
                );
        }
    }

    entity cameraFollowingEntity = GetHighEntity
        (
            gameState,
            gameState->cameraFollowingEntityIndex
        );
    
    if(cameraFollowingEntity.high)
    {
        world_position newCameraPos = gameState->cameraPos;
        gameState->cameraPos.absTileZ = cameraFollowingEntity.low->pos.absTileZ;

#if 0
        if(cameraFollowingEntity.high->pos.x >
           9.0f * worldMap->tileSideInMeters)
        {
            newCameraPos.absTileX += 17;
        }

        if(cameraFollowingEntity.high->pos.x <
           -9.0f * worldMap->tileSideInMeters)
        {
            newCameraPos.absTileX -= 17;
        }

        if(cameraFollowingEntity.high->pos.y >
           5.0f * worldMap->tileSideInMeters)
        {
            newCameraPos.absTileY += 9;
        }

        if(cameraFollowingEntity.high->pos.y <
           -5.0f * worldMap->tileSideInMeters)
        {
           newCameraPos.absTileY -= 9;
        }
#else
        newCameraPos = cameraFollowingEntity.low->pos;
        if(cameraFollowingEntity.high->pos.x >
           1.0f * worldMap->tileSideInMeters)
        {
            newCameraPos.absTileX += 1;
        }

        if(cameraFollowingEntity.high->pos.x <
           -1.0f * worldMap->tileSideInMeters)
        {
            newCameraPos.absTileX -= 1;
        }

        if(cameraFollowingEntity.high->pos.y >
           1.0f * worldMap->tileSideInMeters)
        {
            newCameraPos.absTileY += 1;
        }

        if(cameraFollowingEntity.high->pos.y <
           -1.0f * worldMap->tileSideInMeters)
        {
           newCameraPos.absTileY -= 1;
        }
#endif

        SetCamera(gameState, newCameraPos);
    }
    
    //
    // NOTE: Render
    //
    DrawBitmap(buffer, &gameState->bmp, 0, 0);
    
    r32 screenCenterX = 0.5f * (r32)buffer->width;
    r32 screenCenterY = 0.5f * (r32)buffer->height;

#if 0
    for(i32 relRow = -10;
        relRow < 10;
        relRow++)
    {
        for(i32 relColumn = -20;
            relColumn < 20;
            relColumn++)
        {
            ui32 column = gameState->cameraPos.absTileX + relColumn;
            ui32 row = gameState->cameraPos.absTileY + relRow;
            
            ui32 tileID = GetTileValue
                (
                    worldMap,
                    column, row, gameState->cameraPos.absTileZ
                );
            
            if(tileID > 1)
            {
                r32 gray = (tileID == 2) ? 1.0f : 0.5f;

                if(tileID > 2) gray = 0.25f;
                
                if(row == gameState->cameraPos.absTileY &&
                   column == gameState->cameraPos.absTileX)
                {
                    gray = 0.0f;
                }

                v2 tileSide =
                    {
                        0.5f * tileSideInPixels,
                        0.5f * tileSideInPixels
                    };
                
                v2 center =
                    {
                        screenCenterX - metersToPixels *
                        gameState->cameraPos._offset.x +
                        ((r32)relColumn) * tileSideInPixels,
                        
                        screenCenterY + metersToPixels *
                        gameState->cameraPos._offset.y -
                        ((r32)relRow) * tileSideInPixels
                    };
                
                v2 min = center - 0.9f * tileSide;
                v2 max = center + 0.9f * tileSide;
            
                DrawRectangle
                    (
                        buffer, min, max,
                        gray, gray, gray
                    );
            }
        }
    }
#endif
    
    for(ui32 highEntityIndex = 0;
        highEntityIndex < gameState->highEntityCount;
        highEntityIndex++)
    {
        high_entity *highEntity = gameState->_highEntities + highEntityIndex;
        low_entity *lowEntity = gameState->lowEntities + highEntity->lowEntityIndex;
            
        r32 deltaTime = input->deltaTime;
        r32 ddZ = -9.8f;
        highEntity->z = 0.5f*ddZ*Square(deltaTime) + highEntity->dZ * deltaTime + highEntity->z;
        highEntity->dZ = ddZ * deltaTime + highEntity->dZ;
        if(highEntity->z < 0)
        {
            highEntity->z = 0;
        }
        r32 cAlpha = 1.0f - 0.5f * highEntity->z;
        if(cAlpha < 0.0f)
        {
            cAlpha = 0.0f;
        }
            
        r32 playerR = 1.0f;
        r32 playerG = 1.0f;
        r32 playerB = 0.0f;

        r32 playerGroundX = screenCenterX + metersToPixels *
            highEntity->pos.x;
        r32 playerGroundY = screenCenterY - metersToPixels *
            highEntity->pos.y;

        r32 z = -metersToPixels*highEntity->z;
    
        v2 playerLeftTop =
            {
                playerGroundX - metersToPixels * 0.5f *  lowEntity->width,
                playerGroundY - metersToPixels * 0.5f *  lowEntity->height
            };

        v2 entityWidthHeight = {lowEntity->width, lowEntity->height};

        if(lowEntity->type == EntityType_Hero)
        {
            hero_bitmaps *heroBitmaps = &gameState->
                heroBitmaps[highEntity->facingDirection];

            DrawBitmap
                (
                    buffer, &gameState->shadow,
                    playerGroundX, playerGroundY,
                    heroBitmaps->alignX,
                    heroBitmaps->alignY,
                    cAlpha
                );
            
            DrawBitmap
                (
                    buffer, &heroBitmaps->torso,
                    playerGroundX, playerGroundY + z,
                    heroBitmaps->alignX,
                    heroBitmaps->alignY
                );

            DrawBitmap
                (
                    buffer, &heroBitmaps->cape,
                    playerGroundX, playerGroundY + z,
                    heroBitmaps->alignX,
                    heroBitmaps->alignY
                );
    
            DrawBitmap
                (
                    buffer, &heroBitmaps->head,
                    playerGroundX, playerGroundY + z,
                    heroBitmaps->alignX,
                    heroBitmaps->alignY
                );
        }
        else
        {
            DrawRectangle
                (
                    buffer,
                    playerLeftTop,
                    playerLeftTop + metersToPixels * entityWidthHeight,
                    playerR, playerG, playerB
                );
        }
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
