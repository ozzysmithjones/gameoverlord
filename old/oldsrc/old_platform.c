#include "old_platform.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#else
#include <pthread.h>
#endif
#include <stdio.h>
#include "platform_layer.h"

#ifdef _WIN32

STATIC_ASSERT(sizeof(mutex) >= sizeof(CRITICAL_SECTION), mutex_size_mismatch);
STATIC_ASSERT(alignof(mutex) >= alignof(CRITICAL_SECTION), mutex_alignment_mismatch);

void create_mutex(mutex* m) {
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)&m->data;
    InitializeCriticalSection(cs);
}

void lock_mutex(mutex* m) {
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)&m->data;
    EnterCriticalSection(cs);
}

void unlock_mutex(mutex* m) {
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)&m->data;
    LeaveCriticalSection(cs);
}

void destroy_mutex(mutex* m) {
    DeleteCriticalSection((CRITICAL_SECTION*)&m->data);
}

STATIC_ASSERT(sizeof(thread) >= sizeof(HANDLE), thread_size_mismatch);
STATIC_ASSERT(alignof(thread) >= alignof(HANDLE), thread_alignment_mismatch);

result create_thread(thread* t, unsigned long (*start_routine)(void*), void* arg) {
    HANDLE handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, NULL);
    if (handle == NULL) {
        return RESULT_FAILURE;
    }
    memcpy(&t->data, &handle, sizeof(HANDLE));
    return RESULT_SUCCESS;
}

void join_thread(thread* t) {
    WaitForSingleObject(*(HANDLE*)&t->data, INFINITE);
}

void destroy_thread(thread* t) {
    CloseHandle(*(HANDLE*)&t->data);
}

STATIC_ASSERT(sizeof(condition_variable) >= sizeof(CONDITION_VARIABLE), condition_variable_size_mismatch);
STATIC_ASSERT(alignof(condition_variable) >= alignof(CONDITION_VARIABLE), condition_variable_alignment_mismatch);

void init_condition_variable(condition_variable* cv) {
    InitializeConditionVariable((PCONDITION_VARIABLE)&cv->data);
}

void signal_condition_variable(condition_variable* cv) {
    WakeConditionVariable((PCONDITION_VARIABLE)&cv->data);
}

void wait_condition_variable(condition_variable* cv, mutex* m) {
    SleepConditionVariableCS((PCONDITION_VARIABLE)&cv->data, (PCRITICAL_SECTION)&m->data, INFINITE);
}

#define BITMAPINFO_TEMPLATE \
    (BITMAPINFO){ \
        .bmiHeader = { \
            .biSize = sizeof(BITMAPINFOHEADER), \
            .biWidth = 800,  /* Default width */ \
            .biHeight = 600, /* Default height */ \
            .biPlanes = 1, \
            .biBitCount = 32, /* Assuming 32 bits per pixel (RGBA) */ \
            .biCompression = BI_RGB, \
        } \
    }

static void resize_back_buffer(back_buffer* buffer, uint32_t width, uint32_t height) {
    ASSERT(buffer != NULL, return, "Back buffer cannot be NULL");

    if (buffer->bitmap_handle) {
        DeleteObject(buffer->bitmap_handle);
        buffer->bitmap_handle = NULL;
        buffer->memory = NULL;
    }

    BITMAPINFO bmi = { 0 };
    memcpy(&bmi, &BITMAPINFO_TEMPLATE, sizeof(BITMAPINFO));
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = height;

    if (!buffer->device_context) {
        buffer->device_context = CreateCompatibleDC(NULL);
    }

    HBITMAP bitmap = CreateDIBSection(buffer->device_context, &bmi, DIB_RGB_COLORS, &buffer->memory, NULL, 0);
    if (bitmap == NULL || buffer->memory == NULL) {
        BUG("Failed to create DIB section: %lu", GetLastError());
        DeleteDC(buffer->device_context);
        memset(buffer, 0, sizeof(back_buffer));
        return;
    }

    buffer->width = width;
    buffer->height = height;
    buffer->bitmap_handle = bitmap;
}

static void present_back_buffer(HDC hdc, back_buffer* buffer) {
    ASSERT(buffer != NULL, return, "Back buffer cannot be NULL");
    ASSERT(buffer->memory != NULL, return, "Back buffer memory cannot be NULL");

    BITMAPINFO bmi = { 0 };
    memcpy(&bmi, &BITMAPINFO_TEMPLATE, sizeof(BITMAPINFO));
    bmi.bmiHeader.biWidth = buffer->width;
    bmi.bmiHeader.biHeight = buffer->height;

    StretchDIBits(
        hdc,
        0, 0, buffer->width, buffer->height,
        0, 0, buffer->width, buffer->height,
        buffer->memory,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

static LRESULT window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    //CREATESTRUCT* pCreate = (CREATESTRUCT*)(lParam);
    //user_input* input = (user_input*)(pCreate->lpCreateParams);
    window* w = (window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)(lParam);
        window* w = (window*)(pCreate->lpCreateParams);
        // store the pointer in the instance data of the window
        // so it could always be retrieved by using GetWindowLongPtr(hwnd, GWLP_USERDATA) 
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)w);
    } break;
    case WM_SIZE: {
        // Get area of the window that we can draw to.
        RECT draw_rect = { 0 };
        BOOL result = GetClientRect(hwnd, &draw_rect);
        if (result == 0) {
            BUG("GetClientRect failed: %lu", GetLastError());
            return 0;
        }
        uint32_t width = draw_rect.right - draw_rect.left;
        uint32_t height = draw_rect.bottom - draw_rect.top;
        resize_back_buffer(&w->back_buffer, width, height);
        break;
    }
    case WM_CLOSE: {
        w->input.closed_window = true;
        PostQuitMessage(0);
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        present_back_buffer(hdc, &w->back_buffer);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_KEYDOWN: {
        int key = (int)wParam;
        if (key < 256) {
            w->input.keys[key].down = true;
        }
    } break;
    case WM_KEYUP: {
        int key = (int)wParam;
        if (key < 256) {
            w->input.keys[key].up = true;
        }
    }
    case WM_MOUSEMOVE: {
        w->input.mouse.x = GET_X_LPARAM(lParam);
        w->input.mouse.y = GET_Y_LPARAM(lParam);
    };
    case WM_MOUSEWHEEL: {
        w->input.mouse.wheel = GET_WHEEL_DELTA_WPARAM(wParam);
    } break;
    case WM_LBUTTONDOWN: {
        w->input.mouse.left_button_down = true;
    } break;
    case WM_LBUTTONUP: {
        w->input.mouse.left_button_up = true;
    } break;
    case WM_RBUTTONDOWN: {
        w->input.mouse.right_button_down = true;
    } break;
    case WM_RBUTTONUP: {
        w->input.mouse.right_button_up = true;
    } break;
    case WM_MBUTTONDOWN: {
        w->input.mouse.middle_button_down = true;
    } break;
    case WM_MBUTTONUP: {
        w->input.mouse.middle_button_up = true;
    } break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

result create_window(window* w, const char* title, int32_t width, int32_t height, window_mode mode) {
    DWORD style = WS_OVERLAPPEDWINDOW;
    const char* class_name = "window_class";
    HINSTANCE hInstance = GetModuleHandle(NULL);
    w->handle = NULL;
    w->input = (user_input){ 0 };

    // Register window class if not already registered
    static bool class_registered = false;
    if (!class_registered) {
        WNDCLASSA wc = { 0 };
        wc.lpfnWndProc = window_proc;
        wc.hInstance = hInstance;
        wc.lpszClassName = class_name;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        if (!RegisterClassA(&wc)) {
            fprintf(stderr, "Failed to register window class: %lu\n", GetLastError());
            return RESULT_FAILURE;
        }
        class_registered = true;
    }

    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);

    switch (mode) {
    case WINDOW_MODE_WINDOWED:
        style = WS_OVERLAPPEDWINDOW;
        break;
    case WINDOW_MODE_FULLSCREEN:
        style = WS_POPUP;
        width = screen_width;
        height = screen_height;
        break;
    case WINDOW_MODE_BORDERLESS_FULLSCREEN:
        style = WS_POPUP | WS_VISIBLE;
        width = screen_width;
        height = screen_height;
        break;
    }

    w->handle = CreateWindowExA(
        0, class_name, title,
        style, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, hInstance, w
    );

    if (w->handle == NULL) {
        DWORD error_code = GetLastError();
        BUG("Failed to create window: %lu", error_code);
        return RESULT_FAILURE;
    }

    ShowWindow((HWND)w->handle, SW_SHOW);

    POINT point = { 0 };
    bool is_cursor_pos = GetCursorPos(&point);
    bool is_cursor_within_window = ScreenToClient((HWND)w->handle, &point);
    if (is_cursor_pos && is_cursor_within_window) {
        w->input.mouse.x = point.x;
        w->input.mouse.y = point.y;
    }

    UpdateWindow((HWND)w->handle);
    return RESULT_SUCCESS;
}

void update_window_input(window* w) {
    int old_mouse_x = w->input.mouse.x;
    int old_mouse_y = w->input.mouse.y;
    memset(&w->input, 0, sizeof(w->input));
    w->input.mouse.x = old_mouse_x;
    w->input.mouse.y = old_mouse_y;

    while (1) {
        MSG message = { 0 };
        BOOL message_result = GetMessageA(&message, (HWND)w->handle, 0, 0);
        if (message_result == 0) {
            // WM_QUIT received
            w->input.closed_window = true;
            return;
        }
        else if (message_result == -1) {
            // Error occurred
            BUG("Error in WINAPI window GetMessage: %lu", GetLastError());
            return;
        }
        else {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }
}

void resize_window(window* w, int width, int height, window_mode mode) {
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    DWORD style = WS_OVERLAPPEDWINDOW;
    bool is_fullscreen = false;

    switch (mode) {
    case WINDOW_MODE_WINDOWED:
        style = WS_OVERLAPPEDWINDOW;
        is_fullscreen = false;
        break;
    case WINDOW_MODE_FULLSCREEN:
        style = WS_POPUP;
        is_fullscreen = true;
        break;
    case WINDOW_MODE_BORDERLESS_FULLSCREEN:
        style = WS_POPUP | WS_VISIBLE;
        is_fullscreen = true;
        break;
    }

    SetWindowLongPtr((HWND)w->handle, GWL_STYLE, (LONG_PTR)style);

    if (is_fullscreen) {
        SetWindowPos((HWND)w->handle, HWND_TOP, 0, 0, screen_width, screen_height, SWP_FRAMECHANGED);
    }
    else {
        // For windowed mode, we need to specify the actual size
        // Center the window on the screen
        int x = (screen_width - width) / 2;
        int y = (screen_height - height) / 2;
        SetWindowPos((HWND)w->handle, HWND_TOP, x, y, width, height, SWP_FRAMECHANGED);
    }

    // Make sure the window is visible and gets focus
    ShowWindow((HWND)w->handle, SW_SHOW);
    SetForegroundWindow((HWND)w->handle);
    UpdateWindow((HWND)w->handle);
}

void destroy_window(window* w) {
    DeleteObject(w->back_buffer.bitmap_handle);
    DeleteDC(w->back_buffer.device_context);
    DestroyWindow((HWND)w->handle);
    memset(w, 0, sizeof(window));
}

#ifdef VULKAN
VkSurfaceKHR create_vulkan_surface(window* w, VkInstance instance) {
    VkSurfaceKHR surface;
    if (vkCreateWin32SurfaceKHR(instance, &w->handle, NULL, &surface) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return surface;
}
#endif // VULKAN

#define BLOCK_SIZE 4096

void arena_align(arena_allocator* arena, size_t alignment) {
    size_t mask = alignment - 1;
    arena->bump_pointer = (uintptr_t)(((arena->bump_pointer + mask) & ~mask));
}

result create_arena_allocator(arena_allocator* arena) {
    if (arena == NULL) {
        return RESULT_FAILURE;
    }
    arena->start = NULL;
    arena->end = NULL;
    arena->bump_pointer = 0;

    void* block = VirtualAlloc(NULL, BLOCK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (block == NULL) {
        fprintf(stderr, "Failed to allocate memory for arena allocator\n");
        return RESULT_FAILURE;
    }

    *(void**)block = NULL; // Set the previous page pointer to the current start
    arena->start = block;
    arena->end = (void*)((uint8_t*)block + BLOCK_SIZE);
    arena->bump_pointer = (uintptr_t)((uint8_t*)block + sizeof(void*));
    return RESULT_SUCCESS;
}

void* arena_allocate_unaligned(arena_allocator* arena, size_t size) {
    void* result = NULL;
    if (arena->start == NULL || (arena->bump_pointer + size) > (uintptr_t)arena->end) {
        size_t allocation_size = size + sizeof(void*);
        size_t extended_block_size = allocation_size > BLOCK_SIZE ? allocation_size : BLOCK_SIZE;

        // Allocate the first page
        void* block = VirtualAlloc(NULL, extended_block_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (block == NULL) {
            return NULL;
        }

        *(void**)block = arena->start; // Set the previous page pointer to the current start
        arena->start = block;
        arena->end = (void*)((uint8_t*)block + extended_block_size);
        arena->bump_pointer = (uintptr_t)((uint8_t*)block + sizeof(void*));
    }

    result = (void*)arena->bump_pointer;
    arena->bump_pointer += size;
    return result;
}

void* arena_allocate(arena_allocator* arena, size_t size, size_t alignment) {
    arena_align(arena, alignment);
    void* result = arena_allocate_unaligned(arena, size);
    return result;
}

void arena_reset(arena_allocator* arena) {
    arena->bump_pointer = (uintptr_t)arena->start;
}

void destroy_arena_allocator(arena_allocator* arena) {
    // Free all allocated pages
    void* page = arena->start;
    while (page != NULL) {
        void** next_page = (void**)page;
        VirtualFree(page, 0, MEM_RELEASE);
        page = *next_page;
    }
}

#endif // _WIN32
