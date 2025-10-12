#ifndef PLATFORM_LAYER_H
#define PLATFORM_LAYER_H
#include <stdint.h>
#include <stdalign.h>
#include <stdbool.h>
#include "fundamental.h"
#include "geometry.h"


typedef struct input input;

// =============================================================
// Memory allocation
// =============================================================

typedef struct {
    void* base;
    size_t next_page_bytes;
    size_t used_bytes;
    size_t capacity;
} bump_allocator;

result create_bump_allocator(bump_allocator* allocator, size_t capacity);
void destroy_bump_allocator(bump_allocator* allocator);
void* bump_allocate(bump_allocator* allocator, size_t alignment, size_t bytes);

static inline void reset_bump_allocator(bump_allocator* allocator) {
    allocator->used_bytes = 0;
}

typedef struct {
    bump_allocator temp_allocator;
    bump_allocator permanent_allocator;
} memory_allocators;


// =============================================================
// Time Measurement
// =============================================================

typedef struct {
    float frequency;
    float time_since_previous_update;
    float creation_time;
    float time_since_creation;
} clock;

result create_clock(clock* clock);
void update_clock(clock* clock);

// =============================================================
// Graphics
// =============================================================

typedef struct {
    vector2 position;
    vector2 texcoord;
    vector2 scale;
    float rotation;
} sprite;

#ifndef MAX_SPRITES
#define MAX_SPRITES 32
#endif

typedef struct {
    sprite* elements;
    size_t count;
} sprites;

static inline result sprites_append(sprites* array, sprite element) {
    ASSERT(array != NULL, return RESULT_FAILURE, "Capped array sprites cannot be NULL");
    ASSERT(array->count < MAX_SPRITES, return RESULT_FAILURE, "Capped array sprites capacity exceeded: %u, cannot append element.", MAX_SPRITES);
    memcpy(&array->elements[array->count++], &element, sizeof(sprite));
    return RESULT_SUCCESS;
}

static inline result sprites_append_multiple(sprites* array, sprite* elements, uint32_t count) {
    ASSERT(array != NULL, return RESULT_FAILURE, "Capped array sprites cannot be NULL");
    ASSERT(array->count + count <= MAX_SPRITES, return RESULT_FAILURE, "Capped array sprites capacity exceeded: %u, cannot append elements.", MAX_SPRITES);
    memcpy(&array->elements[array->count], elements, count * sizeof(sprite));
    array->count += count;
    return RESULT_SUCCESS;
}

static inline result sprites_remove(sprites* array, uint32_t index) {
    ASSERT(array != NULL, return RESULT_FAILURE, "Capped array sprites cannot be NULL");
    ASSERT(index < array->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %zu", index, array->count);
    if (array->count > 0) {
        memmove(&array->elements[index], &array->elements[index + 1], (array->count - (index - 1)) * sizeof(sprite));
    }
    --array->count;
    return RESULT_SUCCESS;
}

static inline result sprites_remove_swap(sprites* array, uint32_t index) {
    ASSERT(array != NULL, return RESULT_FAILURE, "Capped array sprites cannot be NULL");
    ASSERT(index < array->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %zu", index, array->count);
    if (array->count > 0) {
        array->elements[index] = array->elements[array->count - 1];
    }
    --array->count;
    return RESULT_SUCCESS;
}

static inline sprite* sprites_bounds_checked_lookup(sprites* array, sprite* fallback, uint32_t index) {
    ASSERT(array != NULL, return fallback, "Capped array sprites cannot be NULL");
    ASSERT(index < array->count, return fallback, "Index out of bounds: %u. Count = %zu", index, array->count);
    return &array->elements[index];
}

static inline result sprites_bounds_checked_get(sprites* array, uint32_t index, sprite* out_element) {
    ASSERT(array != NULL, return RESULT_FAILURE, "Capped array sprites cannot be NULL");
    ASSERT(index < array->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %zu", index, array->count);
    memcpy(out_element, &array->elements[index], sizeof(sprite));
    return RESULT_SUCCESS;
}

static inline result sprites_bounds_checked_set(sprites* array, uint32_t index, sprite value) {
    ASSERT(array != NULL, return RESULT_FAILURE, "Capped array sprites cannot be NULL");
    ASSERT(index < array->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %zu", index, array->count);
    memcpy(&array->elements[index], &value, sizeof(sprite));
    return RESULT_SUCCESS;
}

static inline void sprites_clear(sprites* array) {
    ASSERT(array != NULL, return, "Capped array sprites cannot be NULL");
    array->count = 0;
}

// =============================================================
// User Input
// =============================================================

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

bool is_key_down(input* input_state, keyboard_key key);
bool is_key_held_down(input* input_state, keyboard_key key);
bool is_key_up(input* input_state, keyboard_key key);

// =============================================================
// File I/O
// =============================================================

string get_executable_directory(bump_allocator* allocator);
result find_first_file_with_extension(string directory, string extension, bump_allocator* allocator, string* out_full_path);
bool file_exists(string path);
result read_entire_file(string path, bump_allocator* allocator, string* out_file_contents);
result write_entire_file(string path, const void* data, size_t size);

// =============================================================
// Multithreading
// =============================================================

typedef union {
#ifdef _WIN32
    uint64_t alignment_dummy;
    uint8_t internals[40];
#else
#error Unsupported platform for mutex structure
#endif
} mutex;

typedef union {
#ifdef _WIN32
    uint64_t alignment_dummy;
    uint8_t internals[8];
#else
#error Unsupported platform for thread structure
#endif
} thread;

typedef union {
    uint64_t alignment_dummy;
    uint8_t internals[8];
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


// =============================================================
// APP Lifecycle
// =============================================================

typedef struct {
    void* app_state;
    sprites* sprites;
    memory_allocators* memory_allocators;
    input* input;
    clock clock;
} update_params;

typedef struct {
    void* app_state;
    memory_allocators* memory_allocators;
} shutdown_params;

typedef struct {
    memory_allocators* memory_allocators;
} init_in_params;

typedef struct {
    void* app_state;
} init_out_params;

#endif // PLATFORM_LAYER_H