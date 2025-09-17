#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
#include <stdalign.h>
#include <stdbool.h>

#include "fundamental.h"

#ifdef VULKAN
#include <vulkan/vulkan.h>
#endif

// Mutex and thread structures
// Internals are platform-specific, but the interface remains consistent across platforms.
// We use a fixed-size array to store the platform-specific data, ensuring that the size and alignment are consistent.

typedef struct {
#ifdef _WIN32
    alignas(8) uint8_t data[40];
#else
#error Unsupported platform for mutex structure
#endif
} mutex;

typedef struct {
#ifdef _WIN32
    alignas(8) uint8_t data[8];
#else
#error Unsupported platform for thread structure
#endif
} thread;

typedef struct {
    alignas(8) uint8_t data[8];
} condition_variable;

void create_mutex(mutex* m);
void lock_mutex(mutex* m);
void unlock_mutex(mutex* m);
void destroy_mutex(mutex* m);

result create_thread(thread* t, unsigned long (*start_routine)(void*), void* arg);
void join_thread(thread* t);
void destroy_thread(thread* t);

void init_condition_variable(condition_variable* cv);
void signal_condition_variable(condition_variable* cv);
void wait_condition_variable(condition_variable* cv, mutex* m);

typedef enum {
    KEY_NONE,
    KEY_BACKSPACE = 8,
    KEY_TAB = 9,
    KEY_ENTER = 13,
    KEY_SHIFT = 16,
    KEY_CTRL = 17,
    KEY_ALT = 18,
    KEY_PAUSE = 19,
    KEY_CAPS_LOCK = 20,
    KEY_ESCAPE = 27,
    KEY_SPACE = 32,
    KEY_PAGE_UP = 33,
    KEY_PAGE_DOWN = 34,
    KEY_END = 35,
    KEY_HOME = 36,
    KEY_LEFT = 37,
    KEY_UP = 38,
    KEY_RIGHT = 39,
    KEY_DOWN = 40,
    KEY_SELECT = 41,
    KEY_PRINT = 42,
    KEY_EXEC = 43,
    KEY_PRINT_SCREEN = 44,
    KEY_INSERT = 45,
    KEY_DELETE = 46,
    KEY_HELP = 47,
    KEY_0 = 48,
    KEY_1 = 49,
    KEY_2 = 50,
    KEY_3 = 51,
    KEY_4 = 52,
    KEY_5 = 53,
    KEY_6 = 54,
    KEY_7 = 55,
    KEY_8 = 56,
    KEY_9 = 57,
    KEY_A = 65,
    KEY_B = 66,
    KEY_C = 67,
    KEY_D = 68,
    KEY_E = 69,
    KEY_F = 70,
    KEY_G = 71,
    KEY_H = 72,
    KEY_I = 73,
    KEY_J = 74,
    KEY_K = 75,
    KEY_L = 76,
    KEY_M = 77,
    KEY_N = 78,
    KEY_O = 79,
    KEY_P = 80,
    KEY_Q = 81,
    KEY_R = 82,
    KEY_S = 83,
    KEY_T = 84,
    KEY_U = 85,
    KEY_V = 86,
    KEY_W = 87,
    KEY_X = 88,
    KEY_Y = 89,
    KEY_Z = 90,
    KEY_LEFT_WINDOWS = 91,
    KEY_RIGHT_WINDOWS = 92,
    KEY_APPLICATION = 93,
    KEY_SLEEP = 95,
    KEY_NUMPAD_0 = 96,
    KEY_NUMPAD_1 = 97,
    KEY_NUMPAD_2 = 98,
    KEY_NUMPAD_3 = 99,
    KEY_NUMPAD_4 = 100,
    KEY_NUMPAD_5 = 101,
    KEY_NUMPAD_6 = 102,
    KEY_NUMPAD_7 = 103,
    KEY_NUMPAD_8 = 104,
    KEY_NUMPAD_9 = 105,
    KEY_MULTIPLY = 106,
    KEY_ADD = 107,
    KEY_SEPARATOR = 108,
    KEY_SUBTRACT = 109,
    KEY_DECIMAL = 110,
    KEY_DIVIDE = 111,
    KEY_F1 = 112,
    KEY_F2 = 113,
    KEY_F3 = 114,
    KEY_F4 = 115,
    KEY_F5 = 116,
    KEY_F6 = 117,
    KEY_F7 = 118,
    KEY_F8 = 119,
    KEY_F9 = 120,
    KEY_F10 = 121,
    KEY_F11 = 122,
    KEY_F12 = 123,
    KEY_F13 = 124,
    KEY_F14 = 125,
    KEY_F15 = 126,
    KEY_F16 = 127,
    KEY_F17 = 128,
    KEY_F18 = 129,
    KEY_F19 = 130,
    KEY_F20 = 131,
    KEY_F21 = 132,
    KEY_F22 = 133,
    KEY_F23 = 134,
    KEY_F24 = 135,
    KEY_NUM_LOCK = 144,
    KEY_SCROLL_LOCK = 145,
    KEY_LEFT_SHIFT = 160,
    KEY_RIGHT_SHIFT = 161,
    KEY_LEFT_CTRL = 162,
    KEY_RIGHT_CTRL = 163,
    KEY_LEFT_ALT = 164,
    KEY_RIGHT_ALT = 165,
} keyboard_key;

typedef enum {
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE,
} mouse_button;

typedef enum {
    WINDOW_EVENT_NONE,
    WINDOW_EVENT_CLOSE,
    WINDOW_EVENT_RESIZE,
    WINDOW_EVENT_KEY_DOWN,
    WINDOW_EVENT_KEY_UP,
    WINDOW_EVENT_CHARACTER,
    WINDOW_EVENT_MOUSE_MOVE,
    WINDOW_EVENT_MOUSE_BUTTON_DOWN,
    WINDOW_EVENT_MOUSE_BUTTON_UP,
    WINDOW_EVENT_MOUSE_WHEEL,
    WINDOW_EVENT_FOCUS_GAINED,
    WINDOW_EVENT_FOCUS_LOST,
    WINDOW_EVENT_MOUSE_ENTER,
    WINDOW_EVENT_MOUSE_LEAVE,
} window_event_type;

typedef struct {
    bool up : 1;
    bool down : 1;
} key_input;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t wheel;
    bool left_button_down;
    bool left_button_up;
    bool right_button_down;
    bool right_button_up;
    bool middle_button_down;
    bool middle_button_up;
} mouse_input;

typedef struct {
    key_input keys[256];
    mouse_input mouse;
    bool closed_window;
} user_input;

typedef enum {
    WINDOW_MODE_WINDOWED,
    WINDOW_MODE_FULLSCREEN,
    WINDOW_MODE_BORDERLESS_FULLSCREEN,
} window_mode;

typedef struct {
    uint32_t width;
    uint32_t height;
    void* memory;
    void* bitmap_handle;
    void* device_context;
} back_buffer;

typedef struct {
    back_buffer back_buffer;
    user_input input;
    void* handle;
} window;

result create_window(window* w, const char* title, int32_t width, int32_t height, window_mode mode);

/// @brief After calling this function, the window's 'input' field will be updated with the latest input values.
/// This includes mouse movements, button presses, and keyboard events (feel free to read these input values in your application).
/// The window will also respond to system messages, such as resizing or closing.
/// This function should be called in the main loop of your application to keep the window responsive.
/// Once the window is closed, the 'closed_window' field in the input structure will be set to true.
/// @param w The window to update.
void update_window_input(window* w);

/// @brief resizes the window to the specified width and height.
/// The window mode can be set to WINDOW_MODE_WINDOWED, WINDOW_MODE_FULLSCREEN, or WINDOW_MODE_BORDERLESS_FULLSCREEN.
/// This function will adjust the window's style and position based on the mode.
/// @param w The window to resize.
/// @param width The new width of the window.
/// @param height The new height of the window.
/// @param mode The new window mode.
void resize_window(window* w, int width, int height, window_mode mode);

#ifdef VULKAN
VkSurfaceKHR create_vulkan_surface(window* w, VkInstance instance);
#endif
void destroy_window(window* w);

typedef struct {
    void* start;
    void* end;
    uintptr_t bump_pointer;
} arena_allocator;

result create_arena_allocator(arena_allocator* arena);
void arena_align(arena_allocator* arena, size_t alignment);
void* arena_allocate_unaligned(arena_allocator* arena, size_t size);
void* arena_allocate(arena_allocator* arena, size_t size, size_t alignment);
void arena_reset(arena_allocator* arena);
void destroy_arena_allocator(arena_allocator* arena);

#endif // PLATFORM_H