#if !defined(HANDMADE_H)

// TODO: Services that the platform layer provides to the game

// NOTE: Services that the game provides to the platform layer
struct game_offscreen_buffer
{
    void *memory;
    int width;
    int height;
    int pitch;
};

struct game_sound_output_buffer
{
    int samplesPerSecond;
    int sampleCount;
    int16 *samples;
};

internal void GameUpdateAndRender(game_offscreen_buffer *buffer, int blueOffset, int greenOffset,
                                  game_sound_output_buffer *soundBuffer, int toneHz);

#define HANDMADE_H
#endif
