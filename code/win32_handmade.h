struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

struct win32_sound_output
{
    int samplesPerSecond;
    uint32 runningSampleIndex;
    int bytesPerSample;
    int secondaryBufferSize;
    real32 tSine;
    int latencySampleCount;
};
