#ifndef FILE_FORMATS_H
#define FILE_FORMATS_H
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
DECLARE_CAPPED_ARRAY(images, image, MAX_IMAGES);

result load_sounds(memory_allocators* allocators, sounds* out_sounds);
result load_images(memory_allocators* allocators, images* out_images);
void unload_images(images* images);
#endif // FILE_FORMATS_H