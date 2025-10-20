#ifndef PLATFORM_LAYER_H
#define PLATFORM_LAYER_H
#include <stdint.h>
#include <stdalign.h>
#include <stdbool.h>
#include "fundamental.h"
#include "geometry.h"


typedef struct input input;
typedef struct graphics graphics;
typedef struct audio audio;

/*
=============================================================================================================================
    Memory Allocation
=============================================================================================================================
*/

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
    /* Temporary memory allocator, used for allocations that last for one frame. */
    bump_allocator temp;

    /* Permanent memory allocator, used for allocations that last for the entire program lifetime. */
    bump_allocator perm;
} memory_allocators;


/*
=============================================================================================================================
    Time
=============================================================================================================================
*/


typedef struct {
    float frequency;
    float previous_update_time;
    float time_since_previous_update;
    float creation_time;
    float time_since_creation;
} clock;

result create_clock(clock* clock);
void update_clock(clock* clock);


/*
=============================================================================================================================
    Graphics
=============================================================================================================================
*/

typedef struct {
    int32_t x;
    int32_t y;
} vector2int;

#ifndef MAX_SPRITES
#define MAX_SPRITES 32
#endif

void draw_sprite(graphics* graphics, vector2 position, vector2 scale, vector2int texcoord, vector2int texscale, float rotation);
vector2int get_actual_resolution(graphics* graphics);
vector2int get_virtual_resolution(graphics* graphics);

/*
=============================================================================================================================
    Audio
=============================================================================================================================
*/

#ifndef MAX_CONCURRENT_SOUNDS
#define MAX_CONCURRENT_SOUNDS 8
#endif

#ifndef MAX_SOUNDS
#define MAX_SOUNDS 8
#endif

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 44100
#endif

#ifndef AUDIO_CHANNELS
#define AUDIO_CHANNELS 2
#endif

#ifndef AUDIO_BITS_PER_SAMPLE
#define AUDIO_BITS_PER_SAMPLE 16
#endif

#ifndef AUDIO_DEFAULT_VOLUME
#define AUDIO_DEFAULT_VOLUME 1.0f
#endif

DECLARE_CAPPED_ARRAY(sound_files, string, MAX_SOUNDS);

typedef enum {
    PLAYING_SOUND_NONE = 0,
    PLAYING_SOUND_LOOPING = 1 << 0,
    PLAYING_SOUND_EVEN_IF_ALREADY_PLAYING = 1 << 1,
} playing_sound_flags;

result play_sound(audio* audio, uint32_t sound_index, playing_sound_flags flags, float fade_in_duration);

typedef enum {
    STOPPING_ALL_INSTANCES,
    STOPPING_FIRST_FOUND,
} stopping_mode;

void stop_sound(audio* audio, uint32_t sound_index, stopping_mode mode, float fade_out_duration);

/*
=============================================================================================================================
    User Input
=============================================================================================================================
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

bool is_key_down(input* input_state, keyboard_key key);
bool is_key_held_down(input* input_state, keyboard_key key);
bool is_key_up(input* input_state, keyboard_key key);


/*
=============================================================================================================================
    File I/O
=============================================================================================================================
*/

string get_executable_directory(bump_allocator* allocator);
result find_first_file_with_extension(string directory, string extension, bump_allocator* allocator, string* out_full_path);
bool file_exists(string path);
result read_entire_file(string path, bump_allocator* allocator, string* out_file_contents);
result write_entire_file(string path, const void* data, size_t size);


/*
=============================================================================================================================
    Multi-threading
=============================================================================================================================
*/

typedef union {
#ifdef _WIN32
    uint64_t alignment_dummy;
    uint8_t internals[8];
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

result create_mutex(mutex* m);
result lock_mutex(mutex* m);
result unlock_mutex(mutex* m);
void destroy_mutex(mutex* m);

result create_thread(thread* t, unsigned long (*start_routine)(void*), void* arg);
result join_thread(thread* t);
void destroy_thread(thread* t);

result init_condition_variable(condition_variable* cv);
result signal_condition_variable(condition_variable* cv);
result wait_condition_variable(condition_variable* cv, mutex* m);

/*
=============================================================================================================================
    Game Loop Parameters
=============================================================================================================================
*/

typedef struct {
    void* game_state;
    graphics* graphics;
    audio* audio;
    memory_allocators* memory_allocators;
    input* input;
    clock clock;
} update_params;

typedef struct {
    void* game_state;
    memory_allocators* memory_allocators;
} shutdown_params;

typedef struct {
    void* game_state;
    memory_allocators* memory_allocators;
    audio* audio;
} start_params;

typedef struct {
    memory_allocators* memory_allocators;
} init_in_params;

typedef struct {
    void* game_state;

    /*
    Modern computers are much higher resolution than older games were designed for.
    If you want to make a pixel-art game, drawing the pixel art at the actual resolution would make them look tiny.
    To solve this, we introduce the concept of a "virtual resolution", which is the resolution that the game thinks it's drawing to, but it's actually scaled up to the real size of your screen.
    You need to provide your desired virtual resolution here and the platform layer will handle the rest (scaling up while keeping the aspect ratio the same, letterboxing as needed and so on).
    */
    vector2int virtual_resolution;

    // The sound files to search for (array of file names). Note that these sound files will be searched in the same directory as the executable.
    sound_files sound_files;

    // file_names sprite_sheet_files; TODO: support multiple sprite sheets 
} init_out_params;

#endif // PLATFORM_LAYER_H