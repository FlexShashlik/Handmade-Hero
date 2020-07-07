#include "handmade_platform.h"

#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>
#include <malloc.h>

#include "win32_handmade.h"

global_variable b32 IsRunning;
global_variable b32 IsPause;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER SecondaryBuffer;
global_variable i64 GlobalPerfCountFrequency;
global_variable b32 DEBUGShowCursor;
global_variable WINDOWPLACEMENT WindowPos = {sizeof(WindowPos)};

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
    for(i32 index = 0; index < sourceASize; index++)
    {
        *dest++ = *sourceA++;
    }

    for(i32 index = 0; index < sourceBSize; index++)
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

internal i32
StringLength(char *string)
{
    i32 count = 0;
    while(*string++)
    {
        count++;
    }

    return count;
}

internal void Win32BuildEXEPathFileName
(
    win32_state *state, char *fileName,
    i32 destCount, char *dest
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
            ui32 fileSize32 = SafeTruncateUI64(fileSize.QuadPart);
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

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    b32 result = false;
    
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
Win32InitDSound(HWND window, i32 samplesPerSecond, i32 bufferSize)
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

            // TODO: In release mode, should we not specify DSBCAPS_GLOBALFOCUS?
            DSBUFFERDESC bufferDescription = {};
            bufferDescription.dwSize = sizeof(bufferDescription);
            bufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
#if HANDMADE_INTERNAL
            bufferDescription.dwFlags |= DSBCAPS_GLOBALFOCUS;
#endif
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
Win32ResizeDIBSection(win32_offscreen_buffer *buffer, i32 width, i32 height)
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
    buffer->info.bmiHeader.biHeight = buffer->height; // for bottom-up bitmap
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    buffer->pitch = Align16(buffer->width * buffer->bytesPerPixel);
    i32 bmpMemorySize = buffer->pitch * buffer->height;
    buffer->memory = VirtualAlloc
        (
            0,
            bmpMemorySize,
            MEM_RESERVE|MEM_COMMIT,
            PAGE_READWRITE
        );
}

internal void
Win32DisplayBufferInWindow
(
    win32_offscreen_buffer *buffer,
    HDC deviceContext,
    i32 windowWidth,
    i32 windowHeight
)
{
    if(windowWidth >= 1920 &&
       windowHeight >= 1080)
    {
        StretchDIBits
            (
                deviceContext,
                0, 0, 1920, 1080,
                0, 0, buffer->width, buffer->height,
                buffer->memory,
                &buffer->info,
                DIB_RGB_COLORS,
                SRCCOPY
            );
    }
    else
    {
#if 0
        i32 offsetX = 10;
        i32 offsetY = 10;
    
        PatBlt(deviceContext, 0, 0, windowWidth, offsetY, BLACKNESS);
        PatBlt(deviceContext, 0, offsetY + buffer->height, windowWidth, windowHeight, BLACKNESS);
        PatBlt(deviceContext, 0, 0, offsetX, windowHeight, BLACKNESS);
        PatBlt(deviceContext, offsetX + buffer->width, 0, windowWidth, windowHeight, BLACKNESS);
#else
        i32 offsetX = 0;
        i32 offsetY = 0;
#endif
        
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
}

internal LRESULT CALLBACK
Win32MainWindowCallback
(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
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

        case WM_SETCURSOR:
        {
            if(DEBUGShowCursor)
            {
                result = DefWindowProcA
                (
                    window,
                    message,
                    wParam,
                    lParam
                );
            }
            else
            {
                SetCursor(0);
            }
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
            Assert(!"Keyboard input came in through a non-dispatch message!");
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
        ui8 *destSample = (ui8 *)region1;
        for(DWORD byteIndex = 0; byteIndex < region1Bytes; byteIndex++)
        {            
            *destSample++ = 0;
        }

        destSample = (ui8 *)region2;
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
        i16 *destSample = (i16 *)region1;
        i16 *sourceSample = sourceBuffer->samples;
        DWORD region1SamplesCount = region1Bytes / soundOutput->bytesPerSample;                        
        for(DWORD sampleIndex = 0; sampleIndex < region1SamplesCount; sampleIndex++)
        {            
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            
            soundOutput->runningSampleIndex++;
        }

        destSample = (i16 *)region2;
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
    b32 isDown
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

internal r32
Win32ProcessXInputStickValue(SHORT value, SHORT deadZoneThreshold)
{
    r32 result = 0;
    if(value < -deadZoneThreshold)
    {
        result = (r32)(value + deadZoneThreshold) / (32768.0f - deadZoneThreshold);
    }
    else if(value > deadZoneThreshold)
    {
        result = (r32)(value - deadZoneThreshold) / (32767.0f - deadZoneThreshold);
    }

    return result;
}

internal void
Win32GetInputFileLocation(win32_state *state, b32 isInput, i32 slotIndex, i32 destCount, char *dest)
{
    char temp[64];
    wsprintf(temp, "loop_edit_%d_%s.hmi", slotIndex, isInput ? "input" : "state");
    Win32BuildEXEPathFileName(state, temp, destCount, dest);
}

internal win32_replay_buffer *
Win32GetReplayBuffer(win32_state *state, ui32 index)
{
    Assert(index < ArrayCount(state->replayBuffers));
    win32_replay_buffer *result = &state->replayBuffers[index];

    return result;
}

internal void
Win32BeginRecordingInput(win32_state *state, i32 inputRecordingIndex)
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
Win32BeginPlayBackInput(win32_state *state, i32 inputPlayingIndex)
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
            i32 playingIndex = state->inputPlayingIndex;
        
            Win32EndPlayBackInput(state);
            Win32BeginPlayBackInput(state, playingIndex);
            ReadFile(state->playingHandle, newInput, sizeof(*newInput), &bytesRead, 0);
        }
    }
}

internal void
ToggleFullscreen(HWND window)
{
    // NOTE: https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
    DWORD style = GetWindowLong(window, GWL_STYLE);
    if(style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO mi = {sizeof(mi)};
        if(GetWindowPlacement(window, &WindowPos) &&
           GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY),&mi))
        {
            SetWindowLong
                (
                    window, GWL_STYLE,
                    style & ~WS_OVERLAPPEDWINDOW
                );
            
            SetWindowPos
                (
                    window, HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED
                );
        }
    }
    else
    {
        SetWindowLong
            (
                window, GWL_STYLE,
                style | WS_OVERLAPPEDWINDOW
            );
        SetWindowPlacement(window, &WindowPos);
        SetWindowPos
            (
                window, NULL, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED
            );
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
                ui32 virtualKeyCode = (ui32)message.wParam;
                b32 wasDown = ((message.lParam & (1 << 30)) != 0);
                b32 isDown = ((message.lParam & (1 << 31)) == 0);

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
                                &keyboardController->back,
                                isDown
                            );
                    }
                    else if(virtualKeyCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage
                            (
                                &keyboardController->start,
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

                    if(isDown)
                    {
                        b32 altKeyWasDown = (message.lParam & (1 << 29));
                        if((virtualKeyCode == VK_F4) && altKeyWasDown)
                        {
                            IsRunning = false;
                        }

                        if((virtualKeyCode == VK_RETURN) && altKeyWasDown)
                        {
                            if(message.hwnd)
                            {
                                ToggleFullscreen(message.hwnd);
                            }
                        }
                    }
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

inline r32
Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    return (r32)(end.QuadPart - start.QuadPart) / (r32)GlobalPerfCountFrequency;   
}

internal void
HandleDebugCycleCounters(game_memory *memory)
{
#if HANDMADE_INTERNAL
    OutputDebugStringA("DEBUG CYCLE COUNT:\n");
    
    for(i32 counterIndex = 0;
        counterIndex < ArrayCount(memory->counters);
        counterIndex++)
    {
        debug_cycle_counter *counter = memory->counters + counterIndex;

        if(counter->hitCount)
        {
            char textBuffer[256];
            sprintf_s
                (
                    textBuffer, sizeof(textBuffer),
                    "  %d: %I64ucy %uh %I64ucy/h\n",
                    counterIndex, counter->cycleCount, counter->hitCount, counter->cycleCount / counter->hitCount
                );
            OutputDebugStringA(textBuffer);
            
            counter->cycleCount = 0;
            counter->hitCount = 0;
        }
    }
#endif
}

#if 0
internal void
Win32DebugDrawVertical(win32_offscreen_buffer *buffer, i32 x, i32 top, i32 bottom, ui32 color)
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
        ui8 *pixel = (ui8 *)buffer->memory + x * buffer->bytesPerPixel + top * buffer->pitch;
        for(i32 y = top; y < bottom; y++)
        {
            *(ui32 *)pixel = color;
            pixel += buffer->pitch;
        }
    }
}

inline void
Win32DrawSoundBufferMarker
(
    win32_offscreen_buffer *buffer,
    win32_sound_output *soundOutput,
    r32 coefficient,
    i32 padX,
    i32 top, i32 bottom,
    DWORD value,
    ui32 color
)
{
    i32 x = padX + (i32)(coefficient * (r32)value);    
    Win32DebugDrawVertical(buffer, x, top, bottom, color);
}

internal void
Win32DebugSyncDisplay
(
    win32_offscreen_buffer *buffer,
    i32 markerCount,
    win32_debug_time_marker *markers,
    i32 currentMarkerIndex,
    win32_sound_output *soundOutput,
    r32 targetSecondsPerFrame
)
{
    i32 padX = 16;
    i32 padY = 16;

    i32 lineHeight = 64;
    
    r32 coefficient = (r32)(buffer->width - 2 * padX) / (r32)soundOutput->secondaryBufferSize;
    for(i32 markerIndex = 0; markerIndex < markerCount; markerIndex++)
    {
        win32_debug_time_marker *thisMarker = &markers[markerIndex];

        Assert(thisMarker->outputPlayCursor < soundOutput->secondaryBufferSize);
        Assert(thisMarker->outputWriteCursor < soundOutput->secondaryBufferSize);
        Assert(thisMarker->outputLocation < soundOutput->secondaryBufferSize);
        Assert(thisMarker->outputByteCount < soundOutput->secondaryBufferSize);
        Assert(thisMarker->flipPlayCursor < soundOutput->secondaryBufferSize);
        Assert(thisMarker->flipWriteCursor < soundOutput->secondaryBufferSize);        
        
        i32 top = padY;
        i32 bottom = padY + lineHeight;
            
        DWORD playColor = 0xFFFFFFFF;
        DWORD writeColor = 0xFFFF0000;
        DWORD expectedFlipColor = 0xFFFFFF00;
        DWORD playWindowColor = 0xFFFF00FF;
        
        if(markerIndex == currentMarkerIndex)
        {
            top += lineHeight + padY;
            bottom += lineHeight + padY;

            i32 firstTop = top;

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

struct platform_work_queue_entry
{
    platform_work_queue_callback *callback;
    void *data;
};

struct platform_work_queue
{
    ui32 volatile completionGoal;
    ui32 volatile completionCount;
    
    ui32 volatile nextEntryToWrite;
    ui32 volatile nextEntryToRead;
    HANDLE semaphoreHandle;

    platform_work_queue_entry entries[256];
};

internal void
Win32AddEntry(platform_work_queue *queue, platform_work_queue_callback *callback, void *data)
{
    ui32 newNextEntryToWrite = (queue->nextEntryToWrite + 1) % ArrayCount(queue->entries);
    Assert(newNextEntryToWrite != queue->nextEntryToRead);
    platform_work_queue_entry *entry = queue->entries + queue->nextEntryToWrite;
    entry->callback = callback;
    entry->data = data;
    queue->completionGoal++;
    _WriteBarrier();
    queue->nextEntryToWrite = newNextEntryToWrite;
    ReleaseSemaphore(queue->semaphoreHandle, 1, 0);
}

internal b32
Win32DoNextWorkQueueEntry(platform_work_queue *queue)
{
    b32 weShouldSleep = false;

    ui32 originalNextEntryToRead = queue->nextEntryToRead;
    ui32 newNextEntryToRead = (originalNextEntryToRead + 1) % ArrayCount(queue->entries);
    if(originalNextEntryToRead != queue->nextEntryToWrite)
    {
        ui32 index = InterlockedCompareExchange((LONG volatile *)&queue->nextEntryToRead,
                                                newNextEntryToRead,
                                                originalNextEntryToRead);

        if(index == originalNextEntryToRead)
        {
            platform_work_queue_entry entry = queue->entries[index];
            entry.callback(queue, entry.data);
            InterlockedIncrement((LONG volatile *)&queue->completionCount);
        }
    }
    else
    {
        weShouldSleep = true;
    }

    return weShouldSleep;
}

internal void
Win32CompleteAllWork(platform_work_queue *queue)
{
    while(queue->completionGoal != queue->completionCount)
    {
        Win32DoNextWorkQueueEntry(queue);
    }

    queue->completionGoal = 0;
    queue->completionCount = 0;
}

DWORD WINAPI
ThreadProc(LPVOID lpParameter)
{
    platform_work_queue *queue = (platform_work_queue *)lpParameter;

    for(;;)
    {
        if(Win32DoNextWorkQueueEntry(queue))
        {
            WaitForSingleObjectEx(queue->semaphoreHandle, INFINITE, FALSE);
        }
    }
}

internal PLATFORM_WORK_QUEUE_CALLBACK(DoWorkerWork)
{
    char buffer[256];
    wsprintf(buffer, "Thread %u: %s\n",
             GetCurrentThreadId(), (char *)data);
    
    OutputDebugStringA(buffer);
}

internal void
Win32MakeQueue(platform_work_queue *queue, ui32 threadCount)
{
    queue->completionGoal = 0;
    queue->completionCount = 0;
    
    queue->nextEntryToWrite = 0;
    queue->nextEntryToRead = 0;
    
    ui32 initialCount = 0;
    queue->semaphoreHandle = CreateSemaphoreEx(0, initialCount,
                                               threadCount, 0, 0, SEMAPHORE_ALL_ACCESS);
    
    for(ui32 threadIndex = 0; threadIndex < threadCount; threadIndex++)
    {
        DWORD threadID;
        HANDLE threadHandle = CreateThread(0, 0, ThreadProc, queue, 0, &threadID);
        CloseHandle(threadHandle);
    }
}

internal PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(Win32GetAllFilesOfTypeBegin)
{
    platform_file_group fileGroup = {};

    return fileGroup;
}

internal PLATFORM_GET_ALL_FILE_OF_TYPE_END(Win32GetAllFilesOfTypeEnd)
{
}

internal PLATFORM_OPEN_FILE(Win32OpenFile)
{
    return 0;
}

internal PLATFORM_READ_DATA_FROM_FILE(Win32ReadDataFromFile)
{
}

internal PLATFORM_FILE_ERROR(Win32FileError)
{
}

int
CALLBACK WinMain
(
    HINSTANCE instance,
    HINSTANCE prevInstance,
    LPSTR cmdLine,
    i32 cmdShow
)
{       
    LARGE_INTEGER perfCountFrequencyResult;
    QueryPerformanceFrequency(&perfCountFrequencyResult);
    GlobalPerfCountFrequency = perfCountFrequencyResult.QuadPart;

    platform_work_queue lowPriorityQueue = {};
    Win32MakeQueue(&lowPriorityQueue, 2);
    
    platform_work_queue highPriorityQueue = {};
    Win32MakeQueue(&highPriorityQueue, 6);
    
#if 0
    Win32AddEntry(&queue, DoWorkerWork, "String A0");
    Win32AddEntry(&queue, DoWorkerWork, "String A1");
    Win32AddEntry(&queue, DoWorkerWork, "String A2");
    Win32AddEntry(&queue, DoWorkerWork, "String A3");
    Win32AddEntry(&queue, DoWorkerWork, "String A4");
    Win32AddEntry(&queue, DoWorkerWork, "String A5");
    Win32AddEntry(&queue, DoWorkerWork, "String A6");
    Win32AddEntry(&queue, DoWorkerWork, "String A7");
    Win32AddEntry(&queue, DoWorkerWork, "String A8");
    Win32AddEntry(&queue, DoWorkerWork, "String A9");
    
    Win32AddEntry(&queue, DoWorkerWork, "String B0");
    Win32AddEntry(&queue, DoWorkerWork, "String B1");
    Win32AddEntry(&queue, DoWorkerWork, "String B2");
    Win32AddEntry(&queue, DoWorkerWork, "String B3");
    Win32AddEntry(&queue, DoWorkerWork, "String B4");
    Win32AddEntry(&queue, DoWorkerWork, "String B5");
    Win32AddEntry(&queue, DoWorkerWork, "String B6");
    Win32AddEntry(&queue, DoWorkerWork, "String B7");
    Win32AddEntry(&queue, DoWorkerWork, "String B8");
    Win32AddEntry(&queue, DoWorkerWork, "String B9");

    Win32CompleteAllWork(&queue);
#endif
    
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
    b32 sleepIsGranular = (timeBeginPeriod(1) == TIMERR_NOERROR);
    
    Win32LoadXInput();
    
#if HANDMADE_INTERNAL
    DEBUGShowCursor = true;
#endif
    
    WNDCLASS windowClass = {};

#if 0
    Win32ResizeDIBSection(&GlobalBackbuffer, 1279, 719);
#else
    //Win32ResizeDIBSection(&GlobalBackbuffer, 960, 540);
    Win32ResizeDIBSection(&GlobalBackbuffer, 1920, 1080);
#endif
    
    windowClass.style = CS_HREDRAW|CS_VREDRAW;
    windowClass.lpfnWndProc = Win32MainWindowCallback;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(0, IDC_ARROW);
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
            i32 monitorRefreshHz = 60;

            HDC refreshDC = GetDC(window);            
            i32 win32RefreshRate = GetDeviceCaps(refreshDC, VREFRESH);
            ReleaseDC(window, refreshDC);
            if(win32RefreshRate > 1)
            {
                monitorRefreshHz = win32RefreshRate;
            }
            
            r32 gameUpdateHz = (r32)monitorRefreshHz; //monitorRefreshHz / 2.0f;            
            r32 targetSecondsPerFrame = 1.0f / (r32) gameUpdateHz;
            
            // NOTE: Sound test
            soundOutput.samplesPerSecond = 48000;
            soundOutput.bytesPerSample = sizeof(i16) * 2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.safetyBytes = (i32)((r32)soundOutput.samplesPerSecond * (r32)soundOutput.bytesPerSample / gameUpdateHz / 2.0f);
                        
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

            ui32 maxPossibleOverrun = 2 * 8 * sizeof(ui16);
            i16 *samples = (i16 *)VirtualAlloc
                (
                    0,
                    soundOutput.secondaryBufferSize + maxPossibleOverrun,
                    MEM_RESERVE|MEM_COMMIT,
                    PAGE_READWRITE
                );

#if HANDMADE_INTERNAL
            LPVOID baseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID baseAddress = 0;
#endif
            
            game_memory gameMemory = {};
            gameMemory.permanentStorageSize = Megabytes(256);
            gameMemory.transientStorageSize = Gigabytes(1);

            gameMemory.highPriorityQueue = &highPriorityQueue;
            gameMemory.lowPriorityQueue = &lowPriorityQueue;
            gameMemory.platformAPI.AddEntry = Win32AddEntry;
            gameMemory.platformAPI.CompleteAllWork = Win32CompleteAllWork;
            
            gameMemory.platformAPI.GetAllFilesOfTypeBegin = Win32GetAllFilesOfTypeBegin;
            gameMemory.platformAPI.GetAllFilesOfTypeEnd = Win32GetAllFilesOfTypeEnd;
            gameMemory.platformAPI.OpenFile = Win32OpenFile;
            gameMemory.platformAPI.ReadDataFromFile = Win32ReadDataFromFile;
            gameMemory.platformAPI.FileError = Win32FileError;
            
            gameMemory.platformAPI.DEBUGFreeFileMemory = DEBUGPlatformFreeFileMemory;
            gameMemory.platformAPI.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
            gameMemory.platformAPI.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;
            
            win32State.totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
            win32State.gameMemoryBlock = VirtualAlloc
                (
                    baseAddress,
                    (size_t)win32State.totalSize,
                    MEM_RESERVE|MEM_COMMIT,
                    PAGE_READWRITE
                );
            
            gameMemory.permanentStorage = win32State.gameMemoryBlock;            
            gameMemory.transientStorage = (ui8 *)gameMemory.permanentStorage + gameMemory.permanentStorageSize;
            
            for(i32 replayIndex = 0; replayIndex < ArrayCount(win32State.replayBuffers); replayIndex++)
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

                i32 debugTimeMarkersIndex = 0;
                win32_debug_time_marker debugTimeMarkers[30] = {};
                
                b32 isSoundValid = false;
                DWORD audioLatencyBytes = 0;
                r32 audioLatencySeconds = 0;
                
                win32_game_code gameCode = Win32LoadGameCode
                    (
                        gameCodeDLLFullPath,
                        gameCodeTempDLLFullPath,
                        gameCodeLockFullPath
                    );
                                                
                ui64 lastCycleCount = __rdtsc();
                while(IsRunning)
                {
                    newInput->deltaTime = targetSecondsPerFrame;
                    newInput->executableReloaded = false;
                    
                    FILETIME newDLLWriteTime = Win32GetLastWriteTime(gameCodeDLLFullPath);
                    if(CompareFileTime(&newDLLWriteTime, &gameCode.lastDLLWriteTime) != 0)
                    {
                        Win32CompleteAllWork(&highPriorityQueue);
                        Win32CompleteAllWork(&lowPriorityQueue);

                        Win32UnloadGameCode(&gameCode);
                        gameCode = Win32LoadGameCode
                            (
                                gameCodeDLLFullPath,
                                gameCodeTempDLLFullPath,
                                gameCodeLockFullPath
                            );

                        newInput->executableReloaded = true;
                    }
                    
                    // TODO: Zeroing macro
                    game_controller_input *oldKeyboardController = GetController(oldInput, 0);
                    game_controller_input *newKeyboardController = GetController(newInput, 0);

                    *newKeyboardController = {};
                    newKeyboardController->isConnected = true;

                    for(i32 buttonIndex = 0; buttonIndex < ArrayCount(oldKeyboardController->buttons); buttonIndex++)
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
                            
                                r32 threshold = 0.5f;

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
                    
                        game_offscreen_buffer buffer = {};
                        buffer.memory = GlobalBackbuffer.memory;
                        buffer.width = GlobalBackbuffer.width;
                        buffer.height = GlobalBackbuffer.height;
                        buffer.pitch = GlobalBackbuffer.pitch;
                        
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
                            gameCode.updateAndRender(&gameMemory, newInput, &buffer);
                            HandleDebugCycleCounters(&gameMemory);
                        }
                        
                        LARGE_INTEGER audioWallClock = Win32GetWallClock();
                        r32 deltaAudioSeconds = Win32GetSecondsElapsed(flipWallClock, audioWallClock);
                    
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

                            DWORD expectedBytesPerFrame = (i32)((r32)(soundOutput.samplesPerSecond * soundOutput.bytesPerSample) / gameUpdateHz);
                            r32 secondsLeftUntilFlip = targetSecondsPerFrame - deltaAudioSeconds;
                            DWORD expectedBytesUntilFlip = (DWORD)((secondsLeftUntilFlip / targetSecondsPerFrame) * (r32)expectedBytesPerFrame);
                            
                            DWORD expectedFrameBoundaryByte = playCursor + expectedBytesUntilFlip;
                        
                            DWORD safeWriteCursor = writeCursor;
                            if(safeWriteCursor < playCursor)
                            {
                                safeWriteCursor += soundOutput.secondaryBufferSize;
                            }

                            Assert(safeWriteCursor >= playCursor);
                            safeWriteCursor += soundOutput.safetyBytes;
                        
                            b32 isAudioCardLowLatency = (safeWriteCursor < expectedFrameBoundaryByte);
                        
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
                            soundBuffer.sampleCount = Align8(bytesToWrite / soundOutput.bytesPerSample);
                            bytesToWrite = soundBuffer.sampleCount * soundOutput.bytesPerSample;
                            soundBuffer.samples = samples;

                            if(gameCode.getSoundSamples)
                            {
                                gameCode.getSoundSamples(&gameMemory, &soundBuffer);
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
                            audioLatencySeconds = (r32)audioLatencyBytes / (r32)soundOutput.bytesPerSample / (r32)soundOutput.samplesPerSecond;

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

                        r32 workSecondsElapsed = Win32GetSecondsElapsed(lastCounter, workCounter);
                        r32 secondsElapsedForFrame = workSecondsElapsed;
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

                            r32 testElapsedTime = Win32GetSecondsElapsed(lastCounter, Win32GetWallClock());

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
                        r32 msPerFrame = 1000.0f * Win32GetSecondsElapsed(lastCounter, endCounter);
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
                        
#if 1
                        ui64 endCycleCount = __rdtsc();
                        ui64 cyclesElapsed = endCycleCount - lastCycleCount;
                        lastCycleCount = endCycleCount;
                    
                        r64 fps = 0;
                        r64 mcpf = (r64)cyclesElapsed / (1000.0f * 1000.0f);

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
