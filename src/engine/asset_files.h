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

/*
Some notes on the memory allocation strategy used here:
- For images, we use stb_image to load the image data directly into heap memory managed by stb_image.
  The image struct holds a pointer to this data, and we provide a function to free it when done.
- For sounds, we read the entire WAV file into a buffer allocated from the provided bump allocator.
  The sound struct holds a pointer to this buffer along with its size and format information.

So you only need to destroy the image data using destroy_image when done with an image.
The sound data will be automatically freed when the bump allocator is reset or destroyed.
In the future it might be preferable to make a png reader (instead of stb_image) so we can put all image and sound data in bump allocators for consistency.
*/

void create_sounds_from_files(memory_allocators* allocators, sounds* out_sounds);
result create_image_from_first_file(bump_allocator* allocator, image* out_image);
void destroy_image(image* image);
#endif // ASSET_FILES_H