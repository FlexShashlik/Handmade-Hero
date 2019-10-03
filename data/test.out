#include "handmade.h"

internal void
GameOutputSound(game_state *gameState, game_sound_output_buffer *soundBuffer)
{
    int16 toneVolume = 3000;
    int wavePeriod = soundBuffer->samplesPerSecond / gameState->toneHz;
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

internal void
RenderPlayer(game_offscreen_buffer *buffer, int playerX, int playerY)
{
    uint8 *endOfBuffer = (uint8 *)buffer->memory + buffer->pitch * buffer->height;
    
    uint32 color = 0xFFFFFFFF;
    int top = playerY;
    int bottom = playerY + 10;

    for(int x = playerX; x < playerX + 10; x++)
    {
        uint8 *pixel = (uint8 *)buffer->memory + x * buffer->bytesPerPixel + top * buffer->pitch;
        for(int y = top; y < bottom; y++)
        {
            if(pixel >= buffer->memory && pixel + 4 <= endOfBuffer)
            {
                *(uint32 *)pixel = color;
            }
            
            pixel += buffer->pitch;
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
        char *fileName = __FILE__;
        
        debug_read_file_result file = memory->DEBUGPlatformReadEntireFile(thread, fileName);
        if(file.contents)
        {
            memory->DEBUGPlatformWriteEntireFile(thread, "test.out", file.contentsSize, file.contents);            
            memory->DEBUGPlatformFreeFileMemory(thread, file.contents);            
        }
                
        gameState->toneHz = 256;
        gameState->tSine = 0.0f;

        gameState->playerX = 100;
        gameState->playerY = 100;
        
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
               
        gameState->playerX += (int)(4.0f * controller->stickAverageX);
        gameState->playerY -= (int)(4.0f * controller->stickAverageY);

        // NOTE: Don't do this :DD
        if(gameState->tJump > 0)
        {
            gameState->playerY += (int)(10.0f * sinf(0.5f * Pi32 * gameState->tJump));
        }
        
        if(controller->actionDown.endedDown)
        {
            gameState->tJump = 4.0f;
        }

        gameState->tJump -= 0.033f;
    }
    
    RenderWeirdGradient(buffer, gameState->blueOffset, gameState->greenOffset);
    RenderPlayer(buffer, gameState->playerX, gameState->playerY);

    RenderPlayer(buffer, input->mouseX, input->mouseY);

    for(int buttonIndex = 0; buttonIndex < ArrayCount(input->mouseButtons); buttonIndex++)
    {
        if(input->mouseButtons[buttonIndex].endedDown)
        {
            RenderPlayer(buffer, 10 + 20 * buttonIndex, 10);
        }
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
