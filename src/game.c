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
    memset(state, 0, sizeof(game_state));
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

    vector2 scale = { 400, 400 };   // Large size
    vector2int texcoord = { 0, 0 };    // Top-left of the sprite sheet
    vector2int texscale = { 64, 64 };  // Assuming the sprite is 64x64 pixels in size
    float rotation = 0.0f;             // No rotation

    state->position.x += in->clock.time_since_previous_update * 1000.0f;

    vector2int display_size = get_display_size(in->graphics);
    if (state->position.x > (float)display_size.x) {
        state->position.x = 0.0f;
    }

    draw_sprite(in->graphics, state->position, scale, texcoord, texscale, rotation);

    // Debug message
    printf("Game update: Drew sprite at position: (%f, %f) with scale: (%f, %f)\n",
        state->position.x, state->position.y, scale.x, scale.y);

    puts("Hello from game update!");
    return RESULT_SUCCESS;
}

__declspec(dllexport) void shutdown(shutdown_params* in) {
    puts("Hello from game shutdown!");
}
