// #include "fundamental.hpp" <- precompiled header
#include "platform.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

enum class platform_stage {
    uninitialized,
    running,
    shutting_down,
    terminated,
};

//definitions for struct forward declarations in window.hpp
struct renderer {
    HDC device_context = nullptr;
    HBITMAP bitmap_handle = nullptr;
    BITMAPINFO bitmap_info = { 0 };
    void* memory = nullptr; // Pointer to the pixel data
    int width = 0;
    int height = 0;
};

struct input {
    struct {
        int32_t x = 0;
        int32_t y = 0;
        int32_t wheel = 0; // Positive for scroll up, negative for scroll down
    } mouse;

    enum_array<key, bool> keys_pressed_this_frame;
    enum_array<key, bool> keys_released_this_frame;
    enum_array<key, bool> keys_held_down;
    bool quit_requested = false;
};

struct window_internals {
    HWND handle = NULL;
    renderer renderer;
    input input;
};

constexpr size_t s = sizeof(window_internals);
static_assert(sizeof(window_internals) <= PLATFORM_WINDOW_SIZE_BYTES, "PLATFORM_WINDOW_SIZE_BYTES is too small for window_internals.");
static_assert(alignof(window_internals) <= PLATFORM_WINDOW_ALIGNMENT, "PLATFORM_WINDOW_ALIGNMENT is too small for window_internals.");

// implementation details for window.hpp
static inline void resize_back_buffer(renderer& renderer, long width, long height) {
    ASSERT(width > 0 && height > 0, return, "Width and height must be greater than zero.");

    if (renderer.bitmap_handle) {
        DeleteObject(renderer.bitmap_handle);
        renderer.bitmap_handle = nullptr;
        renderer.memory = nullptr;
    }

    renderer.bitmap_info = BITMAPINFO{
        .bmiHeader = {
            .biSize = sizeof(BITMAPINFOHEADER),
            .biWidth = width,
            .biHeight = height, // Negative height for top-down DIB
            .biPlanes = 1,
            .biBitCount = 32, // Assuming 32 bits per pixel (RGBA)
            .biCompression = BI_RGB,
            .biSizeImage = 0,
            .biXPelsPerMeter = 0,
            .biYPelsPerMeter = 0,
            .biClrUsed = 0,
            .biClrImportant = 0
        }
    };

    if (!renderer.device_context) {
        renderer.device_context = CreateCompatibleDC(NULL);
        ASSERT(renderer.device_context != NULL, return, "Failed to create compatible DC: %lu", GetLastError());
    }

    HBITMAP bitmap = CreateDIBSection(renderer.device_context, &renderer.bitmap_info, DIB_RGB_COLORS, &renderer.memory, NULL, 0);
    ASSERT(bitmap != NULL && renderer.memory != NULL, return, "Failed to create DIB section: %lu", GetLastError());

    renderer.width = width;
    renderer.height = height;
    renderer.bitmap_handle = bitmap;
}

static void present_back_buffer(HDC hdc, renderer& renderer) {
    ASSERT(renderer.memory != nullptr, return, "Renderer memory cannot be NULL for presentation.");

    StretchDIBits(
        hdc,
        0, 0, renderer.width, renderer.height,
        0, 0, renderer.width, renderer.height,
        renderer.memory,
        &renderer.bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

static LRESULT window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    window* w = (window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    window_internals& internals = *(window_internals*)w->get_internal_data();
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)(lParam);
        window* w = (window*)(pCreate->lpCreateParams);
        // store the pointer in the instance data of the window
        // so it could always be retrieved by using GetWindowLongPtr(hwnd, GWLP_USERDATA) 
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)w);
    } break;
    case WM_SIZE: {
        // attempt_copy_element area of the window that we can draw to.
        RECT draw_rect = { 0 };
        BOOL result = GetClientRect(hwnd, &draw_rect);
        if (result == 0) {
            BUG("GetClientRect failed: %lu", GetLastError());
            return 0;
        }
        uint32_t width = draw_rect.right - draw_rect.left;
        uint32_t height = draw_rect.bottom - draw_rect.top;
        resize_back_buffer(internals.renderer, width, height);
        break;
    }
    case WM_CLOSE: {
        internals.input.quit_requested = true;
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        present_back_buffer(hdc, internals.renderer);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_KEYDOWN: {
        int k = (int)wParam;
        if (k < static_cast<int>(key::count)) {
            internals.input.keys_pressed_this_frame.attempt_set_element(static_cast<key>(k), true);
        }
    } break;
    case WM_KEYUP: {
        int k = (int)wParam;
        if (k < static_cast<int>(key::count)) {
            internals.input.keys_released_this_frame.attempt_set_element(static_cast<key>(k), true);
        }
    }
    case WM_MOUSEMOVE: {
        internals.input.mouse.x = GET_X_LPARAM(lParam);
        internals.input.mouse.y = GET_Y_LPARAM(lParam);
    };
    case WM_MOUSEWHEEL: {
        internals.input.mouse.wheel = GET_WHEEL_DELTA_WPARAM(wParam);
    } break;
    case WM_LBUTTONDOWN: {
        internals.input.keys_pressed_this_frame.attempt_set_element(key::left_mouse_button, true);
    } break;
    case WM_LBUTTONUP: {
        internals.input.keys_released_this_frame.attempt_set_element(key::left_mouse_button, true);
    } break;
    case WM_RBUTTONDOWN: {
        internals.input.keys_pressed_this_frame.attempt_set_element(key::right_mouse_button, true);
    } break;
    case WM_RBUTTONUP: {
        internals.input.keys_released_this_frame.attempt_set_element(key::right_mouse_button, true);
    } break;
    case WM_MBUTTONDOWN: {
        internals.input.keys_pressed_this_frame.attempt_set_element(key::middle_mouse_button, true);
    } break;
    case WM_MBUTTONUP: {
        internals.input.keys_released_this_frame.attempt_set_element(key::middle_mouse_button, true);
    } break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

result create_window(string_slice title, int width, int height, window_mode mode, window& out_window) {
    window_internals& internals = *reinterpret_cast<window_internals*>(out_window.get_internal_data());

    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const char* class_name = "global_window_class";
    static bool class_registered = false;
    if (!class_registered) {
        WNDCLASSA wc = { 0 };
        wc.lpfnWndProc = window_proc;
        wc.hInstance = hInstance;
        wc.lpszClassName = class_name;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

        ATOM atom = RegisterClassA(&wc);
        ASSERT(atom != 0, return result::failure, "Failed to register window class: %lu", GetLastError());
        class_registered = true;
    }

    int32_t screen_width = GetSystemMetrics(SM_CXSCREEN);
    int32_t screen_height = GetSystemMetrics(SM_CYSCREEN);

    switch (mode) {
    case window_mode::windowed:
        style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        break;
    case window_mode::borderless_fullscreen:
        style = WS_POPUP | WS_VISIBLE;
        break;
    case window_mode::fullscreen:
        style = WS_POPUP | WS_VISIBLE;
        break;
    }

    internals.handle = CreateWindowExA(
        0, class_name, title.get_data(), style,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, hInstance, &out_window
    );

    ASSERT(internals.handle != NULL, return result::failure, "Failed to create window: %lu", GetLastError());
    ShowWindow((HWND)internals.handle, SW_SHOW);

    POINT point = { 0 };
    BOOL is_cursor_pos = GetCursorPos(&point);
    BOOL is_cursor_within_window = ScreenToClient((HWND)internals.handle, &point);
    if (is_cursor_pos && is_cursor_within_window) {
        internals.input.mouse.x = point.x;
        internals.input.mouse.y = point.y;
    }

    UpdateWindow((HWND)internals.handle);
    return result::success;
}

input& update_input(window& window) {
    window_internals& internals = *(window_internals*)window.get_internal_data();
    input& input = internals.input;

    // Reset per-frame input states
    input.keys_pressed_this_frame.set_everything(false);
    input.keys_released_this_frame.set_everything(false);
    input.mouse = { 0 };

    // Process all pending messages
    MSG message;
    while (PeekMessage(&message, (HWND)internals.handle, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            internals.input.quit_requested = true;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    // Update keys pressed/released states
    for (size_t i = 0; i < static_cast<size_t>(key::count); ++i) {
        key k = static_cast<key>(i);
        bool pressed = false;
        input.keys_pressed_this_frame.attempt_copy_element(k, pressed);
        if (pressed) {
            input.keys_held_down.attempt_set_element(k, true);
        }
    }

    for (size_t i = 0; i < static_cast<size_t>(key::count); ++i) {
        key k = static_cast<key>(i);
        bool released = false;
        input.keys_released_this_frame.attempt_copy_element(k, released);
        if (released) {
            input.keys_held_down.attempt_set_element(k, false);
        }
    }

    return input;
}

bool key_is_held_down(const input& input, key k) {
    bool value = false;
    input.keys_held_down.attempt_copy_element(k, value);
    return value;
}

bool key_is_not_held_down(const input& input, key k) {
    bool value = false;
    input.keys_held_down.attempt_copy_element(k, value);
    return !value;
}

bool key_is_pressed_this_frame(const input& input, key k) {
    bool value = false;
    input.keys_pressed_this_frame.attempt_copy_element(k, value);
    return value;
}

bool key_is_released_this_frame(const input& input, key k) {
    bool value = false;
    input.keys_released_this_frame.attempt_copy_element(k, value);
    return value;
}

bool quit_is_requested(const input& input) {
    return input.quit_requested;
}

renderer& begin_drawing(window& window) {
    BUG("begin_drawing is not implemented yet.");
    window_internals& internals = *(window_internals*)window.get_internal_data();
    return internals.renderer;
}

void end_drawing(renderer& renderer) {
    BUG("end_drawing is not implemented yet.");
}

window::window() {
    std::memset(this->get_internal_data(), 0, sizeof(window_internals));
}

window::~window() {
    window_internals& internals = *(window_internals*)get_internal_data();
    if (internals.renderer.bitmap_handle) {
        DeleteObject(internals.renderer.bitmap_handle);
        internals.renderer.bitmap_handle = nullptr;
        internals.renderer.memory = nullptr;
    }
    if (internals.renderer.device_context) {
        DeleteDC(internals.renderer.device_context);
        internals.renderer.device_context = nullptr;
    }
    if (internals.handle) {
        DestroyWindow(internals.handle);
        internals.handle = NULL;
    }
}

window::window(window&& other) noexcept {
    std::memcpy(this->get_internal_data(), other.get_internal_data(), sizeof(window_internals));
    std::memset(other.get_internal_data(), 0, sizeof(window_internals));
}


