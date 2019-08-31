#include <windows.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

global_variable bool IsRunning;

global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel = 4;

internal void
RenderWeirdGradient(int xOffset, int yOffset)
{
    int pitch = BitmapWidth * BytesPerPixel;
    uint8 *row = (uint8 *)BitmapMemory;

    for(int y = 0; y < BitmapHeight; y++)
    {
        uint32 *pixel = (uint32 *)row;
        
        for(int x = 0; x < BitmapWidth; x++)
        {
            uint8 blue = x + xOffset;
            uint8 green = y + yOffset;
            
            *pixel++ = ((green << 8) | blue);
        }

        row += pitch;
    }
}

internal void
Win32ResizeDIBSection(int width, int height)
{
    if(BitmapMemory)
    {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }

    BitmapWidth = width;
    BitmapHeight = height;
    
    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = BitmapWidth;
    BitmapInfo.bmiHeader.biHeight = -BitmapHeight; // for top-down bitmap
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    int bmpMemorySize = BitmapWidth * BitmapHeight * BytesPerPixel;
    BitmapMemory = VirtualAlloc
        (
            0,
            bmpMemorySize,
            MEM_COMMIT,
            PAGE_READWRITE
        );
}

internal void
Win32UpdateWindow
(
    HDC deviceContext,
    RECT *clientRect,
    int x,
    int y,
    int width,
    int height
)
{
    int windowWidth = clientRect->right - clientRect->left;
    int windowHeight = clientRect->bottom - clientRect->top;
        
    StretchDIBits
        (
            deviceContext,
            /*x, y, width, height,
              x, y, width, height,*/
            0, 0, BitmapWidth, BitmapHeight,
            0, 0, windowWidth, windowHeight,
            BitmapMemory,
            &BitmapInfo,
            DIB_RGB_COLORS,
            SRCCOPY
        );
}

LRESULT CALLBACK
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
            RECT clientRect;
            GetClientRect(window, &clientRect);
        
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;
            
            Win32ResizeDIBSection(width, height);
        } break;

        case WM_CLOSE:
        {
            IsRunning = false;
        } break;

        case WM_DESTROY:
        {
            IsRunning = false;
        } break;        

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");            
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);
            
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
          
            int height = paint.rcPaint.bottom - paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;

            RECT clientRect;
            GetClientRect(window, &clientRect);
        
            Win32UpdateWindow
                (
                    deviceContext,
                    &clientRect,
                    x, y,
                    width, height
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

int CALLBACK WinMain
(
    HINSTANCE instance,
    HINSTANCE prevInstance,
    LPSTR cmdLine,
    int cmdShow
)
{
    WNDCLASS windowClass = {};

    windowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
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
            
            int xOffset = 0;
            int yOffset = 0;
                
            IsRunning = true;
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

                RenderWeirdGradient(xOffset, yOffset);

                HDC deviceContext = GetDC(window);
                
                RECT clientRect;
                GetClientRect(window, &clientRect);
                int windowWidth = clientRect.right - clientRect.left;
                int windowHeight = clientRect.bottom - clientRect.top;
                
                Win32UpdateWindow
                    (
                        deviceContext,
                        &clientRect,
                        0, 0,
                        windowWidth, windowHeight
                    );

                ReleaseDC(window, deviceContext);
                
                ++xOffset;
                yOffset += 2;
            }
        }
        else
        {
            // TODO(sevada): logging
        }
    }
    else
    {
        // TODO(sevada): logging
    }
    
    return 0;
}
