#include <stdio.h>
#include "geometry.h"
#include "platform_layer.h"

int main(void) {
    if (create_platform_layer(NULL) != RESULT_SUCCESS) {
        fprintf(stderr, "Failed to create platform layer.\n");
        return -1;
    }

    window w = { 0 };
    sprite_renderer r = { 0 };
    if (create_window(&w, "Game Overlord", 800, 600, WINDOW_MODE_WINDOWED) != RESULT_SUCCESS) {
        fprintf(stderr, "Failed to create window.\n");
        goto cleanup;
    }

    texture text = { 0 };
    if (create_sprite_renderer(&w, &r, text) != RESULT_SUCCESS) {
        fprintf(stderr, "Failed to create renderer.\n");
        goto cleanup;
    }

    while (1) {
        input* input_state = update_window_input(&w);
        if (is_window_closed(input_state) || is_key_down(input_state, KEY_ESCAPE)) {
            break;
        }

        // Game update and rendering logic would go here.

        // For demonstration, just print if the space key is pressed.
        if (is_key_down(input_state, KEY_SPACE)) {
            printf("Space key pressed!\n");
        }

        reset_bump_allocator(&temp_allocator);
    }

cleanup:
    destroy_sprite_renderer(&r);
    destroy_window(&w);
    destroy_platform_layer();
    return 0;
}