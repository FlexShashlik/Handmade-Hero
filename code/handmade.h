/*
  NOTE:

  HANDMADE_INTERNAL:
      0 - for public release
      1 - for developer only

  HANDMADE_SLOW:
      0 - no slow code
      1 - slow code welcome
 */

#if HANDMADE_SLOW
#define Assert(expression) if(!(expression)) { *(int *)0 = 0; }
#else
#define Assert(expression)
#endif

#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)
#define Terabytes(value) (Gigabytes(value) * 1024)

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

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

struct game_button_state
{
    int halfTransitionCount;
    bool32 endedDown;
};

struct game_controller_input
{
    bool32 isAnalog;
    
    real32 startX;
    real32 startY;

    real32 minX;
    real32 minY;

    real32 maxX;
    real32 maxY;
    
    real32 endX;
    real32 endY;
    
    union
    {
        game_button_state buttons[6];
        struct
        {
            game_button_state up;
            game_button_state down;
            game_button_state left;
            game_button_state right;
            game_button_state leftShoulder;
            game_button_state rightShoulder;
        };
    };
};

struct game_input
{
    game_controller_input controllers[4];
};

struct game_state
{
    int blueOffset;
    int greenOffset;
    int toneHz;
};

struct game_memory
{
    bool32 isInitialized;
    
    uint64 permanentStorageSize;
    void *permanentStorage; // NOTE: REQUIRED to be cleared to zero at startup

    uint64 transientStorageSize;
    void *transientStorage; // NOTE: REQUIRED to be cleared to zero at startup
};

internal void GameUpdateAndRender
(
    game_memory *memory,
    game_offscreen_buffer *buffer,
    game_sound_output_buffer *soundBuffer
);
