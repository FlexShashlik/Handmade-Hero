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

inline tile_map *
GetTileMap(world *worldMap, int32 tileMapX, int32 tileMapY)
{
    tile_map *tileMap = 0;
    
    if(tileMapX >= 0 && tileMapX < worldMap->tileMapCountX &&
       tileMapY >= 0 && tileMapY < worldMap->tileMapCountY)
    {
        tileMap = &worldMap->tileMaps[tileMapY * worldMap->tileMapCountX + tileMapX];
    }

    return tileMap;
}

inline uint32
GetTileMapValueUnchecked(world *worldMap, tile_map *tileMap, int32 tileX, int32 tileY)
{
    Assert(tileMap);
    Assert(tileX >= 0 && tileX < worldMap->countX &&
           tileY >= 0 && tileY < worldMap->countY);
    
    return tileMap->tiles[tileY * worldMap->countX + tileX];
}

inline bool32
IsTileMapPointEmpty(world *worldMap, tile_map *tileMap, int32 testTileX, int32 testTileY)
{
    bool32 isEmpty = false;

    if(tileMap)
    {
        if(testTileX >= 0 && testTileX < worldMap->countX &&
           testTileY >= 0 && testTileY < worldMap->countY)
        {
            uint32 tileMapValue = GetTileMapValueUnchecked(worldMap, tileMap, testTileX, testTileY);
        
            isEmpty = (tileMapValue == 0);
        }
    }

    return isEmpty;
}

inline void
RecanonicalizeCoord
(
    world *worldMap, int32 tileCount,
    int32 *tileMap, int32 *tile, real32 *tileRel
)
{
    int32 offset = FloorReal32ToInt32(*tileRel / worldMap->tileSideInMeters);
    *tile += offset;
    
    *tileRel -= offset * worldMap->tileSideInMeters;
    
    Assert(*tileRel >= 0);
    Assert(*tileRel < worldMap->tileSideInMeters);
    
    if(*tile < 0)
    {
        *tile = tileCount + *tile;
        (*tileMap)--;
    }

    if(*tile >= tileCount)
    {
        *tile = *tile - tileCount;
        (*tileMap)++;
    }
}

inline canonical_position
RecanonicalizePosition(world *worldMap, canonical_position pos)
{
    canonical_position result = pos;

    RecanonicalizeCoord
        (
            worldMap, worldMap->countX,
            &result.tileMapX, &result.tileX, &result.tileRelX
        );
    RecanonicalizeCoord
        (
            worldMap, worldMap->countY,
            &result.tileMapY, &result.tileY, &result.tileRelY
        );

    return result;
}

internal bool32
IsWorldPointEmpty(world *worldMap, canonical_position canPos)
{
    bool32 isEmpty = false;

    tile_map *tileMap = GetTileMap
        (
            worldMap,
            canPos.tileMapX, canPos.tileMapY
        );
    
    isEmpty = IsTileMapPointEmpty
        (
            worldMap, tileMap,
            canPos.tileX, canPos.tileY
        );

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

#define TILE_MAP_COUNT_X 17
#define TILE_MAP_COUNT_Y 9    
    uint32 tiles00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
        {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 0},
        {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1},
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1},
        {1, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1}
    };

    uint32 tiles01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1}
    };

    uint32 tiles10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1}
    };

    uint32 tiles11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1}
    };
    
    tile_map tileMaps[2][2];
        
    tileMaps[0][0].tiles = (uint32 *)tiles00;
    tileMaps[0][1].tiles = (uint32 *)tiles10;
    tileMaps[1][0].tiles = (uint32 *)tiles01;
    tileMaps[1][1].tiles = (uint32 *)tiles11;

    world worldMap = {};
    worldMap.tileMapCountX = 2;
    worldMap.tileMapCountY = 2;
    worldMap.countX = TILE_MAP_COUNT_X;
    worldMap.countY = TILE_MAP_COUNT_Y;

    // TODO: Begin using tileSideInMeters
    worldMap.tileSideInMeters = 1.4f;
    worldMap.tileSideInPixels = 60;
    worldMap.metersToPixels = (real32)worldMap.tileSideInPixels / (real32)worldMap.tileSideInMeters;
    
    worldMap.upperLeftX = -(real32)worldMap.tileSideInPixels/2;
    worldMap.upperLeftY = 0;
    
    real32 playerHeight = worldMap.tileSideInMeters;
    real32 playerWidth = 0.75f * playerHeight;
    
    worldMap.tileMaps = (tile_map *)tileMaps;
    
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        gameState->playerPos.tileMapX = 0;
        gameState->playerPos.tileMapY = 0;
        gameState->playerPos.tileX = 3;
        gameState->playerPos.tileY = 3;
        gameState->playerPos.tileRelX = 5.0f;
        gameState->playerPos.tileRelY = 5.0f;
        
        memory->isInitialized = true;
    }
    
    tile_map *tileMap = GetTileMap
        (
            &worldMap,
            gameState->playerPos.tileMapX,
            gameState->playerPos.tileMapY
        );
    Assert(tileMap);
    
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
            
            if(controller->moveUp.endedDown ||
               controller->actionUp.endedDown)
            {
                dPlayerY = -1.0f;
            }
            
            if(controller->moveDown.endedDown ||
               controller->actionDown.endedDown)
            {
                dPlayerY = 1.0f;
            }
            
            if(controller->moveLeft.endedDown ||
               controller->actionLeft.endedDown)
            {
                dPlayerX = -1.0f;
            }
            
            if(controller->moveRight.endedDown ||
               controller->actionRight.endedDown)
            {
                dPlayerX = 1.0f;
            }
            
            dPlayerX *= 4.0f;
            dPlayerY *= 4.0f;
            
            // TODO: Diagonal will be faster :D
            canonical_position newPlayerPos = gameState->playerPos;
            newPlayerPos.tileRelX += input->deltaTime * dPlayerX;
            newPlayerPos.tileRelY += input->deltaTime * dPlayerY;

            newPlayerPos = RecanonicalizePosition(&worldMap, newPlayerPos);
            
            canonical_position playerLeft = newPlayerPos;
            playerLeft.tileRelX -= 0.5f * playerWidth;
            playerLeft = RecanonicalizePosition(&worldMap, playerLeft);
            
            canonical_position playerRight = newPlayerPos;
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
    
    DrawRectangle(buffer, 0.0f, 0.0f, (real32)buffer->width, (real32)buffer->height, 1.0f, 0.0f, 0.1f);
    
    for(int row = 0; row < TILE_MAP_COUNT_Y; row++)
    {
        for(int column = 0; column < TILE_MAP_COUNT_X; column++)
        {
            uint32 tileID = GetTileMapValueUnchecked(&worldMap, tileMap, column, row);
            real32 gray = (tileID == 1) ? 1.0f : 0.5f;

            if(row == gameState->playerPos.tileY &&
               column == gameState->playerPos.tileX)
            {
                gray = 0.0f;
            }
            
            real32 minX = worldMap.upperLeftX + (real32)column * worldMap.tileSideInPixels;
            real32 minY = worldMap.upperLeftY + (real32)row * worldMap.tileSideInPixels;
            real32 maxX = minX + worldMap.tileSideInPixels;
            real32 maxY = minY + worldMap.tileSideInPixels;
            
            DrawRectangle(buffer, minX, minY, maxX, maxY, gray, gray, gray);
        }
    }

    real32 playerR = 1.0f;
    real32 playerG = 1.0f;
    real32 playerB = 0.0f;
    real32 playerLeft = worldMap.upperLeftX +
        worldMap.tileSideInPixels * gameState->playerPos.tileX +
        worldMap.metersToPixels * gameState->playerPos.tileRelX - 0.5f * worldMap.metersToPixels * playerWidth;
    real32 playerTop = worldMap.upperLeftY +
        worldMap.tileSideInPixels * gameState->playerPos.tileY +
        worldMap.metersToPixels * gameState->playerPos.tileRelY - worldMap.metersToPixels * playerHeight;
    
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
