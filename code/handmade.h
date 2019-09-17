#if !defined(HANDMADE_H)

// TODO: Services that the platform layer provides to the game

// NOTE: Services that the game provides to the platform layer
struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

internal void GameUpdateAndRender(game_offscreen_buffer *buffer, int blueOffset, int greenOffset);

#define HANDMADE_H
#endif
