#include "handmade.h"
#include "handmade_tile.cpp"
#include "handmade_random.h"

internal void
GameOutputSound(game_state *gameState, game_sound_output_buffer *soundBuffer)
{
    int16 toneVolume = 3000;
    int wavePeriod = soundBuffer->samplesPerSecond / 400;
    int16 *sampleOut = soundBuffer->samples;
    
    for(int sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; sampleIndex++)
    {
#if 0
        real32 sineValue = sinf(gameState->tSine);
        int16 sampleValue = (int16)(sineValue * toneVolume);
#else
        int16 sampleValue = 0;
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
RenderWeirdGradient(game_offscreen_buffer *buffer, int blueOffset, int greenOffset)
{
    uint8 *row = (uint8 *)buffer->memory;

    for(int y = 0; y < buffer->height; y++)
    {
        uint32 *pixel = (uint32 *)row;
        
        for(int x = 0; x < buffer->width; x++)
        {
            uint8 blue = (uint8)(x + blueOffset);
            uint8 green = (uint8)(y + greenOffset);
            
            *pixel++ = ((green << 8) | blue);
        }

        row += buffer->pitch;
    }
}

internal void
DrawRectangle
(
    game_offscreen_buffer *buffer,
    real32 realMinX, real32 realMinY,
    real32 realMaxX, real32 realMaxY,
    real32 r, real32 g, real32 b
)
{
    int32 minX = RoundReal32ToInt32(realMinX);
    int32 minY = RoundReal32ToInt32(realMinY);
    int32 maxX = RoundReal32ToInt32(realMaxX);
    int32 maxY = RoundReal32ToInt32(realMaxY);

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
    
    uint8 *endOfBuffer = (uint8 *)buffer->memory + buffer->pitch * buffer->height;
    
    uint32 color =
        (
            RoundReal32ToUInt32(r * 255.0f) << 16 |
            RoundReal32ToUInt32(g * 255.0f) << 8 |
            RoundReal32ToUInt32(b * 255.0f) << 0
        );

    uint8 *row = (uint8 *)buffer->memory + minX * buffer->bytesPerPixel + minY * buffer->pitch;
    for(int y = minY; y < maxY; y++)
    {
        uint32 *pixel = (uint32 *)row;
        for(int x = minX; x < maxX; x++)
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
    real32 realX, real32 realY
)
{
    int32 minX = RoundReal32ToInt32(realX);
    int32 minY = RoundReal32ToInt32(realY);
    int32 maxX = RoundReal32ToInt32(realX + (real32)bmp->width);
    int32 maxY = RoundReal32ToInt32(realY + (real32)bmp->height);

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
    
    uint32 *sourceRow = bmp->pixels +
        bmp->width * (bmp->height - 1);
    
    uint8 *destRow = (uint8 *)buffer->memory +
        minX * buffer->bytesPerPixel + minY * buffer->pitch;
    
    for(int32 y = minY; y < maxY; y++)
    {        
        uint32 *dest = (uint32 *)destRow;
        uint32 *source = sourceRow;

        for(int x = minX; x < maxX; x++)
        {
            real32 a = (real32)((*source >> 24) & 0xFF) / 255.0f;
            real32 sr = (real32)((*source >> 16) & 0xFF);
            real32 sg = (real32)((*source >> 8) & 0xFF);
            real32 sb = (real32)((*source >> 0) & 0xFF);

            real32 dr = (real32)((*dest >> 16) & 0xFF);
            real32 dg = (real32)((*dest >> 8) & 0xFF);
            real32 db = (real32)((*dest >> 0) & 0xFF);

            real32 r = (1.0f - a) * dr + a * sr;
            real32 g = (1.0f - a) * dg + a * sg;
            real32 b = (1.0f - a) * db + a * sb;

            *dest = ((uint32)(r + 0.5f) << 16|
                     (uint32)(g + 0.5f) << 8 |
                     (uint32)(b + 0.5f) << 0);
            
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
    uint16 fileType;
	uint32 fileSize;
	uint16 reserved1;
	uint16 reserved2;
	uint32 bitmapOffset;
    uint32 size;
    int32 width;
	int32 height;
	uint16 planes;
	uint16 bitsPerPixel;
    uint32 compression;
    uint32 sizeOfBitmap;
    int32 horzResolution;
    int32 vertResolution;
    uint32 colorsUsed;
    uint32 colorsImportant;

    uint32 redMask;
    uint32 greenMask;
    uint32 blueMask;
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

        uint32 *pixels = (uint32 *)((uint8 *)readResult.contents + header->bitmapOffset);
        result.pixels = pixels;
        
        uint32 redMask = header->redMask;
        uint32 greenMask = header->greenMask;
        uint32 blueMask = header->blueMask;
        uint32 alphaMask = ~(redMask | greenMask | blueMask);
        
        bit_scan_result redShift = FindLeastSignificantSetBit(redMask);
        bit_scan_result greenShift = FindLeastSignificantSetBit(greenMask);
        bit_scan_result blueShift = FindLeastSignificantSetBit(blueMask);
        bit_scan_result alphaShift = FindLeastSignificantSetBit(alphaMask);

        Assert(redShift.isFound);
        Assert(greenShift.isFound);
        Assert(blueShift.isFound);
        Assert(alphaShift.isFound);
        
        uint32 *sourceDest = pixels;
        for(int32 y = 0; y < header->height; y++)
        {
            for(int32 x = 0; x < header->width; x++)
            {
                uint32 c = *sourceDest;
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
    
    real32 playerHeight = 1.4f;
    real32 playerWidth = 0.75f * playerHeight;
    
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        gameState->bmp = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_background.bmp"
            );

        gameState->heroHead = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_front_head.bmp"
            );
        
        gameState->heroCape = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_front_cape.bmp"
            );
            
        gameState->heroTorso = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_front_torso.bmp"
            );
        
        gameState->playerPos.absTileX = 1;
        gameState->playerPos.absTileY = 3;
        gameState->playerPos.tileRelX = 5.0f;
        gameState->playerPos.tileRelY = 5.0f;
        
        InitializeArena
            (
                &gameState->worldArena,
                memory->permanentStorageSize - sizeof(game_state),
                (uint8 *)memory->permanentStorage + sizeof(game_state)
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

        uint32 randomNumberIndex = 0;
        
        uint32 tilesPerWidth = 17;
        uint32 tilesPerHeight = 9;

        uint32 screenX = 0;
        uint32 screenY = 0;
        
        uint32 absTileZ = 0;

        bool32 isDoorTop = false;
        bool32 isDoorLeft = false;
        bool32 isDoorBottom = false;
        bool32 isDoorRight = false;
        bool32 isDoorUp = false;
        bool32 isDoorDown = false;
        
        for(uint32 screenIndex = 0;
            screenIndex < 100;
            screenIndex++)
        {
            // TODO: Random number generator
            Assert(randomNumberIndex < ArrayCount(randomNumberTable));
            uint32 randomChoice;

            if(isDoorUp || isDoorDown)
            {
                randomChoice = randomNumberTable[randomNumberIndex++] % 2;
            }
            else
            {
                randomChoice = randomNumberTable[randomNumberIndex++] % 3;
            }

            bool32 isCreatedZDoor = false;
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
            
            for(uint32 tileY = 0;
                tileY < tilesPerHeight;
                tileY++)
            {
                for(uint32 tileX = 0;
                    tileX < tilesPerWidth;
                    tileX++)
                {
                    uint32 absTileX = screenX * tilesPerWidth
                        + tileX;
                        
                    uint32 absTileY = screenY * tilesPerHeight
                        + tileY;

                    uint32 tileValue = 1;
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
    
    int32 tileSideInPixels = 60;
    real32 metersToPixels = (real32)tileSideInPixels / (real32)tileMap->tileSideInMeters;
    
    real32 lowerLeftX = -(real32)tileSideInPixels/2;
    real32 lowerLeftY = (real32)buffer->height;
    
    for(int controllerIndex = 0;
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
            real32 dPlayerX = 0.0f;
            real32 dPlayerY = 0.0f;
            
            if(controller->moveUp.endedDown)
            {
                dPlayerY = 1.0f;
            }
            
            if(controller->moveDown.endedDown)
            {
                dPlayerY = -1.0f;
            }
            
            if(controller->moveLeft.endedDown)
            {
                dPlayerX = -1.0f;
            }
            
            if(controller->moveRight.endedDown)
            {
                dPlayerX = 1.0f;
            }
            
            real32 playerSpeed = 2.0f;
            if(controller->actionUp.endedDown)
            {
                playerSpeed = 10.0f;
            }
            
            dPlayerX *= playerSpeed;
            dPlayerY *= playerSpeed;
            
            // TODO: Diagonal will be faster :D
            tile_map_position newPlayerPos = gameState->playerPos;
            newPlayerPos.tileRelX += input->deltaTime * dPlayerX;
            newPlayerPos.tileRelY += input->deltaTime * dPlayerY;

            newPlayerPos = RecanonicalizePosition(tileMap, newPlayerPos);
            
            tile_map_position playerLeft = newPlayerPos;
            playerLeft.tileRelX -= 0.5f * playerWidth;
            playerLeft = RecanonicalizePosition(tileMap, playerLeft);
            
            tile_map_position playerRight = newPlayerPos;
            playerRight.tileRelX += 0.5f * playerWidth;
            playerRight = RecanonicalizePosition(tileMap, playerRight);
            
            if(IsTileMapPointEmpty(tileMap, newPlayerPos) &&
               IsTileMapPointEmpty(tileMap, playerLeft) &&
               IsTileMapPointEmpty(tileMap, playerRight))
            {
                if(!AreOnSameTile(&gameState->playerPos, &newPlayerPos))
                {
                    uint32 newTileValue = GetTileValue(tileMap, newPlayerPos);
                    
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
        }
    }
    
    DrawBitmap(buffer, &gameState->bmp, 0, 0);
    
    real32 screenCenterX = 0.5f * (real32)buffer->width;
    real32 screenCenterY = 0.5f * (real32)buffer->height;
    
    for(int32 relRow = -10;
        relRow < 10;
        relRow++)
    {
        for(int32 relColumn = -20;
            relColumn < 20;
            relColumn++)
        {
            uint32 column = gameState->playerPos.absTileX + relColumn;
            uint32 row = gameState->playerPos.absTileY + relRow;
            
            uint32 tileID = GetTileValue
                (
                    tileMap,
                    column, row, gameState->playerPos.absTileZ
                );
            
            if(tileID > 1)
            {
                real32 gray = (tileID == 2) ? 1.0f : 0.5f;

                if(tileID > 2) gray = 0.25f;
                
                if(row == gameState->playerPos.absTileY &&
                   column == gameState->playerPos.absTileX)
                {
                    gray = 0.0f;
                }

                real32 centerX = screenCenterX - metersToPixels * gameState->playerPos.tileRelX +
                    ((real32)relColumn) * tileSideInPixels;
                real32 centerY = screenCenterY + metersToPixels * gameState->playerPos.tileRelY -
                    ((real32)relRow) * tileSideInPixels;
                real32 minX = centerX - 0.5f * tileSideInPixels;
                real32 minY = centerY - 0.5f * tileSideInPixels;
                real32 maxX = centerX + 0.5f * tileSideInPixels;
                real32 maxY = centerY + 0.5f * tileSideInPixels;
            
                DrawRectangle
                    (
                        buffer, minX, minY,
                        maxX, maxY,
                        gray, gray, gray
                    );
            }
        }
    }

    real32 playerR = 1.0f;
    real32 playerG = 1.0f;
    real32 playerB = 0.0f;
    
    real32 playerLeft = screenCenterX - 0.5f * metersToPixels * playerWidth;
    real32 playerTop = screenCenterY - metersToPixels * playerHeight;
    
    DrawRectangle
        (
            buffer,
            playerLeft, playerTop,
            playerLeft + metersToPixels * playerWidth,
            playerTop + metersToPixels * playerHeight,
            playerR, playerG, playerB
        );
    
    DrawBitmap
        (
            buffer, &gameState->heroHead,
            playerLeft, playerTop
        );
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
