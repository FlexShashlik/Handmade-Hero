#include "handmade.h"
#include "handmade_tile.cpp"

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
InitializeArena
(
    memory_arena *arena,
    memory_index size,
    uint8 *base
)
{
    arena->size = size;
    arena->base = base;
    arena->used = 0;
}

#define PushStruct(arena, type) (type *)PushSize_(arena, sizeof(type))
#define PushArray(arena, count, type) (type *)PushSize_(arena, (count) * sizeof(type))
void *
PushSize_(memory_arena *arena, memory_index size)
{
    Assert(arena->used + size <= arena->size)
    
    void *result = arena->base + arena->used;
    arena->used += size;

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

        // NOTE: This is set to use 256*256 tile chunks
        tileMap->chunkShift = 4;
        tileMap->chunkMask = (1 << tileMap->chunkShift) - 1;
        tileMap->chunkDim = 1 << tileMap->chunkShift;
        
        tileMap->tileChunkCountX = 128;
        tileMap->tileChunkCountY = 128;

        tileMap->tileChunks = PushArray
            (
                &gameState->worldArena,
                tileMap->tileChunkCountX * tileMap->tileChunkCountY,
                tile_chunk
            );
    
        for(uint32 y = 0;
            y < tileMap->tileChunkCountY;
            y++)
        {
            for(uint32 x = 0;
                x < tileMap->tileChunkCountX;
                x++)
            {
                tileMap->tileChunks[y * tileMap->tileChunkCountX + x]
                    .tiles = PushArray
                    (
                        &gameState->worldArena,
                        tileMap->chunkDim * tileMap->chunkDim,
                        uint32
                    );
            }
        }

        tileMap->tileSideInMeters = 1.4f;
        tileMap->tileSideInPixels = 60;
        tileMap->metersToPixels = (real32)tileMap->tileSideInPixels / (real32)tileMap->tileSideInMeters;

        real32 lowerLeftX = -(real32)tileMap->tileSideInPixels/2;
        real32 lowerLeftY = (real32)buffer->height;
        
        uint32 tilesPerWidth = 17;
        uint32 tilesPerHeight = 9;
        for(uint32 screenY = 0;
            screenY < 32;
            screenY++)
        {
            for(uint32 screenX = 0;
            screenX < 32;
            screenX++)
            {
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
                        
                        SetTileValue
                            (
                                &gameState->worldArena, tileMap,
                                absTileX, absTileY,
                                (tileX == tileY && tileY % 2) ? 1 : 0
                            );
                    }
                }
            }
        }
        
        memory->isInitialized = true;
    }

    world *worldMap = gameState->worldMap;
    tile_map *tileMap = worldMap->tileMap;
    
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
                gameState->playerPos = newPlayerPos;
            }
        }
    }
    
    DrawRectangle
        (
            buffer, 0.0f, 0.0f,
            (real32)buffer->width, (real32)buffer->height,
            1.0f, 0.0f, 0.1f
        );

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
            
            uint32 tileID = GetTileValue(tileMap, column, row);
            real32 gray = (tileID == 1) ? 1.0f : 0.5f;

            if(row == gameState->playerPos.absTileY &&
               column == gameState->playerPos.absTileX)
            {
                gray = 0.0f;
            }

            real32 centerX = screenCenterX - tileMap->metersToPixels * gameState->playerPos.tileRelX +
                ((real32)relColumn) * tileMap->tileSideInPixels;
            real32 centerY = screenCenterY + tileMap->metersToPixels * gameState->playerPos.tileRelY -
                ((real32)relRow) * tileMap->tileSideInPixels;
            real32 minX = centerX - 0.5f * tileMap->tileSideInPixels;
            real32 minY = centerY - 0.5f * tileMap->tileSideInPixels;
            real32 maxX = centerX + 0.5f * tileMap->tileSideInPixels;
            real32 maxY = centerY + 0.5f * tileMap->tileSideInPixels;
            
            DrawRectangle
                (
                    buffer, minX, minY,
                    maxX, maxY,
                    gray, gray, gray
                );
        }
    }

    real32 playerR = 1.0f;
    real32 playerG = 1.0f;
    real32 playerB = 0.0f;
    
    real32 playerLeft = screenCenterX - 0.5f * tileMap->metersToPixels * playerWidth;
    real32 playerTop = screenCenterY - tileMap->metersToPixels * playerHeight;
    
    DrawRectangle
        (
            buffer,
            playerLeft, playerTop,
            playerLeft + tileMap->metersToPixels * playerWidth,
            playerTop + tileMap->metersToPixels * playerHeight,
            playerR, playerG, playerB
        );
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
