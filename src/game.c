#include <stdio.h>
#include "geometry.h"
#include "platform_layer.h"

typedef struct {
    int placeholder;
} game_state;

__declspec(dllexport) result init(init_in_params* in, init_out_params* out) {
    game_state* state = (game_state*)bump_allocate(&in->memory_allocators->permanent_allocator, alignof(game_state), sizeof(game_state));
    if (state == NULL) {
        BUG("Failed to allocate memory for game state.");
        return RESULT_FAILURE;
    }
    out->app_state = state;

    puts("Hello from game init!");
    return RESULT_SUCCESS;
}

__declspec(dllexport) result update(update_params* in) {
    game_state* state = (game_state*)in->app_state;
    if (state == NULL) {
        BUG("App state is NULL in update.");
        return RESULT_FAILURE;
    }

    printf("placeholder = %d\n", state->placeholder);
    state->placeholder += 1;

    puts("Hello from game update!");
    return RESULT_SUCCESS;
}

__declspec(dllexport) void shutdown(shutdown_params* in) {
    puts("Hello from game shutdown!");
}
