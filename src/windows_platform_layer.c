#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include "platform_layer.h"

#define APP_BACKEND
#ifdef APP_BACKEND
#define HOT_RELOAD_HOST
#ifdef HOT_RELOAD_HOST
result(*init)(init_in_params* params, init_out_params* out_params);
result(*update)(update_params* params);
void (*shutdown)(shutdown_params* params);
#else
extern result init(init_in_params* in, init_out_params* out);
extern result update(update_params* in);
extern void shutdown(shutdown_params* in);
#endif // HOT_RELOAD_HOST
#endif // APP_BACKEND

result create_bump_allocator(bump_allocator* allocator, size_t capacity) {
    ASSERT(allocator != NULL, return RESULT_FAILURE, "Allocator cannot be NULL");

    allocator->base = VirtualAlloc(NULL, capacity, MEM_RESERVE, PAGE_READWRITE);
    allocator->used_bytes = 0;
    allocator->next_page_bytes = 0;
    allocator->capacity = capacity;

    if (!allocator->base) {
        BUG("Failed to reserve virtual memory for bump allocator. Error: %d", GetLastError());
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

void destroy_bump_allocator(bump_allocator* allocator) {
    ASSERT(allocator != NULL, return, "Allocator cannot be NULL");
    if (allocator->base) {
        VirtualFree(allocator->base, 0, MEM_RELEASE);
        memset(allocator, 0, sizeof(*allocator));
    }
}

void* bump_allocate(bump_allocator* allocator, size_t alignment, size_t bytes) {
    ASSERT(allocator != NULL, return NULL, "Allocator cannot be NULL");
    ASSERT(alignment && (alignment & (alignment - 1)) == 0, alignment = 1, "alignment must be a power of two");

    size_t aligned = (allocator->used_bytes + (alignment - 1)) & ~(alignment - 1);
    size_t new_used_bytes = aligned + bytes;
    if (new_used_bytes > allocator->capacity) {
        BUG("Out of memory for bump allocator.");
        return NULL;
    }

    if (new_used_bytes > allocator->next_page_bytes) {
        SYSTEM_INFO native_memory_info;
        GetSystemInfo(&native_memory_info);

        // Need to commit more memory.
        SIZE_T commit_size = ((new_used_bytes - allocator->next_page_bytes) + (native_memory_info.dwPageSize - 1)) & ~(native_memory_info.dwPageSize - 1);
        if (VirtualAlloc((LPBYTE)allocator->base + allocator->next_page_bytes, commit_size, MEM_COMMIT, PAGE_READWRITE) == NULL) {
            BUG("Failed to commit more memory for bump allocator.");
            return NULL;
        }

        allocator->next_page_bytes += commit_size;
    }

    allocator->used_bytes = new_used_bytes;
    return (LPBYTE)allocator->base + aligned;
}

result create_clock(clock* clock) {
    ASSERT(clock != NULL, return RESULT_FAILURE, "Clock cannot be NULL");

    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency)) {
        BUG("Failed to query performance frequency.");
        return RESULT_FAILURE;
    }

    LARGE_INTEGER current_time;
    if (!QueryPerformanceCounter(&current_time)) {
        BUG("Failed to query performance counter.");
        return RESULT_FAILURE;
    }

    clock->frequency = (float)frequency.QuadPart;
    clock->creation_time = (float)current_time.QuadPart;
    clock->time_since_creation = 0.0f;
    clock->time_since_previous_update = 0.0f;

    return RESULT_SUCCESS;
}

void update_clock(clock* clock) {
    ASSERT(clock != NULL, return, "Clock cannot be NULL");

    LARGE_INTEGER current_time;
    if (!QueryPerformanceCounter(&current_time)) {
        BUG("Failed to query performance counter.");
        return;
    }

    clock->time_since_previous_update = (float)(current_time.QuadPart - clock->time_since_creation) / clock->frequency;
    clock->time_since_creation = (float)current_time.QuadPart - clock->creation_time;
}

bool file_exists(string path) {
    DWORD file_attributes = GetFileAttributesA(path.text);
    return (file_attributes != INVALID_FILE_ATTRIBUTES);
}

result read_entire_file(string path, bump_allocator* allocator, string* out_file_contents) {
    ASSERT(allocator != NULL, return RESULT_FAILURE, "Allocator cannot be NULL");
    ASSERT(out_file_contents != NULL, return RESULT_FAILURE, "Output file contents cannot be NULL");

    HANDLE file_handle = CreateFileA(path.text, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        BUG("Failed to open file for reading: %.*s", path.length, path.text);
        return RESULT_FAILURE;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle, &file_size)) {
        BUG("Failed to get file size for reading: %.*s", path.length, path.text);
        CloseHandle(file_handle);
        return RESULT_FAILURE;
    }

    if (file_size.QuadPart > UINT32_MAX) {
        BUG("File too large to read into memory: %.*s", path.length, path.text);
        CloseHandle(file_handle);
        return RESULT_FAILURE;
    }

    void* buffer = bump_allocate(allocator, 1, (size_t)file_size.QuadPart);
    if (buffer == NULL) {
        BUG("Failed to allocate memory for reading file: %.*s", path.length, path.text);
        CloseHandle(file_handle);
        return RESULT_FAILURE;
    }
    DWORD bytes_read;
    if (!ReadFile(file_handle, buffer, (DWORD)file_size.QuadPart, &bytes_read, NULL) || bytes_read != (DWORD)file_size.QuadPart) {
        BUG("Failed to read file: %.*s", path.length, path.text);
        CloseHandle(file_handle);
        return RESULT_FAILURE;
    }
    CloseHandle(file_handle);
    out_file_contents->text = (const char*)buffer;
    out_file_contents->length = (uint32_t)file_size.QuadPart;
    return RESULT_SUCCESS;
}

result write_entire_file(string path, const void* data, size_t size) {
    ASSERT(data != NULL, return RESULT_FAILURE, "Data pointer cannot be NULL");
    ASSERT(size > 0, return RESULT_FAILURE, "Size must be greater than zero");

    HANDLE file_handle = CreateFileA(path.text, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        BUG("Failed to open file for writing: %.*s", path.length, path.text);
        return RESULT_FAILURE;
    }
    DWORD bytes_written;
    if (!WriteFile(file_handle, data, (DWORD)size, &bytes_written, NULL) || bytes_written != (DWORD)size) {
        BUG("Failed to write file: %.*s", path.length, path.text);
        CloseHandle(file_handle);
        return RESULT_FAILURE;
    }
    CloseHandle(file_handle);
    return RESULT_SUCCESS;
}

typedef struct input {
    uint64_t keys_pressed_bitset[4];
    uint64_t keys_modified_this_frame_bitset[4];
    int32_t mouse_x;
    int32_t mouse_y;
    bool closed_window;
} input;

typedef struct {
    HWND handle;
    HDC hdc;
    input input_state;
} window;


// Vulkan API related functions that need to be implemented in the platform layer .c file.

/*
VkSurfaceKHR create_window_surface(VkInstance instance, window* window) {
    ASSERT(instance != VK_NULL_HANDLE, return VK_NULL_HANDLE, "Vulkan instance cannot be NULL");
    ASSERT(window != NULL, return VK_NULL_HANDLE, "Window pointer cannot be NULL");
    window_internals* w = (window_internals*)&window->internals;
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandle(NULL),
        .hwnd = w->handle,
    };
#endif
    VkSurfaceKHR surface;
    if (vkCreateWin32SurfaceKHR(instance, &create_info, NULL, &surface) != VK_SUCCESS) {
        BUG("Failed to create Win32 Vulkan surface.");
        return VK_NULL_HANDLE;
    }
    return surface;
}

bool check_window_presentation_support(VkPhysicalDevice device, uint32_t queue_family_index, VkSurfaceKHR window_surface) {
    ASSERT(device != VK_NULL_HANDLE, return false, "Physical device cannot be NULL");
    ASSERT(window_surface != VK_NULL_HANDLE, return false, "Window surface cannot be NULL");
    return vkGetPhysicalDeviceWin32PresentationSupportKHR(device, queue_family_index);
}
    */

    // ^^^ Vulkan API related functions that need to be implemented in the platform layer .c file. ^^^

static LRESULT window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    window* w = (window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)(lParam);
        window* w = (window*)(pCreate->lpCreateParams);
        ASSERT(w != NULL, return -1, "Window internals cannot be NULL in WM_CREATE");
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)w);
        break;
    }
    case WM_CLOSE:
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_CLOSE");
        w->input_state.closed_window = true;
        PostQuitMessage(0);
        break;
    case WM_DESTROY:
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_DESTROY");
        w->input_state.closed_window = true;
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_KEYDOWN/WM_SYSKEYDOWN");
        int key = (int)wParam;
        ASSERT(key >= 0 && key < 256, return 0, "Unrecognized key code %d found in WM_KEYDOWN/WM_SYSKEYDOWN event.", key);
        size_t index = key >> 6;
        uint64_t mask = (uint64_t)1 << (key & 63);
        w->input_state.keys_pressed_bitset[index] |= mask;
        w->input_state.keys_modified_this_frame_bitset[index] |= mask;
    } break;
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_KEYUP/WM_SYSKEYUP");
        int key = (int)wParam;
        ASSERT(key >= 0 && key < 256, return 0, "Unrecognized key code %d found in WM_KEYUP/WM_SYSKEYUP event.", key);
        size_t index = key >> 6;
        uint64_t mask = (uint64_t)1 << (key & 63);
        w->input_state.keys_pressed_bitset[index] &= ~mask;
        w->input_state.keys_modified_this_frame_bitset[index] |= mask;
    } break;
    case WM_MOUSEMOVE: {
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_MOUSEMOVE");
        w->input_state.mouse_x = GET_X_LPARAM(lParam);
        w->input_state.mouse_y = GET_Y_LPARAM(lParam);
    } break;
    default: {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    }

    return 0;
}

typedef enum {
    WINDOW_MODE_WINDOWED,
    WINDOW_MODE_FULLSCREEN,
    WINDOW_MODE_BORDERLESS_FULLSCREEN
} window_mode;

static result create_window(window* w, const char* title, uint32_t width, uint32_t height, window_mode mode) {
    ASSERT(w != NULL, return RESULT_FAILURE, "Window pointer cannot be NULL");
    const char* class_name = "window_class";
    HINSTANCE process_instance = GetModuleHandle(NULL);
    memset(w, 0, sizeof(*w));

    static bool class_registered = false;
    if (!class_registered) {
        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = window_proc;
        wc.hInstance = process_instance;
        wc.lpszClassName = class_name;
        if (!RegisterClass(&wc)) {
            BUG("Failed to register window class.");
            return RESULT_FAILURE;
        }
        class_registered = true;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
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

    w->handle = CreateWindowEx(
        0,
        class_name,
        title,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL,
        NULL,
        process_instance,
        w
    );

    if (!w->handle) {
        BUG("Failed to create window.");
        return RESULT_FAILURE;
    }

    ShowWindow((HWND)w->handle, SW_SHOW);
    UpdateWindow((HWND)w->handle);
    return RESULT_SUCCESS;
}

typedef struct window_size {
    uint32_t width;
    uint32_t height;
} window_size;

static window_size get_window_size(window* window) {
    ASSERT(window != NULL, return (window_size){0}, "Window pointer cannot be NULL");
    RECT rect;
    if (GetClientRect(window->handle, &rect)) {
        return (window_size) {
            rect.right - rect.left, rect.bottom - rect.top
        };
    }
    BUG("Failed to get window size.");
    return (window_size) {
        0
    };
}

static input* update_window_input(window* window) {
    ASSERT(window != NULL, return NULL, "Window pointer cannot be NULL");
    input* input_state = &window->input_state;
    memset(input_state->keys_modified_this_frame_bitset, 0, sizeof(input_state->keys_modified_this_frame_bitset));

    // Poll for events
    MSG msg;
    while (PeekMessage(&msg, window->handle, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            input_state->closed_window = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return input_state;
}

static void destroy_window(window* window) {
    ASSERT(window != NULL, return, "Window pointer cannot be NULL");
    DestroyWindow(window->handle);
    memset(window, 0, sizeof(*window));
}

bool is_key_down(input* input_state, keyboard_key key) {
    ASSERT(input_state != NULL, return false, "Input state cannot be NULL");
    ASSERT(key >= 0 && key < 256, return false, "Key %d out of range", key);
    size_t index = key >> 6;
    uint64_t mask = (uint64_t)1 << (key & 63);
    return (input_state->keys_pressed_bitset[index] & mask) != 0 &&
        (input_state->keys_modified_this_frame_bitset[index] & mask) != 0;
}

bool is_key_held_down(input* input_state, keyboard_key key) {
    ASSERT(input_state != NULL, return false, "Input state cannot be NULL");
    ASSERT(key >= 0 && key < 256, return false, "Key %d out of range", key);
    size_t index = key >> 6;
    uint64_t mask = (uint64_t)1 << (key & 63);
    return (input_state->keys_pressed_bitset[index] & mask) != 0;
}

bool is_key_up(input* input_state, keyboard_key key) {
    ASSERT(input_state != NULL, return false, "Input state cannot be NULL");
    ASSERT(key >= 0 && key < 256, return false, "Key %d out of range", key);
    size_t index = key >> 6;
    uint64_t mask = (uint64_t)1 << (key & 63);
    return (input_state->keys_pressed_bitset[index] & mask) == 0 &&
        (input_state->keys_modified_this_frame_bitset[index] & mask) != 0;
}

#ifdef APP_BACKEND

#ifdef HOT_RELOAD_HOST
#ifndef HOT_RELOAD_DLL_PATH
#define HOT_RELOAD_DLL_PATH "game.dll"
#define HOT_RELOAD_DLL_TEMP_PATH HOT_RELOAD_DLL_PATH ".temp"
#endif

static HMODULE app_dll = NULL;

typedef enum {
    HOT_RELOAD_IF_DLL_UPDATED,
    HOT_RELOAD_FORCED,
} hot_reload_condition;

static bool potential_hot_reload(hot_reload_condition hot_reload_condition) {
    static uint64_t last_edit_time = 0;
    uint64_t edit_time = last_edit_time;

    if (hot_reload_condition == HOT_RELOAD_IF_DLL_UPDATED) {
        // Check the last edit time of the DLL file
        // If the DLL file is updated, the time will be different, and we need to reload the DLL
        WIN32_FILE_ATTRIBUTE_DATA file_info;
        if (!GetFileAttributesExA(HOT_RELOAD_DLL_PATH, GetFileExInfoStandard, &file_info)) {
            BUG("Failed to get file attributes for %s", HOT_RELOAD_DLL_PATH);
            return false;
        }

        edit_time = (((uint64_t)file_info.ftLastWriteTime.dwHighDateTime) << 32) | file_info.ftLastWriteTime.dwLowDateTime;
        if (edit_time <= last_edit_time) {
            return false; // No change
        }
    }
    else {
        DEBUG_ASSERT(hot_reload_condition == HOT_RELOAD_FORCED, return false, "Invalid hot reload condition");
    }

    if (app_dll) {
        FreeLibrary(app_dll);
        app_dll = NULL;
        init = NULL;
        update = NULL;
        shutdown = NULL;
    }

    // Copy the DLL to a temporary file to avoid locking issues
    if (!CopyFileA(HOT_RELOAD_DLL_PATH, HOT_RELOAD_DLL_TEMP_PATH, FALSE)) {
        BUG("Failed to copy %s to %s", HOT_RELOAD_DLL_PATH, HOT_RELOAD_DLL_TEMP_PATH);
        return false;
    }

    app_dll = LoadLibraryA(HOT_RELOAD_DLL_TEMP_PATH);
    if (!app_dll) {
        BUG("Failed to load %s", HOT_RELOAD_DLL_TEMP_PATH);
        return false;
    }

    init = (result(*)(init_in_params*, init_out_params*))GetProcAddress(app_dll, "init");
    update = (result(*)(update_params*))GetProcAddress(app_dll, "update");
    shutdown = (void(*)(shutdown_params*))GetProcAddress(app_dll, "shutdown");
    if (!init || !update || !shutdown) {
        BUG("Failed to get function addresses from %s", HOT_RELOAD_DLL_PATH);
        return false;
    }

    last_edit_time = edit_time;
    return true;
}

#endif // HOT_RELOAD_HOST

static struct {
    memory_allocators memory_allocators;
    window window;
    clock clock;
    void* app_state;
} app; // <- this static variable is only used globally in WinMain, create_app() and destroy_app() (but it's members may be passed to function calls)

static result create_app(void) {
    if (create_clock(&app.clock) != RESULT_SUCCESS) {
        BUG("Failed to create application clock (for measuring delta time).");
        return RESULT_FAILURE;
    }

    if (create_bump_allocator(&app.memory_allocators.permanent_allocator, 1024 * 1024 * 1024) != RESULT_SUCCESS) {
        BUG("Failed to create permanent memory allocator.");
        return RESULT_FAILURE;
    }

    if (create_bump_allocator(&app.memory_allocators.temp_allocator, 64 * 1024 * 1024) != RESULT_SUCCESS) {
        BUG("Failed to create temporary memory allocator.");
        return RESULT_FAILURE;
    }

    if (create_window(&app.window, "Game Overlord", 1280, 720, WINDOW_MODE_WINDOWED) != RESULT_SUCCESS) {
        BUG("Failed to create application window.");
        return RESULT_FAILURE;
    }

    init_in_params in_params = { 0 };
    in_params.memory_allocators = &app.memory_allocators;
    init_out_params out_params = { 0 };

    if (init(&in_params, &out_params) != RESULT_SUCCESS) {
        BUG("Failed to initialize app.");
        return RESULT_FAILURE;
    }

    app.app_state = out_params.app_state;
    return RESULT_SUCCESS;
}

static void destroy_app(void) {
    shutdown_params shutdown_params = { 0 };
    shutdown_params.app_state = app.app_state;
    shutdown_params.memory_allocators = &app.memory_allocators;
    shutdown(&shutdown_params);

    destroy_window(&app.window);
    destroy_bump_allocator(&app.memory_allocators.permanent_allocator);
    destroy_bump_allocator(&app.memory_allocators.temp_allocator);

#ifdef HOT_RELOAD_HOST
    FreeLibrary(app_dll);
#endif
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
#ifdef HOT_RELOAD_HOST
    if (!potential_hot_reload(HOT_RELOAD_FORCED)) {
        BUG("Failed to load initial app DLL.");
        FreeLibrary(app_dll);
        return -1;
    }
#endif

    if (create_app() != RESULT_SUCCESS) {
        BUG("Failed to create application.");
        goto cleanup;
    }

    /*-----------------------------------------------------------------*/
    // Main loop
    while (1) {
        update_clock(&app.clock);
        reset_bump_allocator(&app.memory_allocators.temp_allocator);

#ifdef HOT_RELOAD_HOST
        potential_hot_reload(HOT_RELOAD_IF_DLL_UPDATED);
#endif
        input* input_state = update_window_input(&app.window);
        if (input_state->closed_window) {
            break;
        }

        {
            update_params update_params = { 0 };
            update_params.clock = app.clock;
            update_params.app_state = app.app_state;
            update_params.memory_allocators = &app.memory_allocators;
            update_params.input = input_state;

            if (update(&update_params) != RESULT_SUCCESS) {
                BUG("Failed to update app.");
                break;
            }
        }
    }

cleanup:
    destroy_app();
    return 0;
}

#endif