#ifndef RENDERER_H
#define RENDERER_H
#include <stdint.h>
#include "platform_layer.h"

typedef struct renderer {
    alignas(8) uint8_t internals[168];
} renderer;

result create_renderer(window* window, renderer* out_renderer);
void destroy_renderer(renderer* renderer);

#endif