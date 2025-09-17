#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include "platform_layer.h"

// Native memory information structure, filled during platform layer initialization. Contains page size and memory limits for memory allocation functions.
static SYSTEM_INFO native_memory_info;

result create_platform_layer(void) {
    // Init the native memory info structure. So we don't need to keep quering the OS for page size and memory limits.
    GetSystemInfo(&native_memory_info);

    return RESULT_SUCCESS;
}

void destroy_platform_layer(void) {

}

result create_bump_allocator(bump_allocator* allocator, size_t min_capacity, size_t max_capacity) {
    ASSERT(allocator != NULL, return RESULT_FAILURE, "Allocator cannot be NULL");
    ASSERT(memcmp(&native_memory_info, &(SYSTEM_INFO) {
        0
    }, sizeof(native_memory_info)) != 0, return RESULT_FAILURE, "Native memory info struct should not be zero (maybe you forgot to call create_platform_layer?).");

    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T largest_space = 0;
    LPVOID address = native_memory_info.lpMinimumApplicationAddress;
    LPVOID allocator_base_address = NULL;

    while (address < native_memory_info.lpMaximumApplicationAddress) {
        if (VirtualQuery(address, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            if (mbi.State == MEM_FREE) {
                if (mbi.RegionSize > largest_space) {
                    largest_space = mbi.RegionSize;
                    allocator_base_address = mbi.BaseAddress;

                    if (largest_space >= max_capacity) {
                        // Found a big enough space, no need to look further.
                        break;
                    }
                }
            }
            address = (LPBYTE)address + mbi.RegionSize;
        }
        else {
            break;
        }
    }

    allocator_base_address = (LPVOID)(((uintptr_t)allocator_base_address + (native_memory_info.dwAllocationGranularity - 1)) & ~(native_memory_info.dwAllocationGranularity - 1)); // Align to allocation granularity.
    largest_space -= (uintptr_t)allocator_base_address - (uintptr_t)mbi.BaseAddress;

    if (largest_space < min_capacity) {
        BUG("Not enough virtual memory available to reserve space for bump allocator.");
        return RESULT_FAILURE;
    }

    if (largest_space > max_capacity) {
        largest_space = max_capacity;
    }

    allocator->base = VirtualAlloc(allocator_base_address, largest_space, MEM_RESERVE, PAGE_READWRITE);
    allocator->used_bytes = 0;
    allocator->next_page_bytes = 0;
    allocator->capacity = largest_space;

    if (!allocator->base) {
        BUG("Failed to reserve virtual memory for bump allocator.");
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
    uint32_t width;
    uint32_t height;
    input input_state;
} window_internals;

STATIC_ASSERT(sizeof(window) == sizeof(window_internals), window_struct_too_small);
STATIC_ASSERT(alignof(window) >= alignof(window_internals), window_struct_misaligned);

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

    return RESULT_SUCCESS;
}

input* update_window_input(window* w) {
    ASSERT(w != NULL, return NULL, "Window pointer cannot be NULL");
    window_internals* win = (window_internals*)&w->internals;
    input* input_state = &win->input_state;
    memset(input_state->keys_modified_this_frame_bitset, 0, sizeof(input_state->keys_modified_this_frame_bitset));

    // Poll for events
    MSG msg;
    while (PeekMessage(&msg, win->handle, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return input_state;
}

void destroy_window(window* window) {
    ASSERT(window != NULL, return, "Window pointer cannot be NULL");
    window_internals* w = (window_internals*)&window->internals;
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