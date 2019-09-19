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
                    }
                }
                else
                {
                }
            }
            else
            {
            }

            DSBUFFERDESC bufferDescription = {};
            bufferDescription.dwSize = sizeof(bufferDescription);
            bufferDescription.dwBufferBytes = bufferSize;
            bufferDescription.lpwfxFormat = &waveFormat;
            
            // NOTE: "Create" a secondary buffer                
            if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &SecondaryBuffer, 0)))
            {
                OutputDebugStringA("Secondary buffer created successfully!\n");
            }
            else
            {
            }          
        }
        else
        {
        }
    }
    else
    {
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND window)
{
    win32_window_dimension result;
    
    RECT clientRect;
    GetClientRect(window, &clientRect);
    result.Width = clientRect.right - clientRect.left;
    result.Height = clientRect.bottom - clientRect.top;

    return result;
}               

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *buffer, int width, int height)
{
    if(buffer->Memory)
    {
        VirtualFree(buffer->Memory, 0, MEM_RELEASE);
    }

    buffer->Width = width;
    buffer->Height = height;
    
    buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader);
    buffer->Info.bmiHeader.biWidth = buffer->Width;
    buffer->Info.bmiHeader.biHeight = -buffer->Height; // for top-down bitmap
    buffer->Info.bmiHeader.biPlanes = 1;
    buffer->Info.bmiHeader.biBitCount = 32;
    buffer->Info.bmiHeader.biCompression = BI_RGB;

    int bmpMemorySize = buffer->Width * buffer->Height * 4;
    buffer->Memory = VirtualAlloc
        (
            0,
            bmpMemorySize,
            MEM_RESERVE|MEM_COMMIT,
            PAGE_READWRITE
        );
    
    buffer->Pitch = buffer->Width * 4;
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
            0, 0, buffer->Width, buffer->Height,
            buffer->Memory,
            &buffer->Info,
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
            uint32 virtualKeyCode = wParam;
            bool wasDown = ((lParam & (1 << 30)) != 0);
            bool isDown = ((lParam & (1 << 31)) == 0);

            if(wasDown != isDown)
            {
                if(virtualKeyCode == 'W')
                {
                }
                else if(virtualKeyCode == 'A')
                {
                }
                else if(virtualKeyCode == 'S')
                {
                }
                else if(virtualKeyCode == 'D')
                {
                }
                else if(virtualKeyCode == 'Q')
                {
                }
                else if(virtualKeyCode == 'E')
                {
                }
                else if(virtualKeyCode == VK_UP)
                {
                }
                else if(virtualKeyCode == VK_LEFT)
                {
                }
                else if(virtualKeyCode == VK_RIGHT)
                {
                }
                else if(virtualKeyCode == VK_DOWN)
                {
                }
                else if(virtualKeyCode == VK_ESCAPE)
                {               
                    OutputDebugStringA("Escape: ");
                    
                    if(isDown)
                    {
                        OutputDebugStringA("is down ");
                    }
                    if(wasDown)
                    {                    
                        OutputDebugStringA("was down ");
                    }

                    OutputDebugStringA("\n");
                }
                else if(virtualKeyCode == VK_SPACE)
                {
                }                
            }

            bool32 altKeyWasDown = (lParam & (1 << 29));
            if((virtualKeyCode == VK_F4) && altKeyWasDown)
            {
                IsRunning = false;
            }
            
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
                    dimension.Width,
                    dimension.Height
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
Win32FillSoundBuffer(win32_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite,
                     game_sound_output_buffer *sourceBuffer)
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
Win32ProcessXInputDigitalButton(DWORD xInputButtonState,
                                game_button_state *oldState,
                                DWORD buttonBit,
                                game_button_state *newState)
{
    newState->endedDown = (xInputButtonState & buttonBit) == buttonBit;
    newState->halfTransitionCount = (oldState->endedDown != newState->endedDown) ? 1 : 0;
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
    int64 perfCountFrequency = perfCountFrequencyResult.QuadPart;
    
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
            soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;
            
            Win32InitDSound(window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
            
            Win32ClearSoundBuffer(&soundOutput);
            SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
            int16 *samples = (int16 *)VirtualAlloc
                (
                    0,
                    soundOutput.secondaryBufferSize,
                    MEM_RESERVE|MEM_COMMIT,
                    PAGE_READWRITE
                );

            game_input input[2] = {};
            game_input *newInput = &input[0];
            game_input *oldInput = &input[1];

            IsRunning = true;
            
            LARGE_INTEGER lastCounter;
            QueryPerformanceCounter(&lastCounter);
            uint64 lastCycleCounter = __rdtsc();
            while(IsRunning)
            {
                MSG message;
                                
                while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
                {
                    if(message.message == WM_QUIT)
                    {
                        IsRunning = false;
                    }
                        
                    TranslateMessage(&message);
                    DispatchMessage(&message);   
                }

                int maxControllerCount = XUSER_MAX_COUNT;
                if(maxControllerCount > ArrayCount(newInput->controllers))
                {
                    maxControllerCount = ArrayCount(newInput->controllers);
                }
                
                for(DWORD controllerIndex = 0; controllerIndex < maxControllerCount; controllerIndex++)
                {
                    game_controller_input *oldController = &oldInput->controllers[controllerIndex];
                    game_controller_input *newController = &newInput->controllers[controllerIndex];
                    
                    XINPUT_STATE controllerState;                    
                    if(XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
                    {
                        // NOTE: this controller is plugged in
                        XINPUT_GAMEPAD *pad = &controllerState.Gamepad;

                        // TODO: DPad
                        bool up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

                        newController->isAnalog = true;
                        
                        newController->startX = oldController->endX;
                        newController->startY = oldController->endY;
                        
                        real32 x;
                        if(pad->sThumbLX < 0)
                        {
                            x = (real32)pad->sThumbLX / 32768.0f;
                        }
                        else
                        {
                            x = (real32)pad->sThumbLX / 32767.0f;
                        }
                        
                        newController->minX = newController->maxX = newController->endX = x;

                        real32 y;
                        if(pad->sThumbLY < 0)
                        {
                            y = (real32)pad->sThumbLY / 32768.0f;
                        }
                        else
                        {
                            y = (real32)pad->sThumbLY / 32767.0f;
                        }
                        
                        newController->minY = newController->maxY = newController->endY = y;
                        
                        
                        int16 stickX = pad->sThumbLX; 
                        int16 stickY = pad->sThumbLY;

                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->down,
                                                        XINPUT_GAMEPAD_A,
                                                        &newController->down);
                            
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->right,
                                                        XINPUT_GAMEPAD_B,
                                                        &newController->right);
                            
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->left,
                                                        XINPUT_GAMEPAD_X,
                                                        &newController->left);
                            
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->up,
                                                        XINPUT_GAMEPAD_Y,
                                                        &newController->up);
                            
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->leftShoulder,
                                                        XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                        &newController->leftShoulder);
                            
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->rightShoulder,
                                                        XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                        &newController->rightShoulder);
                                                                       
                        //bool start = (pad->wButtons & XINPUT_GAMEPAD_START);
                        //bool back = (pad->wButtons & XINPUT_GAMEPAD_BACK);
                    }
                    else
                    {
                        // NOTE: The controller isn't available
                    }
                }

                DWORD byteToLock = 0;
                DWORD targetCursor = 0;
                DWORD bytesToWrite = 0;
                DWORD playCursor = 0;
                DWORD writeCursor = 0;
                bool32 soundIsValid = false;
                
                if(SUCCEEDED(SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
                {
                    byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;                    
                    
                    targetCursor = (playCursor + soundOutput.latencySampleCount * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
                    
                    if(byteToLock > targetCursor)
                    {
                        bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
                        bytesToWrite += targetCursor;
                    }
                    else
                    {
                        bytesToWrite = targetCursor - byteToLock;
                    }
                    
                    soundIsValid = true;
                }

                game_offscreen_buffer buffer = {};
                buffer.memory = GlobalBackbuffer.Memory;
                buffer.width = GlobalBackbuffer.Width;
                buffer.height = GlobalBackbuffer.Height;
                buffer.pitch = GlobalBackbuffer.Pitch;

                game_sound_output_buffer soundBuffer = {};
                soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
                soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                soundBuffer.samples = samples;
                
                GameUpdateAndRender(newInput, &buffer, &soundBuffer);
                
                // NOTE: DirectSound output test
                if(soundIsValid)
                {
                    Win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
                }               
               
                win32_window_dimension dimension = Win32GetWindowDimension(window);                            
                Win32DisplayBufferInWindow
                    (
                        &GlobalBackbuffer,
                        deviceContext,
                        dimension.Width,
                        dimension.Height
                    );

                uint64 endCycleCounter = __rdtsc();
                
                LARGE_INTEGER endCounter;
                QueryPerformanceCounter(&endCounter);

                uint64 cyclesElapsed = endCycleCounter - lastCycleCounter;
                int64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
                real64 msPerFrame = (1000.0f * (real64)counterElapsed) / (real64)perfCountFrequency;
                real64 fps = (real64)perfCountFrequency / (real64)counterElapsed;
                real64 mcpf = (real64)cyclesElapsed / (1000.0f * 1000.0f);

#if 0
                char buffer[256];
                sprintf(buffer, "%.02fms/f, %.02ff/s, %.02fmc/f\n", msPerFrame, fps, mcpf);
                OutputDebugStringA(buffer);
#endif
                
                lastCounter = endCounter;
                lastCycleCounter = endCycleCounter;

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
    
    return 0;
}
