#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>
#include <malloc.h>

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

#include "handmade.cpp"
#include "win32_handmade.h"

global_variable bool IsRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER SecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

// NOTE: XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE: XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// NOTE: DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal debug_read_file_result
DEBUGPlatformReadEntireFile(char *fileName)
{
    debug_read_file_result result = {};
    
    HANDLE fileHandle = CreateFile
        (
            fileName,
            GENERIC_READ,
            FILE_SHARE_READ,
            0,
            OPEN_EXISTING,
            0,
            0
        );

    if(fileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSize;
        if(GetFileSizeEx(fileHandle, &fileSize))
        {
            uint32 fileSize32 = SafeTruncateUInt64(fileSize.QuadPart);
            result.contents = VirtualAlloc
                (
                    0,
                    fileSize32,
                    MEM_RESERVE|MEM_COMMIT,
                    PAGE_READWRITE
                );
            
            if(result.contents)
            {
                DWORD bytesRead;
                if(ReadFile(fileHandle, result.contents, fileSize32, &bytesRead, 0) &&
                   (fileSize32 == bytesRead))
                {
                    // NOTE: File read successfully
                    result.contentsSize = fileSize32;
                }
                else
                {
                    DEBUGPlatformFreeFileMemory(result.contents);
                    result.contents = 0;
                }
            }
            else
            {
                // TODO: logging
            }
        }
        else
        {
            // TODO: logging
        }

        CloseHandle(fileHandle);
    }
    else
    {
        // TODO: logging
    }

    return result;
}
    
internal void
DEBUGPlatformFreeFileMemory(void *memory)
{
    if(memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

internal bool32
DEBUGPlatformWriteEntireFile(char *fileName, uint32 memorySize, void *memory)
{
    bool32 result = false;
    
    HANDLE fileHandle = CreateFile
        (
            fileName,
            GENERIC_WRITE,
            0,
            0,
            CREATE_ALWAYS,
            0,
            0
        );

    if(fileHandle != INVALID_HANDLE_VALUE)
    {       
        DWORD bytesWritten;
        if(WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0))
        {
            // NOTE: File read successfully
            result = (memorySize == bytesWritten);
        }
        else
        {
            // TODO: logging
        }

        CloseHandle(fileHandle);
    }
    else
    {
        // TODO: logging
    }

    return result;
}

internal void
Win32LoadXInput(void)
{
    HMODULE xInputLibrary = LoadLibrary("xinput1_4.dll");

    if(!xInputLibrary)
    {
        xInputLibrary = LoadLibrary("xinput9_1_0.dll");
    }
    
    if(!xInputLibrary)
    {
        xInputLibrary = LoadLibrary("xinput1_3.dll");
    }

    if(xInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(xInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(xInputLibrary, "XInputSetState");
    }
}

internal void
Win32InitDSound(HWND window, int32 samplesPerSecond, int32 bufferSize)
{
    // NOTE: Load the library
    HMODULE dSoundLibrary = LoadLibrary("dsound.dll");

    if(dSoundLibrary)
    {
        // NOTE: Get a DirectSound object - cooperative
        direct_sound_create *directSoundCreate = (direct_sound_create *)GetProcAddress(dSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND directSound;
        if(directSoundCreate && SUCCEEDED(directSoundCreate(0, &directSound, 0)))
        {
            WAVEFORMATEX waveFormat = {};
            waveFormat.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat.nChannels = 2;
            waveFormat.nSamplesPerSec = samplesPerSecond;
            waveFormat.wBitsPerSample = 16;
            waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
            
            if(SUCCEEDED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC bufferDescription = {};
                bufferDescription.dwSize = sizeof(bufferDescription);
                bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                // NOTE: "Create" a primary buffer
                LPDIRECTSOUNDBUFFER primaryBuffer;                
                if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0)))
                {                  
                    if(SUCCEEDED(primaryBuffer->SetFormat(&waveFormat)))
                    {
                        // NOTE: Finally set the format!
                        OutputDebugStringA("Primary buffer was set!\n");
                    }
                    else
                    {
                        // TODO: logging
                    }
                }
                else
                {
                    // TODO: logging
                }
            }
            else
            {
                // TODO: logging
            }

            DSBUFFERDESC bufferDescription = {};
            bufferDescription.dwSize = sizeof(bufferDescription);
            bufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
            bufferDescription.dwBufferBytes = bufferSize;
            bufferDescription.lpwfxFormat = &waveFormat;
            
            // NOTE: "Create" a secondary buffer                
            if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &SecondaryBuffer, 0)))
            {
                OutputDebugStringA("Secondary buffer created successfully!\n");
            }
            else
            {
                // TODO: logging
            }          
        }
        else
        {
            // TODO: logging
        }
    }
    else
    {
        // TODO: logging
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND window)
{
    win32_window_dimension result;
    
    RECT clientRect;
    GetClientRect(window, &clientRect);
    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;

    return result;
}               

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *buffer, int width, int height)
{
    if(buffer->memory)
    {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;

    buffer->bytesPerPixel = 4;
    
    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height; // for top-down bitmap
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    int bmpMemorySize = buffer->width * buffer->height * buffer->bytesPerPixel;
    buffer->memory = VirtualAlloc
        (
            0,
            bmpMemorySize,
            MEM_RESERVE|MEM_COMMIT,
            PAGE_READWRITE
        );
    
    buffer->pitch = buffer->width * buffer->bytesPerPixel;
}

internal void
Win32DisplayBufferInWindow
(
    win32_offscreen_buffer *buffer,
    HDC deviceContext,
    int windowWidth,
    int windowHeight
)
{
    StretchDIBits
        (
            deviceContext,
            0, 0, windowWidth, windowHeight,
            0, 0, buffer->width, buffer->height,
            buffer->memory,
            &buffer->info,
            DIB_RGB_COLORS,
            SRCCOPY
        );
}

internal LRESULT CALLBACK
Win32MainWindowCallback
(
  HWND   window,
  UINT   message,
  WPARAM wParam,
  LPARAM lParam
)
{
    LRESULT result = 0;
    
    switch(message)
    {
        case WM_SIZE:
        {
        } break;
        
        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");            
        } break;

        case WM_CLOSE:
        {
            IsRunning = false;
        } break;

        case WM_DESTROY:
        {
            IsRunning = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"Keyboard input came here!");
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);
            
            win32_window_dimension dimension = Win32GetWindowDimension(window);
         
            Win32DisplayBufferInWindow
                (
                    &GlobalBackbuffer,
                    deviceContext,
                    dimension.width,
                    dimension.height
                );
                                
            EndPaint(window, &paint);
        } break;
        
        default:
        {
            result = DefWindowProc
                (
                    window,
                    message,
                    wParam,
                    lParam
                );
        } break;
    }
    
    return result;
}

internal void
Win32ClearSoundBuffer(win32_sound_output *soundOutput)
{
    void *region1;
    DWORD region1Bytes;
    void *region2;
    DWORD region2Bytes;
    
    if
     (
         SUCCEEDED
         (
             SecondaryBuffer->Lock
             (
                 0,
                 soundOutput->secondaryBufferSize,
                 &region1, &region1Bytes,
                 &region2, &region2Bytes,
                 0
             )
         )
     )
    {
        uint8 *destSample = (uint8 *)region1;
        for(DWORD byteIndex = 0; byteIndex < region1Bytes; byteIndex++)
        {            
            *destSample++ = 0;
        }

        destSample = (uint8 *)region2;
        for(DWORD byteIndex = 0; byteIndex < region2Bytes; byteIndex++)
        {            
            *destSample++ = 0;
        }

        SecondaryBuffer->Unlock
            (
                region1, region1Bytes,
                region2, region2Bytes
            );
    }
}

internal void
Win32FillSoundBuffer
(
    win32_sound_output *soundOutput,
    DWORD byteToLock,
    DWORD bytesToWrite,
    game_sound_output_buffer *sourceBuffer
)
{
    void *region1;
    DWORD region1Bytes;
    void *region2;
    DWORD region2Bytes;
                
    if
     (
         SUCCEEDED
         (
             SecondaryBuffer->Lock
             (
                 byteToLock,
                 bytesToWrite,
                 &region1, &region1Bytes,
                 &region2, &region2Bytes,
                 0
             )
         )
     )
    {
        int16 *destSample = (int16 *)region1;
        int16 *sourceSample = sourceBuffer->samples;
        DWORD region1SamplesCount = region1Bytes / soundOutput->bytesPerSample;                        
        for(DWORD sampleIndex = 0; sampleIndex < region1SamplesCount; sampleIndex++)
        {            
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            
            soundOutput->runningSampleIndex++;
        }

        destSample = (int16 *)region2;
        DWORD region2SamplesCount = region2Bytes / soundOutput->bytesPerSample;                        
        for(DWORD sampleIndex = 0; sampleIndex < region2SamplesCount; sampleIndex++)
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
                        
            soundOutput->runningSampleIndex++;
        }

        SecondaryBuffer->Unlock
            (
                region1, region1Bytes,
                region2, region2Bytes
            );
    }
}

internal void
Win32ProcessKeyboardMessage
(
    game_button_state *newState,
    bool32 isDown
)
{
    Assert(newState->endedDown != isDown);
    
    newState->endedDown = isDown;
    newState->halfTransitionCount++;
}

internal void
Win32ProcessXInputDigitalButton
(
    DWORD xInputButtonState,
    game_button_state *oldState,
    DWORD buttonBit,
    game_button_state *newState
)
{
    newState->endedDown = (xInputButtonState & buttonBit) == buttonBit;
    newState->halfTransitionCount = (oldState->endedDown != newState->endedDown) ? 1 : 0;
}

internal real32
Win32ProcessXInputStickValue(SHORT value, SHORT deadZoneThreshold)
{
    real32 result = 0;
    if(value < -deadZoneThreshold)
    {
        result = (real32)(value + deadZoneThreshold) / (32768.0f - deadZoneThreshold);
    }
    else if(value > deadZoneThreshold)
    {
        result = (real32)(value - deadZoneThreshold) / (32767.0f - deadZoneThreshold);
    }

    return result;
}

internal void
Win32ProcessPendingMessage(game_controller_input *keyboardController)
{
    MSG message;
                    
    while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        if(message.message == WM_QUIT)
        {
            IsRunning = false;
        }

        switch(message.message)
        {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 virtualKeyCode = (uint32)message.wParam;
                bool wasDown = ((message.lParam & (1 << 30)) != 0);
                bool isDown = ((message.lParam & (1 << 31)) == 0);

                if(wasDown != isDown)
                {
                    if(virtualKeyCode == 'W')
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->moveUp,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == 'A')
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->moveLeft,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == 'S')
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->moveDown,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == 'D')
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->moveRight,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == 'Q')
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->leftShoulder,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == 'E')
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->rightShoulder,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->actionUp,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->actionLeft,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->actionRight,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->actionDown,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == VK_ESCAPE)
                    {               
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->start,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->back,
                                isDown
                            );
                    }                
                }

                bool32 altKeyWasDown = (message.lParam & (1 << 29));
                if((virtualKeyCode == VK_F4) && altKeyWasDown)
                {
                    IsRunning = false;
                }
            
            } break;

            default:
            {                         
                TranslateMessage(&message);
                DispatchMessage(&message);       
            } break;
        }                           
    }
}

inline LARGE_INTEGER
Win32GetWallClock(void)
{    
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);

    return result;
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    return (real32)(end.QuadPart - start.QuadPart) / (real32)GlobalPerfCountFrequency;   
}

internal void
Win32DebugDrawVertical(win32_offscreen_buffer *buffer, int x, int top, int bottom, uint32 color)
{
    uint8 *pixel = (uint8 *)buffer->memory + x * buffer->bytesPerPixel + top * buffer->pitch;
    for(int y = top; y < bottom; y++)
    {
        *(uint32 *)pixel = color;
        pixel += buffer->pitch;
    }
}

inline void
Win32DrawSoundBufferMarker
(
    win32_offscreen_buffer *buffer,
    win32_sound_output *soundOutput,
    real32 coefficient,
    int padX,
    int top, int bottom,
    DWORD value,
    uint32 color
)
{
    Assert(value < soundOutput->secondaryBufferSize);
        
    int x = padX + (int)(coefficient * (real32)value);
    Win32DebugDrawVertical(buffer, x, top, bottom, color);
}

internal void
Win32DebugSyncDisplay
(
    win32_offscreen_buffer *buffer,
    int markerCount,
    win32_debug_time_marker *markers,
    win32_sound_output *soundOutput,
    real32 targetSecondsPerFrame
)
{
    // TODO: Draw where we're writing out sound
    int padX = 16;
    int padY = 16;

    int top = padY;
    int bottom = buffer->height - padY;
    
    real32 coefficient = (real32)(buffer->width - 2 * padX) / (real32)soundOutput->secondaryBufferSize;
    for(int markerIndex = 0; markerIndex < markerCount; markerIndex++)
    {
        win32_debug_time_marker *thisMarker = &markers[markerIndex];

        Win32DrawSoundBufferMarker
            (
                buffer,
                soundOutput,
                coefficient,
                padX,
                top, bottom,
                thisMarker->playCursor,
                0xFFFFFFFF
            );

        Win32DrawSoundBufferMarker
            (
                buffer,
                soundOutput,
                coefficient,
                padX,
                top, bottom,
                thisMarker->writeCursor,
                0xFFFF0000
            );
    }
}

int
CALLBACK WinMain
(
    HINSTANCE instance,
    HINSTANCE prevInstance,
    LPSTR cmdLine,
    int cmdShow
)
{
    LARGE_INTEGER perfCountFrequencyResult;
    QueryPerformanceFrequency(&perfCountFrequencyResult);
    GlobalPerfCountFrequency = perfCountFrequencyResult.QuadPart;

    // NOTE: Set the Windows scheduler granularity
    // so that our Sleep() can be more granular
    bool32 sleepIsGranular = (timeBeginPeriod(1) == TIMERR_NOERROR);
    
    Win32LoadXInput();
    
    WNDCLASS windowClass = {};

    Win32ResizeDIBSection
        (
            &GlobalBackbuffer,
            1280,
            720
        );
    
    windowClass.style = CS_HREDRAW|CS_VREDRAW;
    windowClass.lpfnWndProc = Win32MainWindowCallback;
    windowClass.hInstance = instance;
    // windowClass.hIcon = ;
    windowClass.lpszClassName = "HandmadeHeroWindowClass";

#define framesOfAudioLatency 3
#define monitorRefreshHz 60
#define gameUpdateHz (monitorRefreshHz / 2)
    
    real32 targetSecondsPerFrame = 1.0f / (real32) gameUpdateHz;

    if(RegisterClass(&windowClass))
    {
        HWND window = CreateWindowEx
            (
                0,
                windowClass.lpszClassName,
                "Handmade Hero",
                WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0, 0, instance, 0
             );

        if(window)
        {            
            HDC deviceContext = GetDC(window);            
            win32_sound_output soundOutput = {};
            
            // NOTE: Sound test
            soundOutput.samplesPerSecond = 48000;
            soundOutput.bytesPerSample = sizeof(int16) * 2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.latencySampleCount = framesOfAudioLatency * (soundOutput.samplesPerSecond / gameUpdateHz);
            
            Win32InitDSound(window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
            
            Win32ClearSoundBuffer(&soundOutput);
            SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            IsRunning = true;

#if 0
            // NOTE: This tests the PC/WC update frequency
            while(IsRunning)
            {
                DWORD playCursor;
                DWORD writeCursor;
                SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor);
                
                char textBuffer[256];
                sprintf_s(textBuffer, sizeof(textBuffer), "PC: %u WC: %u\n", playCursor, writeCursor);
                OutputDebugStringA(textBuffer);
            }
#endif
                
            int16 *samples = (int16 *)VirtualAlloc
                (
                    0,
                    soundOutput.secondaryBufferSize,
                    MEM_RESERVE|MEM_COMMIT,
                    PAGE_READWRITE
                );

#if HANDMADE_INTERNAL
            LPVOID baseAddress = (LPVOID)Terabytes((uint64)2);
#else
            LPVOID baseAddress = 0;
#endif
            
            game_memory gameMemory = {};
            gameMemory.permanentStorageSize = Megabytes(64);
            gameMemory.transientStorageSize = Gigabytes(1);

            uint64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
            
            gameMemory.permanentStorage = VirtualAlloc
                (
                    baseAddress,
                    (size_t)totalSize,
                    MEM_RESERVE|MEM_COMMIT,
                    PAGE_READWRITE
                );            
            gameMemory.transientStorage = (uint8 *)gameMemory.permanentStorage + gameMemory.permanentStorageSize;

            if(samples && gameMemory.permanentStorage && gameMemory.transientStorage)
            {
                game_input input[2] = {};
                game_input *newInput = &input[0];
                game_input *oldInput = &input[1];
                
                LARGE_INTEGER lastCounter = Win32GetWallClock();

                int debugTimeMarkersIndex = 0;
                win32_debug_time_marker debugTimeMarkers[gameUpdateHz / 2] = {};

                DWORD lastPlayCursor = 0;
                bool32 soundIsValid = false;
                
                uint64 lastCycleCount = __rdtsc();
                while(IsRunning)
                {
                    // TODO: Zeroing macro
                    game_controller_input *oldKeyboardController = GetController(oldInput, 0);
                    game_controller_input *newKeyboardController = GetController(newInput, 0);

                    *newKeyboardController = {};
                    newKeyboardController->isConnected = true;

                    for(int buttonIndex = 0; buttonIndex < ArrayCount(oldKeyboardController->buttons); buttonIndex++)
                    {
                        newKeyboardController->buttons[buttonIndex].endedDown = oldKeyboardController->buttons[buttonIndex].endedDown;
                    }
                    
                    Win32ProcessPendingMessage(newKeyboardController);                 

                    DWORD maxControllerCount = XUSER_MAX_COUNT;
                    if(maxControllerCount > ArrayCount(newInput->controllers) - 1)
                    {
                        maxControllerCount = ArrayCount(newInput->controllers) - 1;
                    }
                
                    for(DWORD controllerIndex = 0; controllerIndex < maxControllerCount; controllerIndex++)
                    {
                        DWORD ourControllerIndex = controllerIndex + 1;
                        game_controller_input *oldController = GetController(oldInput, ourControllerIndex);
                        game_controller_input *newController = GetController(newInput, ourControllerIndex);
                    
                        XINPUT_STATE controllerState;                    
                        if(XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
                        {
                            newController->isConnected = true;
                            
                            // NOTE: this controller is plugged in
                            XINPUT_GAMEPAD *pad = &controllerState.Gamepad;
                                                   
                            newController->stickAverageX = Win32ProcessXInputStickValue
                                (
                                    pad->sThumbLX,
                                    XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
                                );
                            
                            newController->stickAverageY = Win32ProcessXInputStickValue
                                (
                                    pad->sThumbLY,
                                    XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
                                );

                            if
                                (
                                    newController->stickAverageX != 0.0f ||
                                    newController->stickAverageY != 0.0f
                                )
                            {
                                newController->isAnalog = true;
                            }

                            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                            {
                                newController->stickAverageY = 1;
                                newController->isAnalog = false;
                            }
                            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            {
                                newController->stickAverageY = -1;
                                newController->isAnalog = false;
                            }
                            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            {
                                newController->stickAverageX = -1;
                                newController->isAnalog = false;
                            }
                            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            {
                                newController->stickAverageX = 1;
                                newController->isAnalog = false;
                            }
                            
                            real32 threshold = 0.5f;

                            Win32ProcessXInputDigitalButton
                                (
                                    (newController->stickAverageY > threshold) ? 1 : 0,
                                    &oldController->moveUp,
                                    1,
                                    &newController->moveUp
                                );

                            Win32ProcessXInputDigitalButton
                                (
                                    (newController->stickAverageY < -threshold) ? 1 : 0,
                                    &oldController->moveDown,
                                    1,
                                    &newController->moveDown
                                );

                            Win32ProcessXInputDigitalButton
                                (
                                    (newController->stickAverageX < -threshold) ? 1 : 0,
                                    &oldController->moveLeft,
                                    1,
                                    &newController->moveLeft
                                );

                            Win32ProcessXInputDigitalButton
                                (
                                    (newController->stickAverageX > threshold) ? 1 : 0,
                                    &oldController->moveRight,
                                    1,
                                    &newController->moveRight
                                );

                            Win32ProcessXInputDigitalButton
                                (
                                    pad->wButtons,
                                    &oldController->actionUp,
                                    XINPUT_GAMEPAD_Y,
                                    &newController->actionUp
                                );

                            Win32ProcessXInputDigitalButton
                                (
                                    pad->wButtons,
                                    &oldController->actionDown,
                                    XINPUT_GAMEPAD_A,
                                    &newController->actionDown
                                );

                            Win32ProcessXInputDigitalButton
                                (
                                    pad->wButtons,
                                    &oldController->actionLeft,
                                    XINPUT_GAMEPAD_X,
                                    &newController->actionLeft
                                );
                            
                            Win32ProcessXInputDigitalButton
                                (
                                    pad->wButtons,
                                    &oldController->actionRight,
                                    XINPUT_GAMEPAD_B,
                                    &newController->actionRight
                                );
                            
                            Win32ProcessXInputDigitalButton
                                (
                                    pad->wButtons,
                                    &oldController->leftShoulder,
                                    XINPUT_GAMEPAD_LEFT_SHOULDER,
                                    &newController->leftShoulder
                                );
                            
                            Win32ProcessXInputDigitalButton
                                (
                                    pad->wButtons,
                                    &oldController->rightShoulder,
                                    XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                    &newController->rightShoulder
                                );

                            Win32ProcessXInputDigitalButton
                                (
                                    pad->wButtons,
                                    &oldController->start,
                                    XINPUT_GAMEPAD_START,
                                    &newController->start
                                );

                            Win32ProcessXInputDigitalButton
                                (
                                    pad->wButtons,
                                    &oldController->back,
                                    XINPUT_GAMEPAD_BACK,
                                    &newController->back
                                );
                        }
                        else
                        {
                            // NOTE: The controller isn't available
                            newController->isConnected = false;
                        }
                    }

                    // NOTE: Compute how much sound and where
                    DWORD byteToLock = 0;
                    DWORD targetCursor = 0;
                    DWORD bytesToWrite = 0;                
                    if(soundIsValid)
                    {
                        byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;                    
                    
                        targetCursor = (lastPlayCursor + soundOutput.latencySampleCount * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
                    
                        if(byteToLock > targetCursor)
                        {
                            bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
                            bytesToWrite += targetCursor;
                        }
                        else
                        {
                            bytesToWrite = targetCursor - byteToLock;
                        }
                    }

                    game_offscreen_buffer buffer = {};
                    buffer.memory = GlobalBackbuffer.memory;
                    buffer.width = GlobalBackbuffer.width;
                    buffer.height = GlobalBackbuffer.height;
                    buffer.pitch = GlobalBackbuffer.pitch;

                    game_sound_output_buffer soundBuffer = {};
                    soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
                    soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                    soundBuffer.samples = samples;
                
                    GameUpdateAndRender
                        (
                            &gameMemory,
                            newInput,
                            &buffer,
                            &soundBuffer
                         );
                
                    // NOTE: DirectSound output test
                    if(soundIsValid)
                    {
#if HANDMADE_INTERNAL
                        DWORD playCursor;
                        DWORD writeCursor;
                        SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor);
                    
                        char textBuffer[256];
                        sprintf_s(textBuffer, sizeof(textBuffer), "LPC: %u BTL: %u TC: %u BTW: %u - PC: %u WC: %u\n", lastPlayCursor, byteToLock, targetCursor, bytesToWrite, playCursor, writeCursor);
                        OutputDebugStringA(textBuffer);
#endif
                        
                        Win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
                    }               
                                                                     
                    LARGE_INTEGER workCounter = Win32GetWallClock();

                    real32 workSecondsElapsed = Win32GetSecondsElapsed(lastCounter, workCounter);
                    real32 secondsElapsedForFrame = workSecondsElapsed;
                    if(secondsElapsedForFrame < targetSecondsPerFrame)
                    {
                        if(sleepIsGranular)
                        {
                            DWORD sleepMS = 1000 * (DWORD)(targetSecondsPerFrame - workSecondsElapsed);

                            if(sleepMS > 0)
                            {
                                Sleep(sleepMS);
                            }
                        }

                        real32 testElapsedTime = Win32GetSecondsElapsed(lastCounter, Win32GetWallClock());

                        Assert(testElapsedTime < targetSecondsPerFrame);
                        
                        while(secondsElapsedForFrame < targetSecondsPerFrame)
                        {
                            secondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter, Win32GetWallClock());
                        }
                    }
                    else
                    {
                        // TODO: Missed frame
                        // TODO: Logging
                    }

                    LARGE_INTEGER endCounter = Win32GetWallClock();
                    real64 msPerFrame = 1000.0f * Win32GetSecondsElapsed(lastCounter, endCounter);
                    lastCounter = endCounter;
                    
                    win32_window_dimension dimension = Win32GetWindowDimension(window);
#if HANDMADE_INTERNAL
                    Win32DebugSyncDisplay
                        (
                            &GlobalBackbuffer,
                            ArrayCount(debugTimeMarkers),
                            debugTimeMarkers,
                            &soundOutput,
                            targetSecondsPerFrame
                        );
#endif
                    Win32DisplayBufferInWindow
                        (
                            &GlobalBackbuffer,
                            deviceContext,
                            dimension.width,
                            dimension.height
                         );

                    DWORD playCursor;
                    DWORD writeCursor;
                    if(SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
                    {
                        lastPlayCursor = playCursor;

                        if(!soundIsValid)
                        {
                            soundOutput.runningSampleIndex = writeCursor / soundOutput.bytesPerSample;
                            soundIsValid = true;
                        }
                    }
                    else
                    {
                        soundIsValid = false;
                    }

#if HANDMADE_INTERNAL
                    {                        
                        win32_debug_time_marker *marker = &debugTimeMarkers[debugTimeMarkersIndex++];
                        if(debugTimeMarkersIndex > ArrayCount(debugTimeMarkers))
                        {
                            debugTimeMarkersIndex = 0;
                        }

                        marker->playCursor = playCursor;
                        marker->writeCursor = writeCursor;
                    }
#endif
                    
                    uint64 endCycleCount = __rdtsc();
                    uint64 cyclesElapsed = endCycleCount - lastCycleCount;
                    lastCycleCount = endCycleCount;
                    
                    real64 fps = 0;
                    real64 mcpf = (real64)cyclesElapsed / (1000.0f * 1000.0f);

                    char fpsBuffer[256];
                    sprintf_s(fpsBuffer, sizeof(fpsBuffer), "%.02fms/f, %.02ff/s, %.02fmc/f\n", msPerFrame, fps, mcpf);
                    OutputDebugStringA(fpsBuffer);
                    
                    game_input *temp = newInput;
                    newInput = oldInput;
                    oldInput = temp;                   
                }
            }
            else
            {
                // TODO: logging
            }
        }
        else
        {
            // TODO: logging
        }
    }
    else
    {
        // TODO: logging
    }
    
    return 0;
}
