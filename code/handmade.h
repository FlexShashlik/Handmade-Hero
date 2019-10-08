/*
  NOTE:

  HANDMADE_INTERNAL:
      0 - for public release
      1 - for developer only

  HANDMADE_SLOW:
      0 - no slow code
      1 - slow code welcome
 */

#include <math.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef int32 bool32;

typedef float real32;
typedef double real64;

#if HANDMADE_SLOW
#define Assert(expression) if(!(expression)) { *(int *)0 = 0; }
#else
#define Assert(expression)
#endif

#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) ((uint64)Megabytes(value) * 1024)
#define Terabytes(value) ((uint64)Gigabytes(value) * 1024)

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

inline uint32
SafeTruncateUInt64(uint64 value)
{
    Assert(value <= 0xFFFFFFFF);
    uint32 result = (uint32)value;

    return result;
}

// NOTE: Services that the platform layer provides to the game
#if HANDMADE_INTERNAL
/* IMPORTANT:

   These are NOT for doing anything in the shipping game - they
   are blocking and the write doesn't protect against lost data!
 */

struct debug_read_file_result
{
    uint32 contentsSize;
    void *contents;
};

struct thread_context
{
    int placeholder;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *thread, void *memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *thread, char *fileName)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context *thread, char *fileName, uint32 memorySize, void *memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif

// NOTE: Services that the game provides to the platform layer
struct game_offscreen_buffer
{
    void *memory;
    int width;
    int height;
    int pitch;
    int bytesPerPixel;
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
    bool32 isConnected;
    bool32 isAnalog;
        
    real32 stickAverageX;
    real32 stickAverageY;
    
    union
    {
        game_button_state buttons[12];
        struct
        {
            game_button_state moveUp;
            game_button_state moveDown;
            game_button_state moveLeft;
            game_button_state moveRight;
            
            game_button_state actionUp;
            game_button_state actionDown;
            game_button_state actionLeft;
            game_button_state actionRight;
            
            game_button_state leftShoulder;
            game_button_state rightShoulder;

            game_button_state start;
            game_button_state back;

            // NOTE: All buttons must be added above this one
            game_button_state terminator;
        };
    };
};

struct game_input
{
    game_button_state mouseButtons[5];
    int32 mouseX;
    int32 mouseY;
    int32 mouseZ;

    real32 deltaTime;
    
    game_controller_input controllers[5];
};

inline game_controller_input *GetController(game_input *input, int unsigned controllerIndex)
{
    Assert(controllerIndex < ArrayCount(input->controllers));

    return &input->controllers[controllerIndex];
}

struct game_memory
{
    bool32 isInitialized;
    
    uint64 permanentStorageSize;
    void *permanentStorage; // NOTE: REQUIRED to be cleared to zero at startup

    uint64 transientStorageSize;
    void *transientStorage; // NOTE: REQUIRED to be cleared to zero at startup

    debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
    debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
    debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
};

#define GAME_UPDATE_AND_RENDER(name) void name(thread_context *thread, game_memory *memory, game_input *input, game_offscreen_buffer *buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context *thread, game_memory *memory, game_sound_output_buffer *soundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

struct tile_chunk_position
{
    uint32 tileChunkX;
    uint32 tileChunkY;

    uint32 relTileX;
    uint32 relTileY;
};

struct world_position
{
    // NOTE: These are fixed point tile locations. The high bits are
    // the tile chunk index, and the low bits are the tile index in
    // the chunk
    uint32 absTileX;
    uint32 absTileY;

    real32 tileRelX;
    real32 tileRelY;
};

struct tile_chunk
{
    uint32 *tiles;
};

struct world
{
    uint32 chunkShift;
    uint32 chunkMask;
    uint32 chunkDim;
    
    real32 tileSideInMeters;
    int32 tileSideInPixels;
    real32 metersToPixels;
    
    int32 tileChunkCountX;
    int32 tileChunkCountY;
    
    tile_chunk *tileChunks;
};

struct game_state
{
    world_position playerPos;
};
