#include "handmade.h"

internal void
GameOutputSound(game_state *gameState, game_sound_output_buffer *soundBuffer)
{
    int16 toneVolume = 3000;
    int wavePeriod = soundBuffer->samplesPerSecond / gameState->toneHz;
    int16 *sampleOut = soundBuffer->samples;
    
    for(int sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; sampleIndex++)
    {            
        real32 sineValue = sinf(gameState->tSine);
        int16 sampleValue = (int16)(sineValue * toneVolume);
                            
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
            
        gameState->tSine += 2.0f * Pi32 / (real32)wavePeriod;
        if(gameState->tSine > 2.0f * Pi32)
        {
            gameState->tSine -= 2.0f * Pi32;
        }
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

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == ArrayCount(input->controllers[0].buttons));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);
    
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        char *fileName = __FILE__;
        
        debug_read_file_result file = memory->DEBUGPlatformReadEntireFile(fileName);
        if(file.contents)
        {
            memory->DEBUGPlatformWriteEntireFile("test.out", file.contentsSize, file.contents);            
            memory->DEBUGPlatformFreeFileMemory(file.contents);            
        }
                
        gameState->toneHz = 256;
        gameState->tSine = 0.0f;

        memory->isInitialized = true;
    }

    for(int controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); controllerIndex++)
    {
        game_controller_input *controller = GetController(input, controllerIndex);    
        if(controller->isAnalog)
        {
            gameState->blueOffset += (int)(4.0f * controller->stickAverageX);
            gameState->toneHz = 256 + (int)(128.0f * (controller->stickAverageY));
        }
        else
        {
            // NOTE: Digital movement

            if(controller->moveLeft.endedDown)
            {
                gameState->blueOffset -= 1;
            }
            if(controller->moveRight.endedDown)
            {
                gameState->blueOffset += 1;
            }
        }

        if(controller->actionDown.endedDown)
        {
            gameState->greenOffset += 1;
        }
    }
    
    RenderWeirdGradient(buffer, gameState->blueOffset, gameState->greenOffset);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
