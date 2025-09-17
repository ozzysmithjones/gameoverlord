#include <stdio.h>
#include "geometry.h"
#include "platform_layer.h"

int main(void) {
    create_platform_layer();

    window w;
    if (create_window(&w, "Game Overlord", 800, 600, WINDOW_MODE_WINDOWED) != RESULT_SUCCESS) {
        fprintf(stderr, "Failed to create window.\n");
        destroy_platform_layer();
        return -1;
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


        
    }

    destroy_window(&w);
    destroy_platform_layer();
    return 0;
}