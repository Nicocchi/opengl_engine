#define COBJMACROS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define WIN_32_EXTRA_LEAN

#include <windows.h>
#include <glad/glad.h>
#include <glad/glad.c>
#include <hidusage.h>
#include <xaudio2.h>

#define Assert(exp) \
if(!(exp)) {*(int*)0 = 0;}

#include "game.h"
#include "engine.h"

#include "shader_resource.h"

#include "game.cpp"
#include "renderer.cpp"

#include "win32_main.h"

global_variable PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;
global_variable PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;
global_variable PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
global_variable HWND window;
global_variable HDC dc;

internal void 
FatalError(const char* message)
{
    MessageBoxA(NULL, message, "Error", MB_ICONEXCLAMATION);
    ExitProcess(0);
}

#ifdef DEBUG
internal void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
									 GLsizei length, const GLchar* message, const void* user)
{
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
    if (severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM)
    {
        if (IsDebuggerPresent())
        {
            Assert(!"OpenGL error - check the callstack in debugger");
        }
        FatalError("OpenGL API usage error! Use debugger to examine call stack!");
    }
}
#endif

PLATFORM_LOG(Win32Log)
{
	OutputDebugStringA(fmt);
}

READ_ENTIRE_FILE(Win32ReadEntireFile)
{
	read_file_res res = {};
	
    u32 bytesRead;
    u32 fileSize32;
    HANDLE fileHandle = CreateFileA(filename,
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    0,
                                    OPEN_EXISTING,
                                    0,
                                    0);
    
    if(fileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSize;
        if(GetFileSizeEx(fileHandle, &fileSize))
        {
            // NOTE: Will cause problem for 64bits
            fileSize32 = (u32)fileSize.QuadPart;
			u8* ptr = memory->base + memory->used;
			arena_PushSize_(memory, fileSize32);
            
            if(ptr)
            {
                if(ReadFile(fileHandle, ptr, fileSize32, (LPDWORD)&bytesRead, 0))
                {
					res.data = ptr;
					res.size = fileSize32;
                }
				else
				{
                    // Release arena
				}
            }
            else
            {
                // TODO: Logging
            }
        }
        else
        {
            // TODO: Logging
        }
		
		CloseHandle(fileHandle);
    }
    else
    {
        // TODO: Logging
    }
	
	return res;
}

WRITE_ENTIRE_FILE(Win32WriteEntireFile)
{
    return 0;
}

inline f64
Win32UpdateClock(LARGE_INTEGER* lastTimer, u64 freq)
{
    LARGE_INTEGER c2;
	QueryPerformanceCounter(&c2);
	f64 delta = (f64)(c2.QuadPart - lastTimer->QuadPart) / (f64)freq;
    *lastTimer = c2;
    
    return delta;
}

LRESULT WINAPI
WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
		case WM_MOVING:
		{
			
		}break;
		
		case WM_MOVE:
		{
			
		}break;
		
        case WM_SYSCOMMAND:
        {
            if ((wParam & 0xfff0) == SC_KEYMENU) // NOTE: Disable ALT application menu
                return 0;
        } break;
        
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        } break;
    }
    
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void *platform_load_gl_function(char *funName)
{
    PROC proc = wglGetProcAddress(funName);
    if (!proc)
    {
        static HMODULE openglDLL = LoadLibraryA("opengl32.dll");
        proc = GetProcAddress(openglDLL, funName);

        if (!proc)
        {
            FatalError("Failed to load gl function");
            return nullptr;
        }
    }

    return (void*)proc;
}

internal void
Win32GetWglFunctions()
{
    
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{    
    WNDCLASSEX wc = 
    { 
        sizeof(WNDCLASSEX), 
        CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS, 
        WndProc, 
        0L, 0L, 
        GetModuleHandle(NULL), 
        NULL, NULL, NULL, NULL, 
        "Engine", 
        NULL 
    };

    PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;

    int dwStyle = WS_OVERLAPPEDWINDOW;

    // Fake window initializing OpenGL
    {
        window = CreateWindowExW(
								 0, L"STATIC", L"DummyWindow", WS_OVERLAPPED,
								 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
								 NULL, NULL, NULL, NULL);
        
        if (window == NULL)
        {
            FatalError("Failed to create dummy window");
        }

        HDC fakeDC = GetDC(window);
        if (!fakeDC)
        {
            FatalError("Failed to get HDC");
        }

        PIXELFORMATDESCRIPTOR pfd = {0};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cAlphaBits = 8;
        pfd.cDepthBits = 24;

        int pixelFormat = ChoosePixelFormat(fakeDC, &pfd);
        if (!pixelFormat)
        {
            FatalError("Failed to choose pixel format");
        }

        if (!SetPixelFormat(fakeDC, pixelFormat, &pfd))
        {
            FatalError("Failed to set pixel format");
        }

        // Create a Handle to a fake OpenGL Rendering Context
        HGLRC fakeRC = wglCreateContext(fakeDC);
        if (!fakeRC)
        {
            FatalError("Failed to create render context");
        }

        if (!wglMakeCurrent(fakeDC, fakeRC))
        {
            FatalError("Failed to make current");
        }

        wglChoosePixelFormatARB = 
            (PFNWGLCHOOSEPIXELFORMATARBPROC)platform_load_gl_function("wglChoosePixelFormatARB");
        wglCreateContextAttribsARB =
            (PFNWGLCREATECONTEXTATTRIBSARBPROC)platform_load_gl_function("wglCreateContextAttribsARB");
        wglSwapIntervalEXT =
            (PFNWGLSWAPINTERVALEXTPROC)platform_load_gl_function("wglSwapIntervalEXT");
        
        if (!wglCreateContextAttribsARB || !wglChoosePixelFormatARB)
        {
            FatalError("Failed to load OpenGL functions");
        }

        // Clean up the fake stuff
        wglMakeCurrent(fakeDC, 0);
        wglDeleteContext(fakeRC);
        ReleaseDC(window, fakeDC);

        // Can't reuse the same (Device)Context because
        // we already called "SetPixelFormat"
        DestroyWindow(window);
    }

    if (!RegisterClassEx(&wc))
    {
        FatalError("RegisterClassEX failed");
    }
    
    i32 width = CW_USEDEFAULT;
    i32 height = CW_USEDEFAULT;
    
    platform_engine engine = {};
    platform_render render = {};

    engine.running = true;
    
    {
        platform_memory* memory = &engine.memory;
        memory->permanentStorage.size = Megabyte(256);
        memory->permanentStorage.used = 0;
        memory->permanentStorage.base = (u8*)VirtualAlloc(0, memory->permanentStorage.size, 
														  MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        
        memory->transientStorage.size = Gigabytes(2);
        memory->transientStorage.used = 0;
        memory->transientStorage.base = (u8*)VirtualAlloc(0, memory->transientStorage.size, 
														  MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        
        memory->readFile  = Win32ReadEntireFile;
        memory->writeFile = Win32WriteEntireFile;
        
        // TEMP:
        engine.gameInit = game_Init;
        engine.gameUpdate = game_Update;
        if(engine.gameInit)
        {
            engine.gameInit(&engine);
        }
        
        engine.log = Win32Log;
        engine.vsSource = _shader_resource_vs46;
        engine.psSource = _shader_resource_ps46;
    }
    
    // Real window initializing OpenGL
    {
        window = CreateWindowEx(0, wc.lpszClassName, "Engine", dwStyle,
                                100, 100, width, height, NULL, NULL, wc.hInstance, NULL);
        
        if (window == NULL)
        {
            FatalError("Failed to create real window");
        }

        dc = GetDC(window);
        if (!dc)
        {
            FatalError("Failed to get DC");
        }

        const int pixelAttribs[] =
        {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
            WGL_SWAP_METHOD_ARB,    WGL_SWAP_COPY_ARB,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
            WGL_COLOR_BITS_ARB,     32,
            WGL_ALPHA_BITS_ARB,     8,
            WGL_DEPTH_BITS_ARB,     24,
            0
        };

        UINT numPixelFormats;
        int pixelFormat = 0;
        if (!wglChoosePixelFormatARB(dc, pixelAttribs,
                                    0, // Float list
                                    1, // Max formats
                                    &pixelFormat,
                                    &numPixelFormats))
        {
            FatalError("Failed to wglChoosePixelFormatARB");
            return false;
        }

        PIXELFORMATDESCRIPTOR pfd = {0};
        DescribePixelFormat(dc, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

        if (!SetPixelFormat(dc, pixelFormat, &pfd))
        {
            FatalError("Failed to SetPixelFormat");
        }

        const int contextAttribs[] =
        {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
            0
        };

        HGLRC rc = wglCreateContextAttribsARB(dc, 0, contextAttribs);
        if (!rc)
        {
            FatalError("Failed to create Render context for OpenGL");
        }

        if (!wglMakeCurrent(dc, rc))
        {
            FatalError("Failed to wglMakeCurrent");
        }

        if(!gladLoadGL())
		{
			return -1;
		}
    }

    ShowWindow(window, SW_SHOW);

    PFNWGLGETEXTENSIONSSTRINGEXTPROC _wglGetExtensionsStringEXT = 
	(PFNWGLGETEXTENSIONSSTRINGEXTPROC)wglGetProcAddress("wglGetExtensionsStringEXT");
    b32 swapControlSupported = strstr(_wglGetExtensionsStringEXT(), "WGL_EXT_swap_control") != 0;
    i32 vsynch = 0;
    
    if (swapControlSupported) {
		PFNWGLSWAPINTERVALEXTPROC wglSwapInternalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
		PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT = 
		(PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
		if (wglSwapInternalEXT(1))
		{
			OutputDebugStringA("VSynch enabled \n");
		}
		else
		{
			OutputDebugStringA("Could not enable VSynch\n");
		}
	}
	else
	{
		OutputDebugStringA("WGL_EXT_swap_control not supported \n");
	}
    
    // -------------------------------------------------------------------------
    render_Init(&engine, &render);
    
    LARGE_INTEGER clock;
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&clock);
    
    WINDOWPLACEMENT plac;
    GetWindowPlacement(window, &plac);
    engine.posX = plac.rcNormalPosition.left;
    engine.posY = plac.rcNormalPosition.top;
    
    while(engine.running)
    {
        MSG msg;
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            switch (msg.message)
            {
                case WM_LBUTTONDOWN:
                {
                } break;
                case WM_LBUTTONUP:
                {
                } break;

                case WM_QUIT:
                {
                    engine.running = false;
                } break;
            }
            
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        
        RECT rect;
        GetClientRect(window, &rect);
        width = rect.right - rect.left;
		height = rect.bottom - rect.top;
        
        f64 delta = Win32UpdateClock(&clock, freq.QuadPart);
        engine.time += delta;

        engine.gameUpdate(&engine, delta);
        
        if(width != engine.windowWidth || height != engine.windowHeight)
        {
            render_Resize(&engine, width, height);
        }
        
        render_DrawScene(&engine, &render, delta);
        
        //render_EndFrame(&engine);
        SwapBuffers(dc);
        //if (vsynch != 0) glFinish();
    }
    
    DestroyWindow(window);
    // UnregisterClass(wc.lpszClassName, wc.hInstance);
    
    return 0;
}
