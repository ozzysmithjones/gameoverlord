#ifndef PLATFORM_LAYER_H
#define PLATFORM_LAYER_H
#include <stdint.h>
#include <stdalign.h>
#include <stdbool.h>
#include "fundamental.h"
#include "geometry.h"

/*

Platform layer initialization and shutdown functions.
Note that some platforms might require an initialization step before using any other platform functions.
For example on windows, the native page size is queried during initialization and stored for later use in memory allocation functions.

*/

typedef struct {
    size_t min_temp_allocator_size;
    size_t recommended_temp_allocator_size;
    size_t min_permanent_allocator_size;
    size_t recommended_permanent_allocator_size;
} memory_requirements;


// Create the platform layer, including global allocators. Global allocators will reserve at least the minimum sizes specified in the memory requirements struct. 
// If you pass NULL, the default values will be used. By default, the temporary allocator will reserve 16 MB minimum and 64 MB recommended, and the permanent allocator will reserve 16 MB minimum and as much as possible recommended.
// You can set the recommended sizes to SIZE_MAX in the memory requirements to reserve as much memory as possible (up to the limits of the OS).
result create_platform_layer(memory_requirements* requirements);
void destroy_platform_layer(void);

/*

Memory allocation stuff.

*/

typedef struct {
    void* base;
    size_t next_page_bytes;
    size_t used_bytes;
    size_t capacity;
} bump_allocator;

extern bump_allocator permanent_allocator; // A global permanent allocator for any allocation that should live for the entire program lifetime.
extern bump_allocator temp_allocator; // A global temporary allocator for short lived allocations. You should reset this allocator regularly to avoid running out of memory (like the end of each frame).

/// @brief Create a bump allocator with at least min_capacity bytes and at most max_capacity bytes. It will prioritize reserving as close to max_capacity as possible.
/// Anything below min_capacity will result in failure.
/// @param allocator Pointer to the bump allocator to create.
/// @param min_capacity Minimum capacity of the allocator.
/// @param max_capacity Maximum capacity of the allocator.
/// @return RESULT_SUCCESS on success, RESULT_FAILURE on failure.
result create_bump_allocator(bump_allocator* allocator, size_t min_capacity, size_t max_capacity);
void destroy_bump_allocator(bump_allocator* allocator);
void* bump_allocate(bump_allocator* allocator, size_t alignment, size_t bytes);

static inline void reset_bump_allocator(bump_allocator* allocator) {
    allocator->used_bytes = 0;
}

/*

Multithreading stuff.

*/

typedef struct {
#ifdef _WIN32
    alignas(8) uint8_t internals[40];
#else
#error Unsupported platform for mutex structure
#endif
} mutex;

typedef struct {
#ifdef _WIN32
    alignas(8) uint8_t internals[8];
#else
#error Unsupported platform for thread structure
#endif
} thread;

typedef struct {
    alignas(8) uint8_t internals[8];
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

/*

Window stuff.

*/

typedef struct {
#ifdef _WIN32
    alignas(8) uint8_t internals[200];
#endif
} window;

typedef enum {
    WINDOW_MODE_WINDOWED,
    WINDOW_MODE_FULLSCREEN,
    WINDOW_MODE_BORDERLESS_FULLSCREEN
} window_mode;

result create_window(window* w, const char* title, uint32_t width, uint32_t height, window_mode mode);
void destroy_window(window* w);

/*

User input stuff.

*/
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

typedef struct input input;

input* update_window_input(window* w);
bool is_window_closed(input* input_state);
bool is_key_down(input* input_state, keyboard_key key);
bool is_key_held_down(input* input_state, keyboard_key key);
bool is_key_up(input* input_state, keyboard_key key);

/*

Graphics stuff.

*/

typedef struct renderer renderer;

renderer* begin_draw_commands(window* w);
void present_draw_commands(renderer* r);

#endif // PLATFORM_LAYER_H