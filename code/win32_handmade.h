struct win32_offscreen_buffer
{
    BITMAPINFO info;
    void *memory;
    i32 width;
    i32 height;
    i32 pitch;
    i32 bytesPerPixel;
};

struct win32_window_dimension
{
    i32 width;
    i32 height;
};

struct win32_sound_output
{
    i32 samplesPerSecond;
    u32 runningSampleIndex;
    i32 bytesPerSample;
    DWORD secondaryBufferSize;
    DWORD safetyBytes;
};

struct win32_debug_time_marker
{
    DWORD outputPlayCursor;
    DWORD outputWriteCursor;
    DWORD outputLocation;
    DWORD outputByteCount;

    DWORD expectedFlipPlayCursor;
    
    DWORD flipPlayCursor;
    DWORD flipWriteCursor;
};

struct win32_game_code
{
    HMODULE gameCodeDLL;
    FILETIME lastDLLWriteTime;
    game_update_and_render *updateAndRender;
    game_get_sound_samples *getSoundSamples;

    b32 isValid;
};


// NOTE: Never use MAX_PATH in code, 'cuz it can be dangerous!
#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct win32_replay_buffer
{
    HANDLE fileHandle;
    HANDLE memoryMap;
    char fileName[WIN32_STATE_FILE_NAME_COUNT];
    void *memoryBlock;
};

struct win32_state
{
    u64 totalSize;
    void *gameMemoryBlock;
    win32_replay_buffer replayBuffers[4];
    
    HANDLE recordingHandle;
    int inputRecordingIndex;
    
    HANDLE playingHandle;
    int inputPlayingIndex;
    
    char exeFileName[WIN32_STATE_FILE_NAME_COUNT];
    char *onePastLastEXEFileSlash;
};
