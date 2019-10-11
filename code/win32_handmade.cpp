#include "handmade.h"

#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>
#include <malloc.h>

#include "win32_handmade.h"

global_variable bool32 IsRunning;
global_variable bool32 IsPause;
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

internal void
CatStrings
(
    size_t sourceASize, char *sourceA,
    size_t sourceBSize, char *sourceB,
    size_t destSize, char *dest
)
{
    for(int32 index = 0; index < sourceASize; index++)
    {
        *dest++ = *sourceA++;
    }

    for(int32 index = 0; index < sourceBSize; index++)
    {
        *dest++ = *sourceB++;
    }

    *dest++ = 0;
}

internal void
Win32GetEXEFileName(win32_state *state)
{
    DWORD sizeOfFileName = GetModuleFileNameA(0, state->exeFileName, sizeof(state->exeFileName));
    state->onePastLastEXEFileSlash = state->exeFileName;
    for(char *scan = state->exeFileName; *scan; scan++)
    {
        if(*scan == '\\')
        {
            state->onePastLastEXEFileSlash = scan + 1;
        }
    }
}

internal int
StringLength(char *string)
{
    int32 count = 0;
    while(*string++)
    {
        count++;
    }

    return count;
}

internal void Win32BuildEXEPathFileName
(
    win32_state *state, char *fileName,
    int32 destCount, char *dest
)
{
    CatStrings
        (
            state->onePastLastEXEFileSlash - state->exeFileName, state->exeFileName,
            StringLength(fileName), fileName,
            destCount, dest
        );
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if(memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result result = {};
    
    HANDLE fileHandle = CreateFileA
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
                    DEBUGPlatformFreeFileMemory(thread, result.contents);
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

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 result = false;
    
    HANDLE fileHandle = CreateFileA
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

inline FILETIME
Win32GetLastWriteTime(char *fileName)
{
    FILETIME lastWriteTime = {};
    WIN32_FILE_ATTRIBUTE_DATA data;
    if(GetFileAttributesExA(fileName, GetFileExInfoStandard, &data))
    {
        lastWriteTime = data.ftLastWriteTime;
    }
    
    return lastWriteTime;
}

internal win32_game_code
Win32LoadGameCode
(
    char *sourceDLLName,
    char *tempDLLName,
    char *lockFileName
)
{
    win32_game_code result = {};

    WIN32_FILE_ATTRIBUTE_DATA ignored;
    if(!GetFileAttributesExA(lockFileName, GetFileExInfoStandard, &ignored))
    {
        result.lastDLLWriteTime = Win32GetLastWriteTime(sourceDLLName);

        CopyFile(sourceDLLName, tempDLLName, FALSE);
        result.gameCodeDLL = LoadLibraryA(tempDLLName);
        if(result.gameCodeDLL)
        {
            result.updateAndRender = (game_update_and_render *)GetProcAddress(result.gameCodeDLL, "GameUpdateAndRender");
            result.getSoundSamples = (game_get_sound_samples *)GetProcAddress(result.gameCodeDLL, "GameGetSoundSamples");

            result.isValid = result.updateAndRender && result.getSoundSamples;
        }
    }

    if(!result.isValid)
    {  
        result.updateAndRender = 0;
        result.getSoundSamples = 0;
    }

    return result;
}

internal void
Win32UnloadGameCode(win32_game_code *gameCode)
{
    if(gameCode->gameCodeDLL)
    {
        FreeLibrary(gameCode->gameCodeDLL);
        gameCode->gameCodeDLL = 0;
    }

    gameCode->updateAndRender = 0;
    gameCode->getSoundSamples = 0;

    gameCode->isValid = false;
}

internal void
Win32LoadXInput(void)
{
    HMODULE xInputLibrary = LoadLibraryA("xinput1_4.dll");

    if(!xInputLibrary)
    {
        xInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }
    
    if(!xInputLibrary)
    {
        xInputLibrary = LoadLibraryA("xinput1_3.dll");
    }

    if(xInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(xInputLibrary, "XInputGetState");
        if(!XInputGetState) { XInputGetState = XInputGetStateStub; }

        XInputSetState = (x_input_set_state *)GetProcAddress(xInputLibrary, "XInputSetState");
        if(!XInputSetState) { XInputSetState = XInputSetStateStub; }
    }
}

internal void
Win32InitDSound(HWND window, int32 samplesPerSecond, int32 bufferSize)
{
    // NOTE: Load the library
    HMODULE dSoundLibrary = LoadLibraryA("dsound.dll");

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
Win32ResizeDIBSection(win32_offscreen_buffer *buffer, int32 width, int32 height)
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

    int32 bmpMemorySize = buffer->width * buffer->height * buffer->bytesPerPixel;
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
Win32DisplayBufferInWindow(win32_offscreen_buffer *buffer,
                           HDC deviceContext,
                           int32 windowWidth,
                           int32 windowHeight)
{
    int32 offsetX = 10;
    int32 offsetY = 10;
    
    PatBlt(deviceContext, 0, 0, windowWidth, offsetY, BLACKNESS);
    PatBlt(deviceContext, 0, offsetY + buffer->height, windowWidth, windowHeight, BLACKNESS);
    PatBlt(deviceContext, 0, 0, offsetX, windowHeight, BLACKNESS);
    PatBlt(deviceContext, offsetX + buffer->width, 0, windowWidth, windowHeight, BLACKNESS);
    
    // NOTE: Always blit 1-to-1 pixel for debug
    StretchDIBits
        (
            deviceContext,
            offsetX, offsetY, buffer->width, buffer->height,
            0, 0, buffer->width, buffer->height,
            buffer->memory,
            &buffer->info,
            DIB_RGB_COLORS,
            SRCCOPY
        );
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND   window,
                        UINT   message,
                        WPARAM wParam,
                        LPARAM lParam)
{
    LRESULT result = 0;
    
    switch(message)
    {
        case WM_ACTIVATEAPP:
        {
            /*if(wParam)
            {
                SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_ALPHA);
            }
            else
            {
                SetLayeredWindowAttributes(window, RGB(0, 0, 0), 128, LWA_ALPHA);
            }*/
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
            result = DefWindowProcA
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
    
    if(SUCCEEDED(SecondaryBuffer->Lock(0,
                                       soundOutput->secondaryBufferSize,
                                       &region1, &region1Bytes,
                                       &region2, &region2Bytes,
                                       0)))
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
                
    if(SUCCEEDED(SecondaryBuffer->Lock(byteToLock,
                                       bytesToWrite,
                                       &region1, &region1Bytes,
                                       &region2, &region2Bytes,
                                       0)))
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
    if(newState->endedDown != isDown)
    {
        newState->endedDown = isDown;
        newState->halfTransitionCount++;
    }
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
Win32GetInputFileLocation(win32_state *state, bool32 isInput, int32 slotIndex, int32 destCount, char *dest)
{
    char temp[64];
    wsprintf(temp, "loop_edit_%d_%s.hmi", slotIndex, isInput ? "input" : "state");
    Win32BuildEXEPathFileName(state, temp, destCount, dest);
}

internal win32_replay_buffer *
Win32GetReplayBuffer(win32_state *state, uint32 index)
{
    Assert(index < ArrayCount(state->replayBuffers));
    win32_replay_buffer *result = &state->replayBuffers[index];

    return result;
}

internal void
Win32BeginRecordingInput(win32_state *state, int32 inputRecordingIndex)
{
    win32_replay_buffer *replayBuffer = Win32GetReplayBuffer(state, inputRecordingIndex);
    if(replayBuffer->memoryBlock)
    {
        state->inputRecordingIndex = inputRecordingIndex;

        char fileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation
                    (
                        state,
                        true,
                        inputRecordingIndex,
                        sizeof(fileName),
                        fileName
                    );
        
        state->recordingHandle = CreateFileA
                    (
                        fileName,
                        GENERIC_WRITE,
                        0,
                        0,
                        CREATE_ALWAYS,
                        0,
                        0
                    );

#if 0
        LARGE_INTEGER filePos;
        filePos.QuadPart = state->totalSize;
        SetFilePointerEx(state->recordingHandle, filePos, 0, FILE_BEGIN);
#endif
        
        CopyMemory(replayBuffer->memoryBlock, state->gameMemoryBlock, state->totalSize);
    }
}

internal void
Win32EndRecordingInput(win32_state *state)
{
    CloseHandle(state->recordingHandle);
    state->inputRecordingIndex = 0;
}

internal void
Win32RecordInput(win32_state *state, game_input *newInput)
{
    DWORD bytesWritten;
    WriteFile(state->recordingHandle, newInput, sizeof(*newInput), &bytesWritten, 0);
}

internal void
Win32BeginPlayBackInput(win32_state *state, int32 inputPlayingIndex)
{
    win32_replay_buffer *replayBuffer = Win32GetReplayBuffer(state, inputPlayingIndex);
            
    if(replayBuffer->memoryBlock)
    {
        state->inputPlayingIndex = inputPlayingIndex;

        char fileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation
                    (
                        state,
                        true,
                        inputPlayingIndex,
                        sizeof(fileName),
                        fileName
                    );
        
        state->playingHandle = CreateFileA
                    (
                        fileName,
                        GENERIC_READ,
                        0,
                        0,
                        OPEN_EXISTING,
                        0,
                        0
                    );

#if 0
        LARGE_INTEGER filePos;
        filePos.QuadPart = state->totalSize;
        SetFilePointerEx(state->playingHandle, filePos, 0, FILE_BEGIN);
#endif
        
        CopyMemory(state->gameMemoryBlock, replayBuffer->memoryBlock, state->totalSize);
    }
}

internal void
Win32EndPlayBackInput(win32_state *state)
{
    CloseHandle(state->playingHandle);
    state->inputPlayingIndex = 0;
}

internal void
Win32PlayBackInput(win32_state *state, game_input *newInput)
{
    DWORD bytesRead = 0;
    if(ReadFile(state->playingHandle, newInput, sizeof(*newInput), &bytesRead, 0))
    {
        if(bytesRead == 0)
        {
            // NOTE: The end of the stream
            int32 playingIndex = state->inputPlayingIndex;
        
            Win32EndPlayBackInput(state);
            Win32BeginPlayBackInput(state, playingIndex);
            ReadFile(state->playingHandle, newInput, sizeof(*newInput), &bytesRead, 0);
        }
    }
}

internal void
Win32ProcessPendingMessage(win32_state *state, game_controller_input *keyboardController)
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
                bool32 wasDown = ((message.lParam & (1 << 30)) != 0);
                bool32 isDown = ((message.lParam & (1 << 31)) == 0);

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
#if HANDMADE_INTERNAL
                    else if(virtualKeyCode == 'P')
                    {
                        if(isDown)
                        {
                            IsPause = !IsPause;
                        }
                    }
                    else if(virtualKeyCode == 'L')
                    {
                        if(isDown)
                        {
                            if(state->inputPlayingIndex == 0)
                            {
                                if(state->inputRecordingIndex == 0)
                                {
                                    Win32BeginRecordingInput(state, 1);
                                }
                                else
                                {
                                    Win32EndRecordingInput(state);
                                    Win32BeginPlayBackInput(state, 1);
                                }
                            }
                            else
                            {
                                Win32EndPlayBackInput(state);
                            }
                        }
                    }
#endif
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

#if 0
internal void
Win32DebugDrawVertical(win32_offscreen_buffer *buffer, int32 x, int32 top, int32 bottom, uint32 color)
{
    if(top < 0)
    {
        top = 0;
    }

    if(bottom > buffer->height)
    {
        bottom = buffer->height;
    }
    
    if(x >= 0 && x <= buffer->width)
    {
        uint8 *pixel = (uint8 *)buffer->memory + x * buffer->bytesPerPixel + top * buffer->pitch;
        for(int32 y = top; y < bottom; y++)
        {
            *(uint32 *)pixel = color;
            pixel += buffer->pitch;
        }
    }
}

inline void
Win32DrawSoundBufferMarker
(
    win32_offscreen_buffer *buffer,
    win32_sound_output *soundOutput,
    real32 coefficient,
    int32 padX,
    int32 top, int32 bottom,
    DWORD value,
    uint32 color
)
{
    int32 x = padX + (int)(coefficient * (real32)value);    
    Win32DebugDrawVertical(buffer, x, top, bottom, color);
}

internal void
Win32DebugSyncDisplay
(
    win32_offscreen_buffer *buffer,
    int32 markerCount,
    win32_debug_time_marker *markers,
    int32 currentMarkerIndex,
    win32_sound_output *soundOutput,
    real32 targetSecondsPerFrame
)
{
    int32 padX = 16;
    int32 padY = 16;

    int32 lineHeight = 64;
    
    real32 coefficient = (real32)(buffer->width - 2 * padX) / (real32)soundOutput->secondaryBufferSize;
    for(int32 markerIndex = 0; markerIndex < markerCount; markerIndex++)
    {
        win32_debug_time_marker *thisMarker = &markers[markerIndex];

        Assert(thisMarker->outputPlayCursor < soundOutput->secondaryBufferSize);
        Assert(thisMarker->outputWriteCursor < soundOutput->secondaryBufferSize);
        Assert(thisMarker->outputLocation < soundOutput->secondaryBufferSize);
        Assert(thisMarker->outputByteCount < soundOutput->secondaryBufferSize);
        Assert(thisMarker->flipPlayCursor < soundOutput->secondaryBufferSize);
        Assert(thisMarker->flipWriteCursor < soundOutput->secondaryBufferSize);        
        
        int32 top = padY;
        int32 bottom = padY + lineHeight;
            
        DWORD playColor = 0xFFFFFFFF;
        DWORD writeColor = 0xFFFF0000;
        DWORD expectedFlipColor = 0xFFFFFF00;
        DWORD playWindowColor = 0xFFFF00FF;
        
        if(markerIndex == currentMarkerIndex)
        {
            top += lineHeight + padY;
            bottom += lineHeight + padY;

            int32 firstTop = top;

            Win32DrawSoundBufferMarker
            (
                buffer,
                soundOutput,
                coefficient,
                padX,
                top, bottom,
                thisMarker->outputPlayCursor,
                playColor
            );

            Win32DrawSoundBufferMarker
            (
                buffer,
                soundOutput,
                coefficient,
                padX,
                top, bottom,
                thisMarker->outputWriteCursor,
                writeColor
            );

            top += lineHeight + padY;
            bottom += lineHeight + padY;

            Win32DrawSoundBufferMarker
            (
                buffer,
                soundOutput,
                coefficient,
                padX,
                top, bottom,
                thisMarker->outputLocation,
                playColor
            );

            Win32DrawSoundBufferMarker
            (
                buffer,
                soundOutput,
                coefficient,
                padX,
                top, bottom,
                thisMarker->outputLocation + thisMarker->outputByteCount,
                writeColor
            );

            top += lineHeight + padY;
            bottom += lineHeight + padY;
            
            Win32DrawSoundBufferMarker
            (
                buffer,
                soundOutput,
                coefficient,
                padX,
                firstTop, bottom,
                thisMarker->expectedFlipPlayCursor,
                expectedFlipColor
            );
        }

        Win32DrawSoundBufferMarker
            (
                buffer,
                soundOutput,
                coefficient,
                padX,
                top, bottom,
                thisMarker->flipPlayCursor,
                playColor
            );
        
        Win32DrawSoundBufferMarker
            (
                buffer,
                soundOutput,
                coefficient,
                padX,
                top, bottom,
                thisMarker->flipPlayCursor + 480 * soundOutput->bytesPerSample,
                playWindowColor
            );

        Win32DrawSoundBufferMarker
            (
                buffer,
                soundOutput,
                coefficient,
                padX,
                top, bottom,
                thisMarker->flipWriteCursor,
                writeColor
            );
    }
}
#endif

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

    win32_state win32State = {};
    Win32GetEXEFileName(&win32State);
    
    char gameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName
        (
            &win32State, "handmade.dll",
            sizeof(gameCodeDLLFullPath), gameCodeDLLFullPath
        );
    
    char gameCodeTempDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName
        (
            &win32State, "handmade_temp.dll",
            sizeof(gameCodeTempDLLFullPath), gameCodeTempDLLFullPath
        );

    char gameCodeLockFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName
        (
            &win32State, "lock.tmp",
            sizeof(gameCodeLockFullPath), gameCodeLockFullPath
        );
    
    // NOTE: Set the Windows scheduler granularity
    // so that our Sleep() can be more granular
    bool32 sleepIsGranular = (timeBeginPeriod(1) == TIMERR_NOERROR);
    
    Win32LoadXInput();
    
    WNDCLASS windowClass = {};

    Win32ResizeDIBSection
        (
            &GlobalBackbuffer,
            960,
            540
        );
    
    windowClass.style = CS_HREDRAW|CS_VREDRAW;
    windowClass.lpfnWndProc = Win32MainWindowCallback;
    windowClass.hInstance = instance;
    // windowClass.hIcon = ;
    windowClass.lpszClassName = "HandmadeHeroWindowClass";
    
    if(RegisterClass(&windowClass))
    {
        HWND window = CreateWindowExA
            (
                /*WS_EX_TOPMOST|WS_EX_LAYERED*/0,
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
            win32_sound_output soundOutput = {};
            int32 monitorRefreshHz = 60;

            HDC refreshDC = GetDC(window);            
            int32 win32RefreshRate = GetDeviceCaps(refreshDC, VREFRESH);
            ReleaseDC(window, refreshDC);
            if(win32RefreshRate > 1)
            {
                monitorRefreshHz = win32RefreshRate;
            }
            
            real32 gameUpdateHz = monitorRefreshHz / 2.0f;            
            real32 targetSecondsPerFrame = 1.0f / (real32) gameUpdateHz;
            
            // NOTE: Sound test
            soundOutput.samplesPerSecond = 48000;
            soundOutput.bytesPerSample = sizeof(int16) * 2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.safetyBytes = (int)((real32)soundOutput.samplesPerSecond * (real32)soundOutput.bytesPerSample / gameUpdateHz / 2.0f);
                        
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
            LPVOID baseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID baseAddress = 0;
#endif
            
            game_memory gameMemory = {};
            gameMemory.permanentStorageSize = Megabytes(64);
            gameMemory.transientStorageSize = Gigabytes(1);
            
            gameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
            gameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
            gameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
            
            win32State.totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
            win32State.gameMemoryBlock = VirtualAlloc
                (
                    baseAddress,
                    (size_t)win32State.totalSize,
                    MEM_RESERVE|MEM_COMMIT,
                    PAGE_READWRITE
                );
            
            gameMemory.permanentStorage = win32State.gameMemoryBlock;            
            gameMemory.transientStorage = (uint8 *)gameMemory.permanentStorage + gameMemory.permanentStorageSize;
            
            for(int32 replayIndex = 0; replayIndex < ArrayCount(win32State.replayBuffers); replayIndex++)
            {
                win32_replay_buffer *replayBuffer = &win32State.replayBuffers[replayIndex];
                
                Win32GetInputFileLocation
                    (
                        &win32State,
                        false,
                        replayIndex,
                        sizeof(replayBuffer->fileName),
                        replayBuffer->fileName
                    );

                replayBuffer->fileHandle = CreateFileA
                    (
                        replayBuffer->fileName,
                        GENERIC_WRITE|GENERIC_READ,
                        0,
                        0,
                        CREATE_ALWAYS,
                        0,
                        0
                    );
                
                LARGE_INTEGER maxSize;
                maxSize.QuadPart = win32State.totalSize;
                
                replayBuffer->memoryMap = CreateFileMapping
                    (
                        replayBuffer->fileHandle,
                        0,
                        PAGE_READWRITE,
                        maxSize.HighPart,
                        maxSize.LowPart,
                        0
                    );
                                
                replayBuffer->memoryBlock = MapViewOfFile
                    (
                        replayBuffer->memoryMap,
                        FILE_MAP_ALL_ACCESS,
                        0, 0,
                        win32State.totalSize
                    ); 
                
                if(replayBuffer->memoryBlock)
                {
                }
                else
                {                    
                    // TODO: Diagnostic
                }
            }
            
            if(samples && gameMemory.permanentStorage && gameMemory.transientStorage)
            {
                game_input input[2] = {};
                game_input *newInput = &input[0];
                game_input *oldInput = &input[1];
                
                LARGE_INTEGER lastCounter = Win32GetWallClock();
                LARGE_INTEGER flipWallClock = Win32GetWallClock();

                int32 debugTimeMarkersIndex = 0;
                win32_debug_time_marker debugTimeMarkers[30] = {};
                
                bool32 isSoundValid = false;
                DWORD audioLatencyBytes = 0;
                real32 audioLatencySeconds = 0;
                
                win32_game_code gameCode = Win32LoadGameCode
                    (
                        gameCodeDLLFullPath,
                        gameCodeTempDLLFullPath,
                        gameCodeLockFullPath
                    );
                                                
                uint64 lastCycleCount = __rdtsc();
                while(IsRunning)
                {
                    newInput->deltaTime = targetSecondsPerFrame;
                    
                    FILETIME newDLLWriteTime = Win32GetLastWriteTime(gameCodeDLLFullPath);
                    if(CompareFileTime(&newDLLWriteTime, &gameCode.lastDLLWriteTime) != 0)
                    {
                        Win32UnloadGameCode(&gameCode);
                        gameCode = Win32LoadGameCode
                            (
                                gameCodeDLLFullPath,
                                gameCodeTempDLLFullPath,
                                gameCodeLockFullPath
                            );
                    }
                    
                    // TODO: Zeroing macro
                    game_controller_input *oldKeyboardController = GetController(oldInput, 0);
                    game_controller_input *newKeyboardController = GetController(newInput, 0);

                    *newKeyboardController = {};
                    newKeyboardController->isConnected = true;

                    for(int32 buttonIndex = 0; buttonIndex < ArrayCount(oldKeyboardController->buttons); buttonIndex++)
                    {
                        newKeyboardController->buttons[buttonIndex].endedDown = oldKeyboardController->buttons[buttonIndex].endedDown;
                    }
                    
                    Win32ProcessPendingMessage(&win32State, newKeyboardController);                 

                    if(!IsPause)
                    {
                        POINT mousePos;
                        GetCursorPos(&mousePos);
                        ScreenToClient(window, &mousePos);
                        
                        newInput->mouseX = mousePos.x;
                        newInput->mouseY = mousePos.y;
                        
                        // TODO: Support mouse wheel
                        newInput->mouseZ = 0;
                        
                        Win32ProcessKeyboardMessage
                            (
                                &newInput->mouseButtons[0],
                                GetKeyState(VK_LBUTTON) & (1 << 15)
                            );

                        Win32ProcessKeyboardMessage
                            (
                                &newInput->mouseButtons[1],
                                GetKeyState(VK_RBUTTON) & (1 << 15)
                            );

                        Win32ProcessKeyboardMessage
                            (
                                &newInput->mouseButtons[2],
                                GetKeyState(VK_MBUTTON) & (1 << 15)
                            );

                        Win32ProcessKeyboardMessage
                            (
                                &newInput->mouseButtons[3],
                                GetKeyState(VK_XBUTTON1) & (1 << 15)
                            );

                        Win32ProcessKeyboardMessage
                            (
                                &newInput->mouseButtons[4],
                                GetKeyState(VK_XBUTTON2) & (1 << 15)
                            );
                        
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
                                newController->isAnalog = oldController->isAnalog;
                            
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

                        thread_context thread = {};
                    
                        game_offscreen_buffer buffer = {};
                        buffer.memory = GlobalBackbuffer.memory;
                        buffer.width = GlobalBackbuffer.width;
                        buffer.height = GlobalBackbuffer.height;
                        buffer.pitch = GlobalBackbuffer.pitch;
                        buffer.bytesPerPixel = GlobalBackbuffer.bytesPerPixel;

                        if(win32State.inputRecordingIndex)
                        {
                            Win32RecordInput(&win32State, newInput);
                        }

                        if(win32State.inputPlayingIndex)
                        {
                            Win32PlayBackInput(&win32State, newInput);
                        }

                        if(gameCode.updateAndRender)
                        {
                            gameCode.updateAndRender
                                (
                                    &thread,
                                    &gameMemory,
                                    newInput,
                                    &buffer
                                );
                        }
                        
                        LARGE_INTEGER audioWallClock = Win32GetWallClock();
                        real32 deltaAudioSeconds = Win32GetSecondsElapsed(flipWallClock, audioWallClock);
                    
                        DWORD playCursor;
                        DWORD writeCursor;
                        if(SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
                        {
                            /*
                              NOTE:

                              Here is how sound output computation works :3

                              We define a safety value that is the number of
                              samples we think our game update loop may vary
                              by (up to 2ms).

                              When we wake up to write audio, we will look and
                              see what the play cursor position is and we will
                              forecast ahead where we think the play cursor
                              will be on the next frame boundary.

                              We will then look to see if the write cursor is
                              before that by at least our safety value. If it
                              is, the target fill position is that frame
                              boundary + one frame. This gives us perfect
                              audio sync in the case of a card that has low
                              enough latency.

                              If the write cursor is out after that safety
                              margin, then we assume we can never sync
                              perfectly, so we will write one frame's worth of
                              audio + the safety margin worth's of guard
                              samples.                      
                            */
                        
                            if(!isSoundValid)
                            {
                                soundOutput.runningSampleIndex = writeCursor / soundOutput.bytesPerSample;
                                isSoundValid = true;
                            }

                            DWORD byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;                    

                            DWORD expectedBytesPerFrame = (int)((real32)(soundOutput.samplesPerSecond * soundOutput.bytesPerSample) / gameUpdateHz);
                            real32 secondsLeftUntilFlip = targetSecondsPerFrame - deltaAudioSeconds;
                            DWORD expectedBytesUntilFlip = (DWORD)((secondsLeftUntilFlip / targetSecondsPerFrame) * (real32)expectedBytesPerFrame);
                            
                            DWORD expectedFrameBoundaryByte = playCursor + expectedBytesUntilFlip;
                        
                            DWORD safeWriteCursor = writeCursor;
                            if(safeWriteCursor < playCursor)
                            {
                                safeWriteCursor += soundOutput.secondaryBufferSize;
                            }

                            Assert(safeWriteCursor >= playCursor);
                            safeWriteCursor += soundOutput.safetyBytes;
                        
                            bool32 isAudioCardLowLatency = (safeWriteCursor < expectedFrameBoundaryByte);
                        
                            DWORD targetCursor = 0;                        
                            if(isAudioCardLowLatency)
                            {
                                targetCursor = expectedFrameBoundaryByte + expectedBytesPerFrame;
                            }
                            else
                            {                            
                                targetCursor = writeCursor + expectedBytesPerFrame + soundOutput.safetyBytes;
                            }
                        
                            targetCursor %= soundOutput.secondaryBufferSize;
                                                
                            DWORD bytesToWrite = 0;
                            if(byteToLock > targetCursor)
                            {
                                bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
                                bytesToWrite += targetCursor;
                            }
                            else
                            {
                                bytesToWrite = targetCursor - byteToLock;
                            }

                            game_sound_output_buffer soundBuffer = {};
                            soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
                            soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                            soundBuffer.samples = samples;

                            if(gameCode.getSoundSamples)
                            {
                                gameCode.getSoundSamples(&thread, &gameMemory, &soundBuffer);
                            }
                            
                            Win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
                    
#if HANDMADE_INTERNAL
                            win32_debug_time_marker *marker = &debugTimeMarkers[debugTimeMarkersIndex];
                            marker->outputPlayCursor = playCursor;
                            marker->outputWriteCursor = writeCursor;
                            marker->outputLocation = byteToLock;
                            marker->outputByteCount = bytesToWrite;
                            marker->expectedFlipPlayCursor = expectedFrameBoundaryByte;
                            
                            DWORD unwrappedWriteCursor = writeCursor;
                            if(unwrappedWriteCursor < playCursor)
                            {
                                unwrappedWriteCursor += soundOutput.secondaryBufferSize;
                            }
                        
                            audioLatencyBytes = unwrappedWriteCursor - playCursor;
                            audioLatencySeconds = (real32)audioLatencyBytes / (real32)soundOutput.bytesPerSample / (real32)soundOutput.samplesPerSecond;

#if 0
                            char textBuffer[256];
                            sprintf_s
                                (
                                    textBuffer, sizeof(textBuffer),
                                    "BTL: %u TC: %u BTW: %u - PC: %u WC: %u DELTA: %u (%fs)\n",
                                    byteToLock, targetCursor, bytesToWrite,
                                    playCursor, writeCursor,
                                    audioLatencyBytes, audioLatencySeconds
                                );
                        
                            OutputDebugStringA(textBuffer);
#endif
#endif                        
                        }
                        else
                        {
                            isSoundValid = false;
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

                            if(testElapsedTime < targetSecondsPerFrame)
                            {
                                // TODO: Log missed sleep
                            }
                        
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
                        real32 msPerFrame = 1000.0f * Win32GetSecondsElapsed(lastCounter, endCounter);
                        lastCounter = endCounter;
                    
                        win32_window_dimension dimension = Win32GetWindowDimension(window);

                        HDC deviceContext = GetDC(window);
                        Win32DisplayBufferInWindow
                            (
                                &GlobalBackbuffer,
                                deviceContext,
                                dimension.width,
                                dimension.height
                            );
                        ReleaseDC(window, deviceContext);
                        
                        flipWallClock = Win32GetWallClock();
                        
#if HANDMADE_INTERNAL
                        playCursor = 0;
                        writeCursor = 0;
                        if(SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
                        {
                            Assert(debugTimeMarkersIndex < ArrayCount(debugTimeMarkers));
                            win32_debug_time_marker *marker = &debugTimeMarkers[debugTimeMarkersIndex];
                        
                            marker->flipPlayCursor = playCursor;
                            marker->flipWriteCursor = writeCursor;
                        }
#endif
                    
                        game_input *temp = newInput;
                        newInput = oldInput;
                        oldInput = temp;
                        
#if 0
                        uint64 endCycleCount = __rdtsc();
                        uint64 cyclesElapsed = endCycleCount - lastCycleCount;
                        lastCycleCount = endCycleCount;
                    
                        real64 fps = 0;
                        real64 mcpf = (real64)cyclesElapsed / (1000.0f * 1000.0f);

                        char fpsBuffer[256];
                        sprintf_s(fpsBuffer, sizeof(fpsBuffer), "%.02fms/f, %.02ff/s, %.02fmc/f\n", msPerFrame, fps, mcpf);
                        OutputDebugStringA(fpsBuffer);
#endif
                        
#if HANDMADE_INTERNAL
                        debugTimeMarkersIndex++;
                        if(debugTimeMarkersIndex == ArrayCount(debugTimeMarkers))
                        {
                            debugTimeMarkersIndex = 0;
                        }
#endif
                    }
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
