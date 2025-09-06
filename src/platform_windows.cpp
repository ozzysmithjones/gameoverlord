#include "fundamental.hpp"
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

static platform_stage current_stage = platform_stage::uninitialized;

//definitions for struct forward declarations in platform.hpp
namespace platform {
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

        enum_array<key, bool> keys_just_pressed;
        enum_array<key, bool> keys_just_released;
        enum_array<key, bool> keys_pressed;
        bool quit_requested = false;
    };

    struct window {
        HWND handle = nullptr;
        renderer renderer;
        input input;
    };
}

// implementation details for platform.hpp
namespace platform {
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
            resize_back_buffer(w->renderer, width, height);
            break;
        }
        case WM_CLOSE: {
            w->input.quit_requested = true;
            current_stage = platform_stage::shutting_down;
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            present_back_buffer(hdc, w->renderer);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_KEYDOWN: {
            bool fallback = false;
            int k = (int)wParam;
            if (k < static_cast<int>(key::count)) {
                w->input.keys_just_pressed.lookup(static_cast<key>(k), fallback) = true;
            }
        } break;
        case WM_KEYUP: {
            bool fallback = false;
            int k = (int)wParam;
            if (k < static_cast<int>(key::count)) {
                w->input.keys_just_released.lookup(static_cast<key>(k), fallback) = true;
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
            bool fallback = false;
            w->input.keys_just_pressed.lookup(key::left_mouse_button, fallback) = true;
        } break;
        case WM_LBUTTONUP: {
            bool fallback = false;
            w->input.keys_just_released.lookup(key::left_mouse_button, fallback) = true;
        } break;
        case WM_RBUTTONDOWN: {
            bool fallback = false;
            w->input.keys_just_pressed.lookup(key::right_mouse_button, fallback) = true;
        } break;
        case WM_RBUTTONUP: {
            bool fallback = false;
            w->input.keys_just_released.lookup(key::right_mouse_button, fallback) = true;
        } break;
        case WM_MBUTTONDOWN: {
            bool fallback = false;
            w->input.keys_just_pressed.lookup(key::middle_mouse_button, fallback) = true;
        } break;
        case WM_MBUTTONUP: {
            bool fallback = false;
            w->input.keys_just_released.lookup(key::middle_mouse_button, fallback) = true;
        } break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        return 0;
    }
}

// definitions for functions in platform.hpp
namespace platform {
    window& init() {
        static window main_window;
        if (current_stage != platform_stage::uninitialized) {
            BUG("Platform already initialized, no need to initialize it again.");
            return main_window;
        }

        // Initialize the window
    }

    void cleanup(window& window) {

    }

    input& update_input(window& window) {
        if (current_stage == platform_stage::uninitialized) {
            BUG("Platform must be uninitialized before calling update_input.");
            return window.input;
        }

        input& input = window.input;

        // Reset per-frame input states
        input.keys_just_pressed.set_everything(false);
        input.keys_just_released.set_everything(false);
        input.mouse = { 0 };

        // Process all pending messages
        MSG message;
        while (PeekMessage(&message, (HWND)window.handle, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                window.input.quit_requested = true;
                current_stage = platform_stage::shutting_down;
            }
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        // Update keys pressed/released states
        for (size_t i = 0; i < static_cast<size_t>(key::count); ++i) {
            key k = static_cast<key>(i);
            bool fallback = false;
            if (input.keys_just_pressed.lookup(k, fallback)) {
                input.keys_pressed.lookup(k, fallback) = true;
            }
        }

        for (size_t i = 0; i < static_cast<size_t>(key::count); ++i) {
            key k = static_cast<key>(i);
            bool fallback = false;
            if (input.keys_just_released.lookup(k, fallback)) {
                input.keys_pressed.lookup(k, fallback) = false;
            }
        }

        return input;
    }

    bool is_key_pressed(const input& input, key k) {
        bool fallback = false;
        return input.keys_pressed.lookup(k, fallback);
    }

    bool is_key_released(const input& input, key k) {
        bool fallback = false;
        return !input.keys_pressed.lookup(k, fallback);
    }

    bool is_key_just_pressed(const input& input, key k) {
        bool fallback = false;
        return input.keys_just_pressed.lookup(k, fallback);
    }

    bool is_key_just_released(const input& input, key k) {
        bool fallback = false;
        return input.keys_just_released.lookup(k, fallback);
    }

    bool is_quit_requested(const input& input) {
        return input.quit_requested;
    }
}