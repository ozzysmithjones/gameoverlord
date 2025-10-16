# Gameoverlord

Simple 2D game engine written in C, with hot-reloading, memory allocators, graphics and user input. You can use this to make 2D games like super-mario or asteroids. 


> [!WARNING]
> NOTE! This project is in very early alpha development, so there might be frequent changes and bugs. 

## Project Structure

The project source code is devided into two halves. The "engine" folder contains all of the code necessary to run your game, including a platform layer, which wraps platform-specific code. Currently this game engine only supports Windows platform, but other platforms might be supported in the future. The "game" folder is where you can put your game code. The CMakeLists.txt at the root of the project is set up for hot-reloading. You can code your game while it is still running, and when you hit build in your editor (if hot reloading is enabled) you will see the program instantly refresh on screen with your changes (this hot-reloading feature can save you a bunch of time, as normally you would have to close and re-open the program to see the changes).

## Hot-Reloading

For hot-reloading to work, the game engine looks for three specific functions in your game code. You must provide an init() function, an update() function and a shutdown() function. The game engine will call your init() code when the game starts up, update() every frame, and shutdown() when your game shuts down. If the game hot-reloads, then it will keep using the same game state, but the functions will be refreshed, using the newer update() and shutdown() functions instead of the old ones. Keep in mind that any dynamically-allocated persistent state will still persist the same way after a hot-reload, so don't add or remove variables from any persistent state, the old memory layout will still be used). If you want to change memory layouts from game state, then re-building and re-starting the program is probably the best idea. Use hot-reload only for changing values, adding/removing variables on the stack/in static memory (these don't persist), or changing code. 

If hot-reload is enabled, your game is compiled as a dynamically linked library and linked with the engine at runtime (dynamically linked libraries can re-loaded). If hot-reload is disabled, instead your game and engine is compiled together (linked statically) to maximize performance. Having the engine folder and the game folder side by side in a single project lets you step through the game and the engine source code whenever you need to. 

## Example

Below, you can see an example of how you could implement the three functions for your game. Every function has some useful input parameters. There are no global variables in this game engine. This makes it possible to trace where values are used and passed around. 

``` C
#include <stdio.h>
#include "geometry.h"
#include "platform_layer.h"

typedef struct {
    vector2 position;
    int placeholder;
} game_state;

__declspec(dllexport) result init(init_in_params* in, init_out_params* out) {
    // the in parameter contains useful things that you might want during init.
    // Normally, here you would allocate the memory that you want to persist throughout the entire game (even across hot-reloads)
    // The engine provides two arena allocators (temp and permanent) that you can use. Explained more in a bit.
    // You must allocate your state dynamically with malloc() or use one of these allocators. This is because static values are reset during hot-reload. 
    game_state* state = (game_state*)bump_allocate(&in->memory_allocators->permanent_allocator, alignof(game_state), sizeof(game_state));
    if (state == NULL) {
        BUG("Failed to allocate memory for game state.");
        return RESULT_FAILURE;
    }
    
    // These output prameters must be filled in during init.
    // The state that you want to persist, and the virtual resolution of your screen that you want to target. This is the imaginary dimensions of your games display.
    // The game engine will manage scaling up to the actual screen resolution, keeping the aspect ratio the same and letterboxing as needed.
    out->user_state = state;
    out->virtual_resolution.x = 800;
    out->virtual_resolution.y = 600;

    return RESULT_SUCCESS;
}

__declspec(dllexport) result update(update_params* in) {
    
    // During update, you can get the user-data that you have allocated during init(). Keep in mind that this stateis intended to persist across hot-reloads.
    game_state* state = (game_state*)in->user_state;

    // Game logic would go here
    // ...

    // some useful functions:

    if (is_key_down(in->input, KEY_SPACE)) {
        printf("You pressed space!");
    }

    // Draws a 2D sprite to the screen, using some area in the sprite-sheet.
    draw_sprite(in->graphics, (vector2){0,0}, (vector2){0,0}, (vector2int){0,0}, (vector2int){0,0});

    return RESULT_SUCCESS;
}

__declspec(dllexport) void shutdown(shutdown_params* in) {

}
```

## Memory Management

You will note that there is no call to free() anywhere in this code, that is because the game engine already frees the arena-allocators at certain points. No need to call free()/reset_bump_allocator() yourself! Anything allocated with the permanent allocator is freed at the end of the program, and anything allocated with the temp allocator will be freed at the end of each frame. Internally, an arena is just a pointer pointing to a block of memory. When you call "bump_allocate" the pointer is bumped forward (commiting more memory from the operating system as needed) When you call reset_bump_allocator() the pointer is reset back to the beginning of the block. Each Arena is implemented in this program using one continuous block of reserved virtual memory (1 GB by default for the permanent allocator, and 64 MB for the temp).

## Asset Management

For now, the game engine just searches for the first .png file in the executable's directory, and uses that as a spritesheet to render your games sprites. 

## Error Handling

This game engine uses a unique approach to error handling where there is a different strategy depending on the build of your program. If you are building a debug/development build, any error results in an immediate breakpoint with a handy error message (failing fast): 

``` C
if (pointer == NULL) {
    BUG("uh oh") // will breakpoint and print an error message in development build.
}
ASSERT(pointer != NULL, return EXIT_FAILURE, "pointer is null") // ASSERT will also breakpoint and print an error in development
```

In release, the strategy changes, and instead of trying to fail-fast (to catch issues as soon as possible) the macros switch to logging errors and fault-tolerant error recoveries. You will notice that ASSERT() has three arguments. The ASSERTS are slightly different than traditional assert(). In release-mode, these ASSERTS do not disappear but instead perform the second argument when a failure occurs (normally early return with an exit code). This allows the program to recover even in the event of a bug (by checking the error code).

This strategy allows the code to be fault-tolerant in release-mode, but fail-fast in development mode so you can fix bugs during development. The philosphy here is that a release program should be bug free, and if it isn't we want to avoid bad-user experiences as much as possible, but still notify the developers somehow that something went wrong. Bad user-experience matters much more in release than it does in development. In development, we want the programmers to be alerted as soon as possible when something goes wrong. This is why we use different strategies for different builds. 

# Set Up

For now - Just clone the project and make the game in the src/game directory. In the future it might be possible to make a script/editor that generates projects, but for now just use the existing CMakeLists.txt at the root of the project. It should be good enough to gather all of the source files automatically and enable hot-reloading for you. 

The only dependencies that this project depends on is CMake and the Windows operating system (which includes the Windows API and DirectX11). 





