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

    // Draw the sprite at center of screen with a large size to make it obvious
    vector2int position = { 640, 360 }; // Center of a 1280x720 window
    vector2int scale = { 400, 400 };   // Large size
    vector2int texcoord = { 0, 0 };    // Top-left of the sprite sheet
    float rotation = 0.0f;             // No rotation

    draw_sprite(in->graphics, position, scale, texcoord, rotation);

    // Debug message
    printf("Game update: Drew sprite at position: (%d, %d) with scale: (%d, %d)\n",
        position.x, position.y, scale.x, scale.y);

    puts("Hello from game update!");
    return RESULT_SUCCESS;
}

__declspec(dllexport) void shutdown(shutdown_params* in) {
    puts("Hello from game shutdown!");
}
