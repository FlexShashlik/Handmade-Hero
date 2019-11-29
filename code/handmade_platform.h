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

//
// NOTE: Types
//
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>

typedef uint8_t ui8;
typedef uint16_t ui16;
typedef uint32_t ui32;
typedef uint64_t ui64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef i32 b32;

typedef size_t memory_index;

typedef float r32;
typedef double r64;

#define R32MAX FLT_MAX;

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

#if HANDMADE_SLOW
#define Assert(expression) if(!(expression)) { *(int *)0 = 0; }
#else
#define Assert(expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath")

#define Kilobytes(value) ((value) * 1024LL)
#define Megabytes(value) (Kilobytes(value) * 1024LL)
#define Gigabytes(value) (Megabytes(value) * 1024LL)
#define Terabytes(value) (Gigabytes(value) * 1024LL)

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

inline ui32
SafeTruncateUI64(ui64 value)
{
    Assert(value <= 0xFFFFFFFF);
    ui32 result = (ui32)value;

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
    ui32 contentsSize;
    void *contents;
} debug_read_file_result;

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *thread, void *memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *thread, char *fileName)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name(thread_context *thread, char *fileName, ui32 memorySize, void *memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif

#define BITMAP_BYTES_PER_PIXEL 4
typedef struct game_offscreen_buffer
{
    // NOTE: Pixels are always 32-bits wide
    void *memory;
    i32 width;
    i32 height;
    i32 pitch;
} game_offscreen_buffer;

typedef struct game_sound_output_buffer
{
    i32 samplesPerSecond;
    i32 sampleCount;
    i16 *samples;
} game_sound_output_buffer;

typedef struct game_button_state
{
    i32 halfTransitionCount;
    b32 endedDown;
} game_button_state;

typedef struct game_controller_input
{
    b32 isConnected;
    b32 isAnalog;
        
    r32 stickAverageX;
    r32 stickAverageY;
    
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
    i32 mouseX;
    i32 mouseY;
    i32 mouseZ;

    b32 executableReloaded;
    r32 deltaTime;
    
    game_controller_input controllers[5];
} game_input;

typedef struct game_memory
{
    b32 isInitialized;
    
    ui64 permanentStorageSize;
    void *permanentStorage; // NOTE: REQUIRED to be cleared to zero at startup

    ui64 transientStorageSize;
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
