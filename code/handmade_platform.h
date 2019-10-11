/*
  NOTE:

  HANDMADE_INTERNAL:
      0 - for public release
      1 - for developer only

  HANDMADE_SLOW:
      0 - no slow code
      1 - slow code welcome
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COMPILER_MSVC
#define COMPILER_MSVC 0
#endif

#ifndef COMPILER_LLVM
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
    // TODO: More compilers
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#endif
    
#include <stdint.h>
#include <stddef.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef int32 bool32;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

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

typedef struct thread_context
{
    int placeholder;
} thread_context;

// NOTE: Services that the platform layer provides to the game
#if HANDMADE_INTERNAL
/* IMPORTANT:

   These are NOT for doing anything in the shipping game - they
   are blocking and the write doesn't protect against lost data!
 */

typedef struct debug_read_file_result
{
    uint32 contentsSize;
    void *contents;
} debug_read_file_result;

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *thread, void *memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *thread, char *fileName)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context *thread, char *fileName, uint32 memorySize, void *memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif

typedef struct game_offscreen_buffer
{
    void *memory;
    int width;
    int height;
    int pitch;
    int bytesPerPixel;
} game_offscreen_buffer;

typedef struct game_sound_output_buffer
{
    int samplesPerSecond;
    int sampleCount;
    int16 *samples;
} game_sound_output_buffer;

typedef struct game_button_state
{
    int halfTransitionCount;
    bool32 endedDown;
} game_button_state;

typedef struct game_controller_input
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
} game_controller_input;

typedef struct game_input
{
    game_button_state mouseButtons[5];
    int32 mouseX;
    int32 mouseY;
    int32 mouseZ;

    real32 deltaTime;
    
    game_controller_input controllers[5];
} game_input;

typedef struct game_memory
{
    bool32 isInitialized;
    
    uint64 permanentStorageSize;
    void *permanentStorage; // NOTE: REQUIRED to be cleared to zero at startup

    uint64 transientStorageSize;
    void *transientStorage; // NOTE: REQUIRED to be cleared to zero at startup

    debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
    debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
    debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) void name(thread_context *thread, game_memory *memory, game_input *input, game_offscreen_buffer *buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context *thread, game_memory *memory, game_sound_output_buffer *soundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

inline game_controller_input *GetController(game_input *input, int unsigned controllerIndex)
{
    Assert(controllerIndex < ArrayCount(input->controllers));

    return &input->controllers[controllerIndex];
}
    
#ifdef __cplusplus
}
#endif
