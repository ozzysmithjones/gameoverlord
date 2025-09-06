#pragma once
#include "fundamental.hpp"

enum class key;

#if defined(_WIN32)
// Windows virtual key codes
enum class key {
    none = 0,
    backspace = 8,
    tab = 9,
    enter = 13,
    shift = 16,
    ctrl = 17,
    alt = 18,
    pause = 19,
    caps_lock = 20,
    escape = 27,
    space = 32,
    page_up = 33,
    page_down = 34,
    end = 35,
    home = 36,
    left = 37,
    up = 38,
    right = 39,
    down = 40,
    select = 41,
    print = 42,
    execute = 43,
    print_screen = 44,
    insert = 45,
    delete_key = 46,
    help = 47,
    num_0 = 48,
    num_1 = 49,
    num_2 = 50,
    num_3 = 51,
    num_4 = 52,
    num_5 = 53,
    num_6 = 54,
    num_7 = 55,
    num_8 = 56,
    num_9 = 57,
    a = 65,
    b = 66,
    c = 67,
    d = 68,
    e = 69,
    f = 70,
    g = 71,
    h = 72,
    i = 73,
    j = 74,
    k = 75,
    l = 76,
    m = 77,
    n = 78,
    o = 79,
    p = 80,
    q = 81,
    r = 82,
    s = 83,
    t = 84,
    u = 85,
    v = 86,
    w = 87,
    x = 88,
    y = 89,
    z = 90,
    left_meta = 91,
    right_meta = 92,
    apps = 93,
    sleep = 95,
    numpad_0 = 96,
    numpad_1 = 97,
    numpad_2 = 98,
    numpad_3 = 99,
    numpad_4 = 100,
    numpad_5 = 101,
    numpad_6 = 102,
    numpad_7 = 103,
    numpad_8 = 104,
    numpad_9 = 105,
    multiply = 106,
    add = 107,
    separator = 108,
    subtract = 109,
    decimal = 110,
    divide = 111,
    f1 = 112,
    f2 = 113,
    f3 = 114,
    f4 = 115,
    f5 = 116,
    f6 = 117,
    f7 = 118,
    f8 = 119,
    f9 = 120,
    f10 = 121,
    f11 = 122,
    f12 = 123,
    num_lock = 144,
    scroll_lock = 145,
    left_shift = 160,
    right_shift = 161,
    left_ctrl = 162,
    right_ctrl = 163,
    left_alt = 164,
    right_alt = 165,

    left_mouse_button = 1,
    right_mouse_button = 2,
    middle_mouse_button = 4,
    count = 256,
};
#endif

enum class program_expectation {
    should_terminate,
    should_continue,
};

namespace platform {
    struct window;
    struct renderer;
    struct input;

    [[nodiscard]] window& init();
    void cleanup(window& window);
    [[nodiscard]] input& update_input(window& window);

    // if the key is currently pressed, can be pressed for multiple frames
    bool is_key_pressed(const input& input, key k);

    // if the key is currently released, can be released for multiple frames
    bool is_key_released(const input& input, key k);

    // If the key was just pressed this frame (not held)
    bool is_key_just_pressed(const input& input, key k);

    // If the key was just released this frame (not held)
    bool is_key_just_released(const input& input, key k);

    bool is_quit_requested(const input& input);

    [[nodiscard]] renderer& begin_drawing(window& window);
    void end_drawing(renderer& renderer);
}
