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
   u8 *row = (u8 *)buffer->memory;

    for(int y = 0; y < buffer->height; y++)
    {
        u32 *pixel = (u32 *)row;
        
        for(int x = 0; x < buffer->width; x++)
        {
            u8 blue = (u8)(x + blueOffset);
            u8 green = (u8)(y + greenOffset);
            
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
    
    u8 *endOfBuffer = (u8 *)buffer->memory + buffer->pitch * buffer->height;
    
    u32 color =
        (
            RoundR32ToU32(r * 255.0f) << 16 |
            RoundR32ToU32(g * 255.0f) << 8 |
            RoundR32ToU32(b * 255.0f) << 0
        );

    u8 *row = (u8 *)buffer->memory + minX * buffer->bytesPerPixel + minY * buffer->pitch;
    for(i32 y = minY; y < maxY; y++)
    {
        u32 *pixel = (u32 *)row;
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
    
    u32 *sourceRow = bmp->pixels +
        bmp->width * (bmp->height - 1);

    sourceRow += -sourceOffsetY * bmp->width + sourceOffsetX;
    
    u8 *destRow = (u8 *)buffer->memory +
        minX * buffer->bytesPerPixel + minY * buffer->pitch;
    
    for(i32 y = minY; y < maxY; y++)
    {        
        u32 *dest = (u32 *)destRow;
        u32 *source = sourceRow;

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

            *dest = ((u32)(r + 0.5f) << 16|
                     (u32)(g + 0.5f) << 8 |
                     (u32)(b + 0.5f) << 0);
            
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
    u16 fileType;
	u32 fileSize;
	u16 reserved1;
	u16 reserved2;
	u32 bitmapOffset;
    u32 size;
    i32 width;
	i32 height;
	u16 planes;
	u16 bitsPerPixel;
    u32 compression;
    u32 sizeOfBitmap;
    i32 horzResolution;
    i32 vertResolution;
    u32 colorsUsed;
    u32 colorsImportant;

    u32 redMask;
    u32 greenMask;
    u32 blueMask;
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

        u32 *pixels = (u32 *)((u8 *)readResult.contents + header->bitmapOffset);
        result.pixels = pixels;
        
        u32 redMask = header->redMask;
        u32 greenMask = header->greenMask;
        u32 blueMask = header->blueMask;
        u32 alphaMask = ~(redMask | greenMask | blueMask);
        
        bit_scan_result redShift = FindLeastSignificantSetBit(redMask);
        bit_scan_result greenShift = FindLeastSignificantSetBit(greenMask);
        bit_scan_result blueShift = FindLeastSignificantSetBit(blueMask);
        bit_scan_result alphaShift = FindLeastSignificantSetBit(alphaMask);

        Assert(redShift.isFound);
        Assert(greenShift.isFound);
        Assert(blueShift.isFound);
        Assert(alphaShift.isFound);
        
        u32 *sourceDest = pixels;
        for(i32 y = 0; y < header->height; y++)
        {
            for(i32 x = 0; x < header->width; x++)
            {
                u32 c = *sourceDest;
                *sourceDest = ((((c >> alphaShift.index) & 0xFF) << 24) |
                               (((c >> redShift.index) & 0xFF) << 16) |
                               (((c >> greenShift.index) & 0xFF) << 8) |
                               (((c >> blueShift.index) & 0xFF) << 0));
            }
        }
    }

    return result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == ArrayCount(input->controllers[0].buttons));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);
    
    r32 playerHeight = 1.4f;
    r32 playerWidth = 0.75f * playerHeight;
    
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
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
        
        gameState->playerPos.absTileX = 1;
        gameState->playerPos.absTileY = 3;
        gameState->playerPos.offset.x = 5.0f;
        gameState->playerPos.offset.y = 5.0f;
        
        InitializeArena
            (
                &gameState->worldArena,
                memory->permanentStorageSize - sizeof(game_state),
                (u8 *)memory->permanentStorage + sizeof(game_state)
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

        u32 randomNumberIndex = 0;
        
        u32 tilesPerWidth = 17;
        u32 tilesPerHeight = 9;

        u32 screenX = 0;
        u32 screenY = 0;
        
        u32 absTileZ = 0;

        b32 isDoorTop = false;
        b32 isDoorLeft = false;
        b32 isDoorBottom = false;
        b32 isDoorRight = false;
        b32 isDoorUp = false;
        b32 isDoorDown = false;
        
        for(u32 screenIndex = 0;
            screenIndex < 100;
            screenIndex++)
        {
            // TODO: Random number generator
            Assert(randomNumberIndex < ArrayCount(randomNumberTable));
            u32 randomChoice;

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
            
            for(u32 tileY = 0;
                tileY < tilesPerHeight;
                tileY++)
            {
                for(u32 tileX = 0;
                    tileX < tilesPerWidth;
                    tileX++)
                {
                    u32 absTileX = screenX * tilesPerWidth
                        + tileX;
                        
                    u32 absTileY = screenY * tilesPerHeight
                        + tileY;

                    u32 tileValue = 1;
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
                        else
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
        if(controller->isAnalog)
        {
            // NOTE: Analog movement
        }
        else
        {
            // NOTE: Digital movement
            v2 ddPlayerPos = {};
            
            if(controller->moveUp.endedDown)
            {
                gameState->heroFacingDirection = 1;
                ddPlayerPos.y = 1.0f;
            }
            
            if(controller->moveDown.endedDown)
            {
                gameState->heroFacingDirection = 3;
                ddPlayerPos.y = -1.0f;
            }
            
            if(controller->moveLeft.endedDown)
            {
                gameState->heroFacingDirection = 2;
                ddPlayerPos.x = -1.0f;
            }
            
            if(controller->moveRight.endedDown)
            {
                gameState->heroFacingDirection = 0;
                ddPlayerPos.x = 1.0f;
            }
                        
            if(ddPlayerPos.x != 0.0f && ddPlayerPos.y != 0.0f)
            {
                ddPlayerPos *= 0.707106781f;
            }

            r32 playerSpeed = 10.0f;
            if(controller->actionUp.endedDown)
            {
                playerSpeed = 50.0f;
            }
            
            ddPlayerPos *= playerSpeed;

            // TODO: ODE!!!
            ddPlayerPos += -1.5f * gameState->dPlayerPos;
                
            tile_map_position newPlayerPos = gameState->playerPos;
            newPlayerPos.offset =
                0.5f * ddPlayerPos * Square(input->deltaTime) +
                gameState->dPlayerPos * input->deltaTime +
                newPlayerPos.offset;
                        
            gameState->dPlayerPos = ddPlayerPos * input->deltaTime +
                gameState->dPlayerPos;

            newPlayerPos = RecanonicalizePosition(tileMap, newPlayerPos);
            
            tile_map_position playerLeft = newPlayerPos;
            playerLeft.offset.x -= 0.5f * playerWidth;
            playerLeft = RecanonicalizePosition(tileMap, playerLeft);
            
            tile_map_position playerRight = newPlayerPos;
            playerRight.offset.x += 0.5f * playerWidth;
            playerRight = RecanonicalizePosition(tileMap, playerRight);
            
            if(IsTileMapPointEmpty(tileMap, newPlayerPos) &&
               IsTileMapPointEmpty(tileMap, playerLeft) &&
               IsTileMapPointEmpty(tileMap, playerRight))
            {
                if(!AreOnSameTile(&gameState->playerPos, &newPlayerPos))
                {
                    u32 newTileValue = GetTileValue(tileMap, newPlayerPos);
                    
                    if(newTileValue == 3)
                    {
                        newPlayerPos.absTileZ++;
                    }
                    else if(newTileValue == 4)
                    {
                        newPlayerPos.absTileZ--;
                    }
                }
                
                gameState->playerPos = newPlayerPos;
            }

            gameState->cameraPos.absTileZ = gameState->playerPos.absTileZ;

            tile_map_difference diff = SubtractInR32
                (
                    tileMap,
                    &gameState->playerPos, &gameState->cameraPos
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
    }
    
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
            u32 column = gameState->cameraPos.absTileX + relColumn;
            u32 row = gameState->cameraPos.absTileY + relRow;
            
            u32 tileID = GetTileValue
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

    tile_map_difference diff = SubtractInR32
        (
            tileMap,
            &gameState->playerPos, &gameState->cameraPos
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
            playerGroundX - 0.5f * metersToPixels * playerWidth,
            playerGroundY - metersToPixels * playerHeight
        };

    v2 playerWidthHeight = {playerWidth, playerHeight};
    
    DrawRectangle
        (
            buffer,
            playerLeftTop,
            playerLeftTop + metersToPixels * playerWidthHeight,
            playerR, playerG, playerB
        );

    hero_bitmaps *heroBitmaps = &gameState->
        heroBitmaps[gameState->heroFacingDirection];
    
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

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
