#include "handmade.h"
#include "handmade_tile.cpp"
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
RenderWeirdGradient(game_offscreen_buffer *buffer, i32 blueOffset, i32 greenOffset)
{
   ui8 *row = (ui8 *)buffer->memory;

    for(int y = 0; y < buffer->height; y++)
    {
        ui32 *pixel = (ui32 *)row;
        
        for(int x = 0; x < buffer->width; x++)
        {
            ui8 blue = (ui8)(x + blueOffset);
            ui8 green = (ui8)(y + greenOffset);
            
            *pixel++ = ((green << 8) | blue);
        }

        row += buffer->pitch;
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
    i32 alignX = 0, i32 alignY = 0
)
{
    rX -= (r32)alignX;
    rY -= (r32)alignY;
    
    i32 minX = RoundR32ToI32(rX);
    i32 minY = RoundR32ToI32(rY);
    i32 maxX = RoundR32ToI32(rX + (r32)bmp->width);
    i32 maxY = RoundR32ToI32(rY + (r32)bmp->height);

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
                *sourceDest = (RotateLeft(c & redMask, redShift) |
                               RotateLeft(c & greenMask, greenShift) |
                               RotateLeft(c & blueMask, blueShift) |
                               RotateLeft(c & alphaMask, alphaShift));
            }
        }
    }

    return result;
}

inline entity *
GetEntity(game_state *gameState, ui32 index)
{
    entity *_entity = 0;

    if(index > 0 && index < ArrayCount(gameState->entities))
    {
        _entity = &gameState->entities[index];
    }

    return _entity;
}

internal void
InitializePlayer(game_state *gameState, ui32 entityIndex)
{
    entity *_entity = GetEntity(gameState, entityIndex);
    
    _entity->isExists = true;
    _entity->pos.absTileX = 1;
    _entity->pos.absTileY = 3;
    _entity->pos.offset.x = 5.0f;
    _entity->pos.offset.y = 5.0f;
    _entity->height = 1.4f;
    _entity->width = 0.75f * _entity->height;

    if(!GetEntity(gameState,
                  gameState->cameraFollowingEntityIndex))
    {
        gameState->cameraFollowingEntityIndex = entityIndex;
    }
}

internal ui32
AddEntity(game_state *gameState)
{
    Assert(gameState->entityCount < ArrayCount(gameState->entities));
    ui32 entityIndex = gameState->entityCount++;
    entity *_entity = &gameState->entities[entityIndex];
    *_entity = {};

    
    return entityIndex;
}

internal void
MovePlayer
(
    game_state *gameState, entity *_entity,
    r32 deltaTime, v2 ddPlayerPos
)
{
    tile_map *tileMap = gameState->worldMap->tileMap;
    if(ddPlayerPos.x != 0.0f && ddPlayerPos.y != 0.0f)
    {
        ddPlayerPos *= 0.707106781f;
    }

    r32 playerSpeed = 50.0f;
    ddPlayerPos *= playerSpeed;

    // TODO: ODE!!!
    ddPlayerPos += -8.0f * _entity->dPos;

    tile_map_position oldPlayerPos = _entity->pos;
    tile_map_position newPlayerPos = oldPlayerPos;
    
    v2 deltaPlayerPos = 0.5f * ddPlayerPos *
        Square(deltaTime) + _entity->dPos * deltaTime;
    
    newPlayerPos.offset += deltaPlayerPos;
                        
    _entity->dPos = ddPlayerPos * deltaTime +
        _entity->dPos;
    
    newPlayerPos = RecanonicalizePosition(tileMap, newPlayerPos);

#if 1
            
    tile_map_position playerLeft = newPlayerPos;
    playerLeft.offset.x -= 0.5f * _entity->width;
    playerLeft = RecanonicalizePosition(tileMap, playerLeft);
            
    tile_map_position playerRight = newPlayerPos;
    playerRight.offset.x += 0.5f * _entity->width;
    playerRight = RecanonicalizePosition(tileMap, playerRight);

    b32 isCollided = false;
    tile_map_position collisionPos = {};
    if(!IsTileMapPointEmpty(tileMap, newPlayerPos))
    {
        collisionPos = newPlayerPos;
        isCollided = true;
    }

    if(!IsTileMapPointEmpty(tileMap, playerLeft))
    {
        collisionPos = playerLeft;
        isCollided = true;
    }

    if(!IsTileMapPointEmpty(tileMap, playerRight))
    {
        collisionPos = playerRight;
        isCollided = true;
    }
                        
    if(isCollided)
    {
        v2 r = {};
        if(collisionPos.absTileX < _entity->pos.absTileX)
        {
            r = v2{1, 0};
        }
        if(collisionPos.absTileX > _entity->pos.absTileX)
        {
            r = v2{-1, 0};
        }
        if(collisionPos.absTileY < _entity->pos.absTileY)
        {
            r = v2{0, 1};
        }
        if(collisionPos.absTileY > _entity->pos.absTileY)
        {
            r = v2{0, -1};
        }
                
        _entity->dPos = _entity->dPos - 1 * Inner(_entity->dPos, r) * r;
    }
    else
    {
        _entity->pos = newPlayerPos;
    }
#else

    ui32 minTileX = 0;
    ui32 minTileY = 0;
    ui32 onePastMaxTileX = 0;
    ui32 onePastMaxTileY = 0;
    ui32 absTileZ = gameState->playerPos.absTileZ;
    tile_map_position bestPlayerPos = gameState->playerPos;
    r32 bestDistanceSq = LenghtSq(deltaPlayerPos);
            
    for(ui32 absTileY = minTileY;
        absTileY != onePastMaxTileY;
        absTileY++)
    {
        for(ui32 absTileX = minTileX;
            absTileX != onePastMaxTileX;
            absTileX++)
        {
            tile_map_position testTilePos = CenteredTilePoint
                (
                    absTileX, absTileY, absTileZ
                );
                    
            ui32 tileValue = GetTileValue(tileMap, testTilePos);
                    
            if(IsTileValueEmpty(tileValue))
            {
                v2 minCorner = -0.5f * v2
                    {
                        tileMap->tileSideInMeters,
                        tileMap->tileSideInMeters
                    };
                        
                v2 maxCorner = 0.5f * v2
                    {
                        tileMap->tileSideInMeters,
                        tileMap->tileSideInMeters
                    };

                tile_map_difference relNewPlayerPos = Subtract
                    (
                        tileMap,
                        &testTilePos, &newPlayerPos
                    );

                v2 testP = ClosestPointInRect
                    (
                        minCorner,
                        maxCorner,
                        relNewPlayerPos
                    );

                testDistanceSq = ;
                if(bestDistanceSq > testDistanceSq)
                {
                    bestPlayerPos = ;
                    bestDistanceSq = ;
                }
            }
        }
    }
#endif

    //
    // NOTE: Update camera & player Z based on the last movement
    //
    if(!AreOnSameTile(&oldPlayerPos, &_entity->pos))
    {
        ui32 newTileValue = GetTileValue(tileMap, _entity->pos);
                    
        if(newTileValue == 3)
        {
            _entity->pos.absTileZ++;
        }
        else if(newTileValue == 4)
        {
            _entity->pos.absTileZ--;
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

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == ArrayCount(input->controllers[0].buttons));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);
        
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        // NOTE: Reserve the null entity slot
        AddEntity(gameState);
        gameState->bmp = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_background.bmp"
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
        heroBMP->alignY = 155;

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

        heroBMP->alignX = 74;
        heroBMP->alignY = 150;

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

        heroBMP->alignX = 70;
        heroBMP->alignY = 157;

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
        heroBMP->alignY = 150;

        heroBMP++;

        gameState->cameraPos.absTileX = 17/2;
        gameState->cameraPos.absTileY = 9/2;
        
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
        worldMap->tileMap = PushStruct
            (
                &gameState->worldArena,
                tile_map
            );

        tile_map *tileMap = worldMap->tileMap;

        tileMap->chunkShift = 4;
        tileMap->chunkMask = (1 << tileMap->chunkShift) - 1;
        tileMap->chunkDim = 1 << tileMap->chunkShift;
        
        tileMap->tileChunkCountX = 128;
        tileMap->tileChunkCountY = 128;
        tileMap->tileChunkCountZ = 2;

        tileMap->tileChunks = PushArray
            (
                &gameState->worldArena,
                tileMap->tileChunkCountX *
                tileMap->tileChunkCountY *
                tileMap->tileChunkCountZ,
                tile_chunk
            );
    
        tileMap->tileSideInMeters = 1.4f;

        ui32 randomNumberIndex = 0;
        
        ui32 tilesPerWidth = 17;
        ui32 tilesPerHeight = 9;

        ui32 screenX = 0;
        ui32 screenY = 0;
        
        ui32 absTileZ = 0;

        b32 isDoorTop = false;
        b32 isDoorLeft = false;
        b32 isDoorBottom = false;
        b32 isDoorRight = false;
        b32 isDoorUp = false;
        b32 isDoorDown = false;
        
        for(ui32 screenIndex = 0;
            screenIndex < 100;
            screenIndex++)
        {
            // TODO: Random number generator
            Assert(randomNumberIndex < ArrayCount(randomNumberTable));
            ui32 randomChoice;

            if(isDoorUp || isDoorDown)
            {
                randomChoice = randomNumberTable[randomNumberIndex++] % 2;
            }
            else
            {
                randomChoice = randomNumberTable[randomNumberIndex++] % 3;
            }

            b32 isCreatedZDoor = false;
            if(randomChoice == 2)
            {
                isCreatedZDoor = true;
                
                if(absTileZ == 0)
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
                        
                    SetTileValue
                        (
                            &gameState->worldArena, tileMap,
                            absTileX, absTileY, absTileZ,
                            tileValue
                        );
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
                if(absTileZ == 0)
                {
                    absTileZ = 1;
                }
                else
                {
                    absTileZ = 0;
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
        
        memory->isInitialized = true;
    }
        
    world *worldMap = gameState->worldMap;
    tile_map *tileMap = worldMap->tileMap;
    
    i32 tileSideInPixels = 60;
    r32 metersToPixels = (r32)tileSideInPixels / (r32)tileMap->tileSideInMeters;
    
    r32 lowerLeftX = -(r32)tileSideInPixels/2;
    r32 lowerLeftY = (r32)buffer->height;
    
    for(i32 controllerIndex = 0;
        controllerIndex < ArrayCount(input->controllers);
        controllerIndex++)
    {
        game_controller_input *controller = GetController(input, controllerIndex);
        entity *controllingEntity = GetEntity
            (
                gameState,
                gameState->playerIndexForController[controllerIndex]
            );
        
        if(controllingEntity)
        {
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

            MovePlayer
                (
                    gameState, controllingEntity,
                    input->deltaTime, ddPlayerPos
                );
        }
        else
        {
            if(controller->start.endedDown)
            {
                ui32 entityIndex = AddEntity(gameState);
                controllingEntity = GetEntity(gameState, entityIndex);
                InitializePlayer(gameState, entityIndex);
                gameState->playerIndexForController[controllerIndex] = entityIndex;
            }
        }
    }

    entity *cameraFollowingEntity = GetEntity
        (
            gameState, gameState->cameraFollowingEntityIndex
        );
    
    if(cameraFollowingEntity)
    {
        gameState->cameraPos.absTileZ = cameraFollowingEntity->pos.absTileZ;
        
        tile_map_difference diff = Subtract
            (
                tileMap,
                &cameraFollowingEntity->pos, &gameState->cameraPos
            );

        if(diff.dXY.x > 9.0f * tileMap->tileSideInMeters)
        {
            gameState->cameraPos.absTileX += 17;
        }

        if(diff.dXY.x < -9.0f * tileMap->tileSideInMeters)
        {
            gameState->cameraPos.absTileX -= 17;
        }

        if(diff.dXY.y > 5.0f * tileMap->tileSideInMeters)
        {
            gameState->cameraPos.absTileY += 9;
        }

        if(diff.dXY.y < -5.0f * tileMap->tileSideInMeters)
        {
            gameState->cameraPos.absTileY -= 9;
        }
    }
    //
    // NOTE: Render
    //
    DrawBitmap(buffer, &gameState->bmp, 0, 0);
    
    r32 screenCenterX = 0.5f * (r32)buffer->width;
    r32 screenCenterY = 0.5f * (r32)buffer->height;
    
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
                    tileMap,
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
                        gameState->cameraPos.offset.x +
                        ((r32)relColumn) * tileSideInPixels,
                        
                        screenCenterY + metersToPixels *
                        gameState->cameraPos.offset.y -
                        ((r32)relRow) * tileSideInPixels
                    };
                
                v2 min = center - tileSide;
                v2 max = center + tileSide;
            
                DrawRectangle
                    (
                        buffer, min, max,
                        gray, gray, gray
                    );
            }
        }
    }

    entity *_entity = gameState->entities;
    for(ui32 entityIndex = 0;
        entityIndex < gameState->entityCount;
        entityIndex++, _entity++)
    {
        // TODO: Culling of entities based on Z-axis
        if(_entity->isExists)
        {
            tile_map_difference diff = Subtract
                (
                    tileMap,
                    &_entity->pos, &gameState->cameraPos
                 );
        
            r32 playerR = 1.0f;
            r32 playerG = 1.0f;
            r32 playerB = 0.0f;

            r32 playerGroundX = screenCenterX + metersToPixels *
                diff.dXY.x;
            r32 playerGroundY = screenCenterY - metersToPixels *
                diff.dXY.y;
    
            v2 playerLeftTop =
                {
                    playerGroundX - 0.5f * metersToPixels * _entity->width,
                    playerGroundY - metersToPixels * _entity->height
                };

            v2 entityWidthHeight = {_entity->width, _entity->height};
    
            DrawRectangle
                (
                    buffer,
                    playerLeftTop,
                    playerLeftTop + metersToPixels * entityWidthHeight,
                    playerR, playerG, playerB
                );

            hero_bitmaps *heroBitmaps = &gameState->
                heroBitmaps[_entity->facingDirection];
    
            DrawBitmap
                (
                    buffer, &heroBitmaps->torso,
                    playerGroundX, playerGroundY,
                    heroBitmaps->alignX,
                    heroBitmaps->alignY
                );

            DrawBitmap
                (
                    buffer, &heroBitmaps->cape,
                    playerGroundX, playerGroundY,
                    heroBitmaps->alignX,
                    heroBitmaps->alignY
                );
    
            DrawBitmap
                (
                    buffer, &heroBitmaps->head,
                    playerGroundX, playerGroundY,
                    heroBitmaps->alignX,
                    heroBitmaps->alignY
                );
        }
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
