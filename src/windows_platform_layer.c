#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

/* Need relevant includes for creating a window surface for vulkan graphics API*/
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>

#include "renderer.h"
#include "platform_layer.h"

bump_allocator permanent_allocator;
bump_allocator temp_allocator;

// Native memory information structure, filled during platform layer initialization. Contains page size and memory limits for memory allocation functions.
static SYSTEM_INFO native_memory_info;

result create_platform_layer(memory_requirements* requirements) {
    // Init the native memory info structure. So we don't need to keep quering the OS for page size and memory limits.
    GetSystemInfo(&native_memory_info);

    memory_requirements default_requirements = {
        .permanent_allocator_capacity = 1024 * 1024 * 1024, // 1 GB
        .temp_allocator_capacity = 64 * 1024 * 1024         // 64 MB
    };

    if (requirements == NULL) {
        requirements = &default_requirements;
    }

    if (create_bump_allocator(&temp_allocator, requirements->temp_allocator_capacity) != RESULT_SUCCESS) {
        BUG("Failed to create temporary allocator, probably not enough virtual memory available.");
        return RESULT_FAILURE;
    }

    if (create_bump_allocator(&permanent_allocator, requirements->permanent_allocator_capacity) != RESULT_SUCCESS) {
        BUG("Failed to create permanent allocator, probably not enough remaining virtual memory available.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

void destroy_platform_layer(void) {
    destroy_bump_allocator(&temp_allocator);
    destroy_bump_allocator(&permanent_allocator);
}

result create_bump_allocator(bump_allocator* allocator, size_t capacity) {
    ASSERT(allocator != NULL, return RESULT_FAILURE, "Allocator cannot be NULL");
    ASSERT(memcmp(&native_memory_info, &(SYSTEM_INFO) {
        0
    }, sizeof(native_memory_info)) != 0, return RESULT_FAILURE, "Native memory info struct should not be zero (maybe you forgot to call create_platform_layer?).");


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
    renderer renderer;
} window_internals;

STATIC_ASSERT(sizeof(window) == sizeof(window_internals), window_struct_too_small);
STATIC_ASSERT(alignof(window) >= alignof(window_internals), window_struct_misaligned);

// Vulkan API related functions that need to be implemented in the platform layer .c file.

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

// ^^^ Vulkan API related functions that need to be implemented in the platform layer .c file. ^^^

static LRESULT window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    window_internals* w = (window_internals*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)(lParam);
        window_internals* w = (window_internals*)(pCreate->lpCreateParams);
        ASSERT(w != NULL, return -1, "Window internals cannot be NULL in WM_CREATE");
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)w);
        break;
    }
    case WM_CLOSE:
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_CLOSE");
        w->input_state.closed_window = true;
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


result create_window(window* window, const char* title, uint32_t width, uint32_t height, window_mode mode) {
    ASSERT(window != NULL, return RESULT_FAILURE, "Window pointer cannot be NULL");
    const char* class_name = "window_class";
    HINSTANCE process_instance = GetModuleHandle(NULL);

    window_internals* w = (window_internals*)&window->internals;
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

    if (!create_renderer(window, &w->renderer)) {
        BUG("Failed to create renderer for window.");
        DestroyWindow(w->handle);
        memset(w, 0, sizeof(*w));
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

window_size get_window_size(window* window) {
    ASSERT(window != NULL, return (window_size){0}, "Window pointer cannot be NULL");
    window_internals* w = (window_internals*)&window->internals;
    ASSERT(w->handle != NULL, return (window_size){0}, "Window handle cannot be NULL");
    RECT rect;
    if (GetClientRect(w->handle, &rect)) {
        return (window_size) {
            rect.right - rect.left, rect.bottom - rect.top
        };
    }
    BUG("Failed to get window size.");
    return (window_size) {
        0
    };
}

input* update_window_input(window* window) {
    ASSERT(window != NULL, return NULL, "Window pointer cannot be NULL");
    window_internals* w = (window_internals*)&window->internals;
    input* input_state = &w->input_state;
    memset(input_state->keys_modified_this_frame_bitset, 0, sizeof(input_state->keys_modified_this_frame_bitset));

    // Poll for events
    MSG msg;
    while (PeekMessage(&msg, w->handle, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return input_state;
}

void destroy_window(window* window) {
    ASSERT(window != NULL, return, "Window pointer cannot be NULL");
    window_internals* w = (window_internals*)&window->internals;
    destroy_renderer(&w->renderer);
    DestroyWindow(w->handle);
    memset(w, 0, sizeof(*w));
}

bool is_window_closed(input* input_state) {
    ASSERT(input_state != NULL, return true, "Input state cannot be NULL");
    return input_state->closed_window;
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