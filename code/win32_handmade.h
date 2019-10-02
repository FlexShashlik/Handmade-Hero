struct win32_offscreen_buffer
{
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    int pitch;
    int bytesPerPixel;
};

struct win32_window_dimension
{
    int width;
    int height;
};

struct win32_sound_output
{
    int samplesPerSecond;
    uint32 runningSampleIndex;
    int bytesPerSample;
    DWORD secondaryBufferSize;
    real32 tSine;
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

    bool32 isValid;
};


// NOTE: Never use MAX_PATH in code, 'cuz it can be dangerous!
#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_state
{
    uint64 totalSize;
    void *gameMemoryBlock;
    
    HANDLE recordingHandle;
    int inputRecordingIndex;
    
    HANDLE playingHandle;
    int inputPlayingIndex;
    
    char exeFileName[WIN32_STATE_FILE_NAME_COUNT];
    char *onePastLastEXEFileSlash;
};
