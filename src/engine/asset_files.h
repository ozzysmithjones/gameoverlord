#ifndef ASSET_FILES_H
#define ASSET_FILES_H

/*
This asset files module provides an interface for the game engine to read asset files.
This is made separate from the platform layer because in the future we might also want to do some asset processing during compile-time,
and so it makes sense to centralise the functions that might execute at runtime or compiletime for mental clarity.
*/

#include "platform_layer.h"

typedef struct {
    void* data;
    uint32_t channels;
    uint32_t height;
    uint32_t width;
} image;

// This exactly matches the memory layout of the "fmt " chunk in a WAV file.
typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} sound_format;

typedef struct {
    void* data;
    size_t data_size;
    sound_format format;
} sound;

DECLARE_CAPPED_ARRAY(sounds, sound, MAX_SOUNDS);

void load_sounds(memory_allocators* allocators, sounds* out_sounds);
result load_first_image(bump_allocator* allocator, image* out_image);
void unload_image(image* image);
#endif // ASSET_FILES_H