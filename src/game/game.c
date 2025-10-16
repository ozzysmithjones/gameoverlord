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
    out->user_state = state;
    out->virtual_resolution = (vector2int){ 600, 400 };

    puts("Hello from game init!");
    return RESULT_SUCCESS;
}

__declspec(dllexport) result update(update_params* in) {
    game_state* state = (game_state*)in->user_state;
    if (state == NULL) {
        BUG("User state is NULL in update.");
        return RESULT_FAILURE;
    }

    vector2 scale = { 160, 160 };
    vector2int texcoord = { 0, 0 };
    vector2int texscale = { 32, 32 };
    float rotation = 0.0f;

    vector2int display_size = get_virtual_resolution(in->graphics);
    if (state->position.x > (float)display_size.x) {
        state->position.x = 0.0f;
    }

    draw_sprite(in->graphics, state->position, scale, texcoord, texscale, rotation);

    return RESULT_SUCCESS;
}

__declspec(dllexport) void shutdown(shutdown_params* in) {
    puts("Hello from game shutdown!");
}
