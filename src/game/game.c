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
    out->virtual_resolution = (vector2int){ 256, 256 };
    return RESULT_SUCCESS;
}

DLL_EXPORT result start(start_params* in) {
    ASSERT(in->game_state, return RESULT_FAILURE, "Game state is NULL in start.");
    game_state* state = (game_state*)in->game_state;
    return RESULT_SUCCESS;
}

DLL_EXPORT result update(update_params* in) {
    ASSERT(in->game_state, return RESULT_FAILURE, "Game state is NULL in update.");
    game_state* state = (game_state*)in->game_state;
    state->position.x += 50.0f * in->clock.time_since_previous_update;
    if (is_key_down(in->input, KEY_SPACE)) {
        play_sound(in->audio, 1, 0, 0.0f);
    }

    color background_color = color_from_uint32(0x222323);
    draw_background_color(in->graphics, background_color.r, background_color.g, background_color.b, background_color.a);
    draw_sprite(in->graphics, (vector2) {
        128, 128
    }, (vector2) {
            128.0f, 128.0f
        }, (vector2int) {
                0, 0
            }, (vector2int) {
                    256, 256
                }, 0.0f);
                return RESULT_SUCCESS;
}

DLL_EXPORT void shutdown(shutdown_params* in) {
    ASSERT(in->game_state, return, "Game state is NULL in shutdown.");
}
