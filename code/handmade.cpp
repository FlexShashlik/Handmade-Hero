#include "handmade.h"
#include "handmade_intrinsics.h"

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

inline tile_chunk *
GetTileChunk(world *worldMap, int32 tileChunkX, int32 tileChunkY)
{
    tile_chunk *tileChunk = 0;
    
    if(tileChunkX >= 0 && tileChunkX < worldMap->tileChunkCountX &&
       tileChunkY >= 0 && tileChunkY < worldMap->tileChunkCountY)
    {
        tileChunk = &worldMap->tileChunks[tileChunkY * worldMap->tileChunkCountX + tileChunkX];
    }

    return tileChunk;
}

inline uint32
GetTileValueUnchecked
(
    world *worldMap,
    tile_chunk *tileChunk,
    uint32 tileX, uint32 tileY
)
{
    Assert(tileChunk);
    Assert(tileX < worldMap->chunkDim);
    Assert(tileY < worldMap->chunkDim);
    
    return tileChunk->tiles[tileY * worldMap->chunkDim + tileX];
}

inline uint32
GetTileValue
(
    world *worldMap,
    tile_chunk *tileChunk,
    uint32 testTileX, uint32 testTileY
)
{
    uint32 tileChunkValue = 0;

    if(tileChunk)
    {
        tileChunkValue = GetTileValueUnchecked
            (
                worldMap,
                tileChunk, testTileX, testTileY
            );
    }

    return tileChunkValue;
}

inline void
RecanonicalizeCoord
(
    world *worldMap, uint32 *tile, real32 *tileRel
)
{
    // NOTE: World is assumed to be toroidal topology
    int32 offset = RoundReal32ToInt32
        (
            *tileRel / worldMap->tileSideInMeters
        );
    *tile += offset;
    
    *tileRel -= offset * worldMap->tileSideInMeters;
    
    Assert(*tileRel >= -0.5f * worldMap->tileSideInMeters);
    Assert(*tileRel <= 0.5f * worldMap->tileSideInMeters);
}

inline world_position
RecanonicalizePosition(world *worldMap, world_position pos)
{
    world_position result = pos;

    RecanonicalizeCoord
        (
            worldMap, &result.absTileX, &result.tileRelX
        );
    RecanonicalizeCoord
        (
            worldMap, &result.absTileY, &result.tileRelY
        );

    return result;
}

inline tile_chunk_position
GetChunkPosition(world *worldMap, uint32 absTileX, uint32 absTileY)
{
    tile_chunk_position result;

    result.tileChunkX = absTileX >> worldMap->chunkShift;
    result.tileChunkY = absTileY >> worldMap->chunkShift;
    result.relTileX = absTileX & worldMap->chunkMask;
    result.relTileY = absTileY & worldMap->chunkMask;

    return result;
}

internal uint32
GetTileValue(world *worldMap, uint32 absTileX, uint32 absTileY)
{
    tile_chunk_position chunkPos = GetChunkPosition
        (worldMap, absTileX, absTileY);
    
    tile_chunk *tileMap = GetTileChunk
        (
            worldMap,
            chunkPos.tileChunkX, chunkPos.tileChunkY
        );

    uint32 tileChunkValue = GetTileValue
        (
            worldMap, tileMap,
            chunkPos.relTileX, chunkPos.relTileY
        );

    return tileChunkValue;
}

internal bool32
IsWorldPointEmpty(world *worldMap, world_position canPos)
{
    uint32 tileChunkValue = GetTileValue(worldMap, canPos.absTileX, canPos.absTileY);
    bool32 isEmpty = (tileChunkValue == 0);

    return isEmpty;
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

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == ArrayCount(input->controllers[0].buttons));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);

#define TILE_MAP_COUNT_X 256
#define TILE_MAP_COUNT_Y 256  
    uint32 tempTiles[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1}
    };
    
    world worldMap = {};

    // NOTE: This is set to use 256*256 tile chunks
    worldMap.chunkShift = 8;
    worldMap.chunkMask = (1 << worldMap.chunkShift) - 1;
    worldMap.chunkDim = 256;
    
    worldMap.tileChunkCountX = 1;
    worldMap.tileChunkCountY = 1;

    tile_chunk tileChunk;
    tileChunk.tiles = (uint32 *)tempTiles;
    worldMap.tileChunks = &tileChunk;

    worldMap.tileSideInMeters = 1.4f;
    worldMap.tileSideInPixels = 60;
    worldMap.metersToPixels = (real32)worldMap.tileSideInPixels / (real32)worldMap.tileSideInMeters;

    real32 lowerLeftX = -(real32)worldMap.tileSideInPixels/2;
    real32 lowerLeftY = (real32)buffer->height;
    
    real32 playerHeight = worldMap.tileSideInMeters;
    real32 playerWidth = 0.75f * playerHeight;
    
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        gameState->playerPos.absTileX = 3;
        gameState->playerPos.absTileY = 3;
        gameState->playerPos.tileRelX = 5.0f;
        gameState->playerPos.tileRelY = 5.0f;
        
        memory->isInitialized = true;
    }
    
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
            world_position newPlayerPos = gameState->playerPos;
            newPlayerPos.tileRelX += input->deltaTime * dPlayerX;
            newPlayerPos.tileRelY += input->deltaTime * dPlayerY;

            newPlayerPos = RecanonicalizePosition(&worldMap, newPlayerPos);
            
            world_position playerLeft = newPlayerPos;
            playerLeft.tileRelX -= 0.5f * playerWidth;
            playerLeft = RecanonicalizePosition(&worldMap, playerLeft);
            
            world_position playerRight = newPlayerPos;
            playerRight.tileRelX += 0.5f * playerWidth;
            playerRight = RecanonicalizePosition(&worldMap, playerRight);
            
            if(IsWorldPointEmpty(&worldMap, newPlayerPos) &&
               IsWorldPointEmpty(&worldMap, playerLeft) &&
               IsWorldPointEmpty(&worldMap, playerRight))
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
            
            uint32 tileID = GetTileValue(&worldMap, column, row);
            real32 gray = (tileID == 1) ? 1.0f : 0.5f;

            if(row == gameState->playerPos.absTileY &&
               column == gameState->playerPos.absTileX)
            {
                gray = 0.0f;
            }

            real32 centerX = screenCenterX - worldMap.metersToPixels * gameState->playerPos.tileRelX +
                ((real32)relColumn) * worldMap.tileSideInPixels;
            real32 centerY = screenCenterY + worldMap.metersToPixels * gameState->playerPos.tileRelY -
                ((real32)relRow) * worldMap.tileSideInPixels;
            real32 minX = centerX - 0.5f * worldMap.tileSideInPixels;
            real32 minY = centerY - 0.5f * worldMap.tileSideInPixels;
            real32 maxX = centerX + 0.5f * worldMap.tileSideInPixels;
            real32 maxY = centerY + 0.5f * worldMap.tileSideInPixels;
            
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
    
    real32 playerLeft = screenCenterX - 0.5f * worldMap.metersToPixels * playerWidth;
    real32 playerTop = screenCenterY - worldMap.metersToPixels * playerHeight;
    
    DrawRectangle
        (
            buffer,
            playerLeft, playerTop,
            playerLeft + worldMap.metersToPixels * playerWidth,
            playerTop + worldMap.metersToPixels * playerHeight,
            playerR, playerG, playerB
        );
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
