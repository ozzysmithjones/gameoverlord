#include <stdio.h>
#include "geometry.h"
#include "platform_layer.h"

typedef struct {
    vector2 position;
    int placeholder;
} game_state;

__declspec(dllexport) result init(init_in_params* in, init_out_params* out) {
    game_state* state = (game_state*)bump_allocate(&in->memory_allocators->permanent_allocator, alignof(game_state), sizeof(game_state));
    if (state == NULL) {
        BUG("Failed to allocate memory for game state.");
        return RESULT_FAILURE;
    }
    
    // These values must be filled in during init.
    // The state that you want to persist, and the virtual resolution of your screen that you want to target.
    // The game engine will manage scaling up to the actual screen resolution, keeping the aspect ratio the same and letterboxing as needed.
    out->user_state = state;
    out->virtual_resolution.x = 800;
    out->virtual_resolution.y = 600;

    return RESULT_SUCCESS;
}

__declspec(dllexport) result update(update_params* in) {
    game_state* state = (game_state*)in->user_state;

    // Game logic would go here

    return RESULT_SUCCESS;
}

__declspec(dllexport) void shutdown(shutdown_params* in) {

}
