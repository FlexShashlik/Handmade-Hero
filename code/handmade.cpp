#include "handmade.h"

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

inline int32
RoundReal32ToInt32(real32 value)
{
    return (int32)(value + 0.5f);
}

inline uint32
RoundReal32ToUInt32(real32 value)
{
    return (uint32)(value + 0.5f);
}

#include "math.h"
inline int32
FloorReal32ToInt32(real32 value)
{
    return (int32)floorf(value);
}

inline int32
TruncateReal32ToInt32(real32 value)
{
    return (int32)value;
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

inline canonical_position
GetCanonicalPosition(world *worldMap, raw_position pos)
{
    canonical_position result;
    result.tileMapX = pos.tileMapX;
    result.tileMapY = pos.tileMapY;

    real32 x = pos.x - worldMap->upperLeftX;
    real32 y = pos.y - worldMap->upperLeftY;
    
    result.tileX = FloorReal32ToInt32(x / worldMap->tileWidth);
    result.tileY = FloorReal32ToInt32(y / worldMap->tileHeight);

    result.tileRelX = x - result.tileX * worldMap->tileWidth;
    result.tileRelY = y - result.tileY * worldMap->tileHeight;

    Assert(result.tileRelX >= 0);
    Assert(result.tileRelY >= 0);
    Assert(result.tileRelX < worldMap->tileWidth);
    Assert(result.tileRelY < worldMap->tileHeight);
    
    if(result.tileX < 0)
    {
        result.tileX = worldMap->countX + result.tileX;
        result.tileMapX--;
    }

    if(result.tileY < 0)
    {
        result.tileY = worldMap->countY + result.tileY;
        result.tileMapY--;
    }

    if(result.tileX >= worldMap->countX)
    {
        result.tileX = result.tileX - worldMap->countX;
        result.tileMapX++;
    }

    if(result.tileY >= worldMap->countY)
    {
        result.tileY = result.tileY - worldMap->countY;
        result.tileMapY++;
    }

    return result;
}

internal bool32
IsWorldPointEmpty(world *worldMap, raw_position testPos)
{
    bool32 isEmpty = false;

    canonical_position canPos = GetCanonicalPosition(worldMap, testPos);
    
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
    worldMap.upperLeftX = -30;
    worldMap.upperLeftY = 0;
    worldMap.tileWidth = 60;
    worldMap.tileHeight = 60;
    
    real32 playerWidth = 0.75f * worldMap.tileWidth;
    real32 playerHeight = worldMap.tileHeight;
    
    worldMap.tileMaps = (tile_map *)tileMaps;
    
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        gameState->playerX = 150;
        gameState->playerY = 150;
        
        memory->isInitialized = true;
    }
    
    tile_map *tileMap = GetTileMap(&worldMap, gameState->playerTileMapX, gameState->playerTileMapY);
    Assert(tileMap);
    
    for(int controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); controllerIndex++)
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
            
            dPlayerX *= 64.0f;
            dPlayerY *= 64.0f;
            
            // TODO: Diagonal will be faster :D
            real32 newPlayerX = gameState->playerX + input->deltaTime * dPlayerX;
            real32 newPlayerY = gameState->playerY + input->deltaTime * dPlayerY;

            raw_position playerPos = {gameState->playerTileMapX, gameState->playerTileMapY, newPlayerX, newPlayerY};
            raw_position playerLeft = playerPos;
            playerLeft.x -= 0.5f * playerWidth;
            raw_position playerRight = playerPos;
            playerRight.x += 0.5f * playerWidth;
            
            if(IsWorldPointEmpty(&worldMap, playerPos) &&
               IsWorldPointEmpty(&worldMap, playerLeft) &&
               IsWorldPointEmpty(&worldMap, playerRight))
            {
                canonical_position canPos = GetCanonicalPosition(&worldMap, playerPos);

                gameState->playerTileMapX = canPos.tileMapX;
                gameState->playerTileMapY = canPos.tileMapY;
                
                gameState->playerX = worldMap.upperLeftX + worldMap.tileWidth * canPos.tileX + canPos.tileRelX;
                gameState->playerY = worldMap.upperLeftY + worldMap.tileHeight * canPos.tileY + canPos.tileRelY;
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
            
            real32 minX = worldMap.upperLeftX + (real32)column * worldMap.tileWidth;
            real32 minY = worldMap.upperLeftY + (real32)row * worldMap.tileHeight;
            real32 maxX = minX + worldMap.tileWidth;
            real32 maxY = minY + worldMap.tileHeight;
            
            DrawRectangle(buffer, minX, minY, maxX, maxY, gray, gray, gray);
        }
    }

    real32 playerR = 1.0f;
    real32 playerG = 1.0f;
    real32 playerB = 0.0f;
    real32 playerLeft = gameState->playerX - 0.5f * playerWidth;
    real32 playerTop = gameState->playerY - playerHeight;
    
    DrawRectangle
        (
            buffer,
            playerLeft, playerTop,
            playerLeft + playerWidth, playerTop + playerHeight,
            playerR, playerG, playerB
        );
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
