#include <print>
#include "test.hpp"
#include "platform.hpp"

static void game_loop(window& window) {
    while (true) {
        input& input = update_input(window);
        if (quit_is_requested(input)) {
            break;
        }

        if (key_is_pressed_this_frame(input, key::escape) && key_is_held_down(input, key::ctrl)) {
            break;
        }

        renderer& renderer = begin_draw_commands(window);

        submit_draw_commands(renderer);
    }
}

int main() {
#ifndef NDEBUG
    test::everything();
#endif
    create_window("Hello world", 400, 300, window_mode::windowed)
        .if_valid_do(game_loop);
    return 0;
}