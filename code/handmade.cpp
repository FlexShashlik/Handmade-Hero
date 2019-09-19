#include "handmade.h"

internal void
GameOutputSound(game_sound_output_buffer *soundBuffer, int toneHz)
{
    local_persist real32 tSine;
    int16 toneVolume = 3000;
    int wavePeriod = soundBuffer->samplesPerSecond / toneHz;
    int16 *sampleOut = soundBuffer->samples;
    
    for(int sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; sampleIndex++)
    {            
        real32 sineValue = sinf(tSine);
        int16 sampleValue = (int16)(sineValue * toneVolume);
                            
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
            
        tSine += 2.0f * Pi32 / (real32)wavePeriod;
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
            uint8 blue = x + blueOffset;
            uint8 green = y + greenOffset;
            
            *pixel++ = ((green << 8) | blue);
        }

        row += buffer->pitch;
    }
}

internal game_state *
GameStartup(void)
{
    game_state gameState* = new game_state;
    if(gameState)
    {
        gameState->blueOffset = 0;
        gameState->greenOffset = 0;
        gameState->toneHz = 256;
    }

    return gameState;
}

internal void
GameShutdown(game_state *gameState)
{
    delete gameState;
}

internal void
GameUpdateAndRender(game_state *gameState,
                    game_input *input,
                    game_offscreen_buffer *buffer,
                    game_sound_output_buffer *soundBuffer)
{
    game_controller_input *input0 = &input->controllers[1];
    
    if(input0->isAnalog)
    {
        gameState->blueOffset += 4.0f * input0->endX;
        gameState->toneHz = 256 + (int)(128.0f * (input0->endY));
    }
    else
    {
    }

    if(input0->down.endedDown)
    {
        gameState->greenOffset += 1;
    }
    
    // TODO: Allow sample offsets
    GameOutputSound(soundBuffer, toneHz);
    RenderWeirdGradient(buffer, blueOffset, greenOffset);
}
