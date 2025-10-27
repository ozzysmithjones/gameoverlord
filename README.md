# Gameoverlord

2D game engine in C, with hot-reloading, memory allocators, audio, graphics and user input. Uses XAudio2 and DirectX11 as audio/graphics API. This game engine is designed to render a high number of 2D sprites to the screen with excellent performance (just a single draw call). 

> [!WARNING]
> NOTE! This project is in very early alpha development, so there might be frequent changes and bugs. 

## Features

Hot Reloading - You can build your game while it is still running and the changes will update live. (No need to close and re-open the game to see your changes). This hot-reloading feature needs to be explicitly enabled in the CMakeLists.txt at the root of the project (just change HOT_RELOAD to ON).
Audio - `play_sound` function for playing sounds.
Graphics - `draw_sprite` function for drawing sprites.
User input - Simple functions for checking user input like `is_key_down`, `is_key_up- and `is_key_held_down`.

## Project Structure

The project is devided into two folders within src. The "engine" subfolder contains code relevant for the game engine itself, this includes the platform layer which wraps platform specific code. This game engine currently only works on Windows but other platforms might be supported in the future. The platform layer also wraps the main event-loop, which could be different from platform to platform (look in windows_platform_layer.c to find the main entry-point).

The "game" subfolder is where you would put your game specific code. Having the engine source code and the game source code side-by-side allows you to easily inspect both in the debugger whenever you need to. The CMakeLists.txt at the root of the project controls the building of the game and the engine. When hot-reloading is turned on, the game is built as a dynamically-linked library which can be reloaded at runtime. When hot-reloading is off, instead the game and the engine is built together as a single executable.

## Hot Reloading

Hot-Reloading allows you to to build the game while it is still running and see the changes update live. The way that this works is that the game is compiled as a DLL (Dynamically linked library) that is linked with the engine at runtime. When your DLL is built, the engine detects the updated DLL file and unloads the current DLL (if one exists) and then re-directs function pointers to point to the functions in the new DLL. It's worth pointing out here, that since the DLL is reloaded, any static values in your game will not persist across hot-reloads (static variables will reset to zero). You can get around this by allocating your values on the heap instead using the provided memory allocators - these memory allocations will stay around even when the game is hot-reloaded. Just keep in mind that since the memory allocations stay around, changes to the memory layouts of these heap-allocated resources (like adding more elements to a struct) will likely not work because it will still use the heap-allocated resources of the previous DLL (with the previous memory layouts). Hot-reloading is useful for making quick edits of code and stack variables and seeing the changes instantly refresh - but for anything persistent recommend closing and re-starting the application instead.

To enable hot-reloading, set HOT_RELOADING to ON in the CMakeLists.txt at the root of the project. Building your game however you want should automatically make the program hot-reload and refresh instantly. You can build your game even while it is still running. The engine looks for these four functions in your game code that are exported from the DLL file(init, start, update, and shutdown) (example in game.c):

``` C
#include <stdio.h>
#include "geometry.h"
#include "platform_layer.h"

typedef struct {
    vector2 position;
    int placeholder;
} game_state;

DLL_EXPORT result init(init_in_params* in, init_out_params* out) {
    game_state* state = (game_state*)bump_allocate(&in->memory_allocators->perm, alignof(game_state), sizeof(game_state));
    if (state == NULL) {
        BUG("Failed to allocate memory for game state.");
        return RESULT_FAILURE;
    }

    out->game_state = (void*)state;
    out->virtual_resolution = (vector2int){ 600, 400 };
    return RESULT_SUCCESS;
}

DLL_EXPORT result start(start_params* in) {
    ASSERT(in->game_state, return RESULT_FAILURE, "Game state is NULL in start.");
    game_state* state = (game_state*)in->game_state;

    play_sound(in->audio, 0, PLAYING_SOUND_LOOPING, 0.0f);
    return RESULT_SUCCESS;
}

DLL_EXPORT result update(update_params* in) {
    ASSERT(in->game_state, return RESULT_FAILURE, "Game state is NULL in update.");
    game_state* state = (game_state*)in->game_state;
    state->position.x += 50.0f * in->clock.time_since_previous_update;
    if (is_key_down(in->input, KEY_SPACE)) {
        play_sound(in->audio, 1, 0, 0.0f);
    }

    color background_color = color_from_uint32(0xFF1A1AFF);
    draw_background_color(in->graphics, background_color.r, background_color.g, background_color.b, background_color.a);
    draw_sprite(in->graphics, state->position,(vector2) { 128.0f, 128.0f}, (vector2int) {0, 0},(vector2int) {64, 64}, 0.0f);

    return RESULT_SUCCESS;
}

DLL_EXPORT void shutdown(shutdown_params* in) {

}
```
## Memory Management

The game engine provides what is known as "arena allocators" or "bump allocators" for memory management. A bump allocator is a reserved block of memory with a pointer pointing to the beginning of the block. Whenever you need more memory, the pointer is simply bumped forward, committing more pages of memory from the operating system as needed. This is a very simple approach to memory management - you can bump the pointer forward whenever you need more memory and you can reset it back to the start whenever you wish to "free" everything. The advantage of this is that you do not need to concern yourself with memory management at all - you simply free everything all at once whenever there is a good time. The main downside is that you cannot recycle/free memory with the same level of fine granularity as the heap. The game engine provides two bump allocators - the permanent allocator is for any allocations that you want to persist throughout the whole game, and the temp allocator is reset every frame automatically. Use the temp allocator for any temporary allocations, like temporary string manipulation, which would normally be a pain in C.

## Error Handling

This game engine uses a different strategy with errors depending on the build type of your program. If you are compiling a development/debug build - all errors result in an immediate breakpoint and a handy error message - allowing the programmer to quickly diagnose and fix issues as soon as they happen. This is a "fail fast" approach to handling errors. In a release build, the strategy switches, and instead of trapping the program the errors switch to reporting an error message and performing graceful error-recoveries. 

The assert macro looks like this:

```C
#ifndef BUG
#include <stdio.h>
#ifndef NDEBUG
#define BUG(...) printf(__FILE__":" TOSTRING(__LINE__) " Bug: " __VA_ARGS__); fflush(stdout); BREAKPOINT()
#else
#include <stdio.h>
#define BUG(...) printf(__FILE__":" TOSTRING(__LINE__) " Bug: " __VA_ARGS__);
#endif
#endif

#define ASSERT(condition, fallback, ...) \
    if (!(condition)) { \
        BUG(__VA_ARGS__); \
        fallback; \
    }
```

The assert macro has a condition (like normal) but it also has a fallback recovery behaviour to perform in a release build. In release, asserts do not dissapear but instead perform the fallback behaviour in the event of an error. This means that your program can "Fail Fast" in development builds but be "Fault Tolerant" in release builds, configuring behaviour to match the need of the target audience. These macros can of course be adjusted to suit your application.