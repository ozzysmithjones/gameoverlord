#include <stdio.h>
#include "geometry_math.h"
#include "platform.h"
#include "fundamental.h"

int main() {
  printf("Hello, Game Overlord!\n");
  window w = { 0 };
  if (create_window(&w, "Test Window", 800, 600, WINDOW_MODE_WINDOWED) != RESULT_SUCCESS) {
    BUG("Failed to create window\n");
    return 1;
  }

  while (!w.input.closed_window) {
    update_window_input(&w);
  }

  destroy_window(&w);
  return 0;
}
