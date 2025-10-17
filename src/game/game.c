#include <stdio.h>
#include "geometry.h"
#include "platform_layer.h"

typedef struct {
    vector2 position;
    int placeholder;
} game_state;

__declspec(dllexport) result init(init_in_params* in, init_out_params* out) {
    game_state* state = (game_state*)bump_allocate(&in->memory_allocators->perm, alignof(game_state), sizeof(game_state));
    if (state == NULL) {
        BUG("Failed to allocate memory for game state.");
        return RESULT_FAILURE;
    }

    out->game_state = (void*)state;
    out->virtual_resolution = (vector2int){ 600, 400 };
    return RESULT_SUCCESS;
}

__declspec(dllexport) result update(update_params* in) {
    game_state* state = (game_state*)in->game_state;
    if (state == NULL) {
        BUG("Game state is NULL in update.");
        return RESULT_FAILURE;
    }

    state->position.x += 50.0f * in->clock.time_since_previous_update;

    draw_sprite(
        in->graphics,
        state->position,
        (vector2) {
        128.0f, 128.0f
    },
        (vector2int) {
        0, 0
    },
        (vector2int) {
        64, 64
    },
        0.0f
    );

    return RESULT_SUCCESS;
}

__declspec(dllexport) void shutdown(shutdown_params* in) {

}
