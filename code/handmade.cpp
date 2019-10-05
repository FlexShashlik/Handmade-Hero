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
GetTileMapValueUnchecked(tile_map *tileMap, int32 tileX, int32 tileY)
{
    return tileMap->tiles[tileY * tileMap->countX + tileX];
}

internal bool32
IsTileMapPointEmpty(tile_map *tileMap, real32 testX, real32 testY)
{
    bool32 isEmpty = false;
    
    int32 playerTileX = TruncateReal32ToInt32((testX - tileMap->upperLeftX) / tileMap->tileWidth);
    int32 playerTileY = TruncateReal32ToInt32((testY - tileMap->upperLeftY) / tileMap->tileHeight);

    if(playerTileX >= 0 && playerTileX < tileMap->countX &&
       playerTileY >= 0 && playerTileY < tileMap->countY)
    {
        uint32 tileMapValue = GetTileMapValueUnchecked(tileMap, playerTileX, playerTileY);
        
        isEmpty = (tileMapValue == 0);
    }

    return isEmpty;
}

internal bool32
IsWorldMapPointEmpty(world *worldMap, int32 tileMapX, int32 tileMapY, real32 testX, real32 testY)
{
    bool32 isEmpty = false;

    tile_map *tileMap = GetTileMap(worldMap, tileMapX, tileMapY);
    if(tileMap)
    {
        int32 playerTileX = TruncateReal32ToInt32((testX - tileMap->upperLeftX) / tileMap->tileWidth);
        int32 playerTileY = TruncateReal32ToInt32((testY - tileMap->upperLeftY) / tileMap->tileHeight);

        if(playerTileX >= 0 && playerTileX < tileMap->countX &&
           playerTileY >= 0 && playerTileY < tileMap->countY)
        {
            uint32 tileMapValue = GetTileMapValueUnchecked(tileMap, playerTileX, playerTileY);
        
            isEmpty = (tileMapValue == 0);
        }
    }
    
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
    tileMaps[0][0].countX = TILE_MAP_COUNT_X;
    tileMaps[0][0].countY = TILE_MAP_COUNT_Y;
    tileMaps[0][0].upperLeftX = -30;
    tileMaps[0][0].upperLeftY = 0;
    tileMaps[0][0].tileWidth = 60;
    tileMaps[0][0].tileHeight = 60;
    tileMaps[0][0].tiles = (uint32 *)tiles00;

    tileMaps[0][1] = tileMaps[0][0];
    tileMaps[0][1].tiles = (uint32 *)tiles01;

    tileMaps[1][0] = tileMaps[0][0];
    tileMaps[1][0].tiles = (uint32 *)tiles10;

    tileMaps[1][1] = tileMaps[0][0];
    tileMaps[1][1].tiles = (uint32 *)tiles11;

    tile_map *tileMap = &tileMaps[0][0];

    world worldMap = {};
    worldMap.tileMapCountX = 2;
    worldMap.tileMapCountY = 2;
    
    worldMap.tileMaps = (tile_map *)tileMaps;
    
    real32 playerWidth = 0.75f * tileMap->tileWidth;
    real32 playerHeight = tileMap->tileHeight;
    
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        gameState->playerX = 150;
        gameState->playerY = 150;
        
        memory->isInitialized = true;
    }
    
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
            
            if(controller->moveUp.endedDown)
            {
                dPlayerY = -1.0f;
            }
            
            if(controller->moveDown.endedDown)
            {
                dPlayerY = 1.0f;
            }
            
            if(controller->moveLeft.endedDown)
            {
                dPlayerX = -1.0f;
            }
            
            if(controller->moveRight.endedDown)
            {
                dPlayerX = 1.0f;
            }
            
            dPlayerX *= 64.0f;
            dPlayerY *= 64.0f;
            
            // TODO: Diagonal will be faster :D
            real32 newPlayerX = gameState->playerX + input->deltaTime * dPlayerX;
            real32 newPlayerY = gameState->playerY + input->deltaTime * dPlayerY;

            if(IsTileMapPointEmpty(tileMap, newPlayerX - 0.5f * playerWidth, newPlayerY) &&
               IsTileMapPointEmpty(tileMap, newPlayerX + 0.5f * playerWidth, newPlayerY) &&
               IsTileMapPointEmpty(tileMap, newPlayerX, newPlayerY))
            {
                gameState->playerX = newPlayerX;
                gameState->playerY = newPlayerY;
            }
        }
    }
    
    DrawRectangle(buffer, 0.0f, 0.0f, (real32)buffer->width, (real32)buffer->height, 1.0f, 0.0f, 0.1f);
    
    for(int row = 0; row < TILE_MAP_COUNT_Y; row++)
    {
        for(int column = 0; column < TILE_MAP_COUNT_X; column++)
        {
            uint32 tileID = GetTileMapValueUnchecked(tileMap, column, row);
            real32 gray = (tileID == 1) ? 1.0f : 0.5f;
            
            real32 minX = tileMap->upperLeftX + (real32)column * tileMap->tileWidth;
            real32 minY = tileMap->upperLeftY + (real32)row * tileMap->tileHeight;
            real32 maxX = minX + tileMap->tileWidth;
            real32 maxY = minY + tileMap->tileHeight;
            
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
