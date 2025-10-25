#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "asset.h"

#ifdef EXECUTABLE_EMBEDDED_ASSETS
//#include "generated_sound_assets.h"
//#include "generated_image_assets.h"
#endif

IMPLEMENT_CAPPED_ARRAY(sounds, sound, MAX_SOUNDS)
IMPLEMENT_CAPPED_ARRAY(images, image, MAX_IMAGES)

typedef struct {
    union {
        uint32_t id;
        char id_chars[4];
    };
    uint32_t size;
} wav_file_chunk_header;

static result read_wav_file(string file_path, bump_allocator* allocator, sound* out_file_data) {
    ASSERT(file_path.length > 0, return RESULT_FAILURE, "File path cannot be empty");
    ASSERT(allocator != NULL, return RESULT_FAILURE, "Allocator cannot be NULL");
    ASSERT(out_file_data != NULL, return RESULT_FAILURE, "Output WAV file data cannot be NULL");

    memset(out_file_data, 0, sizeof(sound));

    string file_text;
    if (read_entire_file(file_path, allocator, &file_text) != RESULT_SUCCESS) {
        BUG("Failed to read WAV file: %.*s", file_path.length, file_path.text);
        return RESULT_FAILURE;
    }

    uint8_t* cursor = (uint8_t*)file_text.text;
    uint8_t* file_end = (uint8_t*)file_text.text + file_text.length;

    // Read RIFF chunk
    wav_file_chunk_header* riff_chunk = (wav_file_chunk_header*)cursor;
    if (memcmp(&riff_chunk->id_chars, "RIFF", 4) != 0) {
        BUG("Invalid WAV file: Missing RIFF chunk");
        return RESULT_FAILURE;
    }

    cursor += sizeof(wav_file_chunk_header);
    if (cursor + 4 > file_end || memcmp(cursor, "WAVE", 4) != 0) {
        BUG("Invalid WAV file: Missing WAVE format identifier");
        return RESULT_FAILURE;
    }

    cursor += 4; // Move past "WAVE" format identifier

    // Read chunks until we find "fmt " and "data"
    bool fmt_chunk_found = false;
    bool data_chunk_found = false;

    while (cursor + sizeof(wav_file_chunk_header) <= file_end) {
        wav_file_chunk_header* chunk_header = (wav_file_chunk_header*)cursor;
        cursor += sizeof(wav_file_chunk_header);

        if (memcmp(&chunk_header->id_chars, "fmt ", 4) == 0) {
            // Read format chunk
            if (chunk_header->size < 16 || cursor + chunk_header->size > file_end) {
                BUG("Invalid WAV file: Corrupted fmt chunk");
                return RESULT_FAILURE;
            }

            memcpy(&out_file_data->format, cursor, sizeof(sound_format));
            fmt_chunk_found = true;
            if (fmt_chunk_found && data_chunk_found) {
                return RESULT_SUCCESS;
            }
        }
        else if (memcmp(&chunk_header->id_chars, "data", 4) == 0) {
            // Read data chunk
            if (cursor + chunk_header->size > file_end) {
                BUG("Invalid WAV file: Corrupted data chunk");
                return RESULT_FAILURE;
            }

            out_file_data->data = cursor;
            out_file_data->data_size = chunk_header->size;

            data_chunk_found = true;
            if (fmt_chunk_found && data_chunk_found) {
                return RESULT_SUCCESS;
            }
        }

        cursor += chunk_header->size;
    }

    BUG("Invalid WAV file: Missing fmt or data chunk");
    return RESULT_FAILURE;
}

typedef struct {
    //Note that this is NOT the count for the number of elements, but the number of elements valid in the array (might not be contiguous)
    uint32_t num_valid;

    // This is a mapping from the file name to the index it should be stored at.
    // If the file cannot be mapped, it will have the value FILE_ORDERING_INVALID_INDEX.
    uint32_t index_by_file_name[MAX_FILE_NAMES];
} file_ordering;
STATIC_ASSERT(MAX_FILE_NAMES <= 64, max_file_names_should_be_bitset_compatible);

#define FILE_ORDERING_INVALID_INDEX UINT32_MAX

static void create_file_ordering(file_names* file_names, file_ordering* out_ordering) {
    ASSERT(file_names != NULL, return, "File names pointer cannot be NULL");
    ASSERT(out_ordering != NULL, return, "Output ordering pointer cannot be NULL");
#ifndef NDEBUG
    uint64_t bitset = 0;
#endif

    for (uint32_t i = 0; i < file_names->count; ++i) {
        out_ordering->index_by_file_name[i] = FILE_ORDERING_INVALID_INDEX;
        ASSERT(file_names->elements[i].length > 0, continue, "File name is empty at index %u", i);
        const char* cursor = file_names->elements[i].text + file_names->elements[i].length;
        while (cursor > file_names->elements[i].text && *(cursor - 1) != '/' && *(cursor - 1) != '\\') {
            --cursor;
        }

        ASSERT(cursor > file_names->elements[i].text, continue, "File name does not contain a valid name: %.*s", file_names->elements[i].length, file_names->elements[i].text);
        ASSERT(*cursor >= '0' && *cursor <= '9', continue, "File name does not start with a digit: %.*s", file_names->elements[i].length, file_names->elements[i].text);

        uint32_t file_index = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            file_index *= 10;
            file_index += (uint32_t)(*cursor - '0');
            ++cursor;
        }

#ifndef NDEBUG
        bitset |= ((uint64_t)1 << file_index);
#endif
        out_ordering->index_by_file_name[i] = file_index;
        ++out_ordering->num_valid;
    }

#ifndef NDEBUG
    if (bitset != (out_ordering->num_valid == 64 ? UINT64_MAX : ((uint64_t)1 << out_ordering->num_valid) - 1)) {
        BUG("File ordering contains duplicate or out-of-bounds indices. Ensure that it is a linear sequence starting from 0.");
    }
#endif
}

static result create_sounds_from_files(memory_allocators* allocators, string sound_directory, sounds* out_sounds) {
    ASSERT(allocators != NULL, return RESULT_FAILURE, "Memory allocators pointer cannot be NULL");
    ASSERT(out_sounds != NULL, return RESULT_FAILURE, "Output sounds pointer cannot be NULL");

    file_names sound_file_names = { 0 };
    if (find_files_with_extension(sound_directory, (string)CSTR(".wav"), &allocators->temp, &sound_file_names) != RESULT_SUCCESS) {
        BUG("No .wav sound files found in directory: %.*s", sound_directory.length, sound_directory.text);
        return RESULT_FAILURE;
    }

    file_ordering sound_ordering = { 0 };
    create_file_ordering(&sound_file_names, &sound_ordering);
    out_sounds->count = sound_ordering.num_valid;

    // create sounds at the specified indices
    for (uint32_t i = 0; i < sound_file_names.count; ++i) {
        uint32_t sound_index = sound_ordering.index_by_file_name[i];
        if (sound_index == FILE_ORDERING_INVALID_INDEX) {
            continue; // file name marked as invalid (not starting with a digit)
        }

        if (sound_index >= MAX_SOUNDS) {
            BUG("Sound index %u is out of bounds (max %u)", sound_index, MAX_SOUNDS);
            continue;
        }

        if (read_wav_file(sound_file_names.elements[i], &allocators->perm, &out_sounds->elements[sound_index]) != RESULT_SUCCESS) {
            BUG("Failed to read WAV file: %.*s", sound_file_names.elements[i].length, sound_file_names.elements[i].text);
            continue;
        }
    }

    return RESULT_SUCCESS;
}

static result create_images_from_files(memory_allocators* allocators, string image_directory, images* out_images) {
    ASSERT(allocators != NULL, return RESULT_FAILURE, "Memory allocators pointer cannot be NULL");
    ASSERT(out_images != NULL, return RESULT_FAILURE, "Output images pointer cannot be NULL");

    file_names image_file_names = { 0 };
    if (find_files_with_extension(image_directory, (string)CSTR(".png"), &allocators->temp, &image_file_names) != RESULT_SUCCESS) {
        BUG("No .png image files found in directory: %.*s", image_directory.length, image_directory.text);
        return RESULT_FAILURE;
    }

    file_ordering image_ordering = { 0 };
    create_file_ordering(&image_file_names, &image_ordering);
    out_images->count = image_ordering.num_valid;

    for (uint32_t i = 0; i < image_file_names.count; ++i) {
        uint32_t image_index = image_ordering.index_by_file_name[i];
        if (image_index == FILE_ORDERING_INVALID_INDEX) {
            continue; // file name marked as invalid (not starting with a digit)
        }

        if (image_index >= MAX_IMAGES) {
            BUG("Image index %u is out of bounds (max %u)", image_index, MAX_IMAGES);
            continue;
        }

        string image_file_content = { 0 };
        if (read_entire_file(image_file_names.elements[i], &allocators->perm, &image_file_content) != RESULT_SUCCESS) {
            BUG("Failed to read image file: %.*s", image_file_names.elements[i].length, image_file_names.elements[i].text);
            continue;
        }

        int32_t width = 0;
        int32_t height = 0;
        int32_t channels_in_file = 0;
        uint8_t* image_data = stbi_load_from_memory(image_file_content.text, image_file_content.length, &width, &height, &channels_in_file, STBI_rgb_alpha);
        if (!image_data) {
            BUG("Failed to load image file: %.*s", image_file_names.elements[i].length, image_file_names.elements[i].text);
            continue;
        }
        out_images->elements[image_index] = (image){
            .data = image_data,
            .width = (uint32_t)width,
            .height = (uint32_t)height,
            .channels = (uint32_t)channels_in_file,
        };
    }

    return RESULT_SUCCESS;
}

result load_sounds(memory_allocators* allocators, sounds* out_sounds) {
    ASSERT(allocators != NULL, return RESULT_FAILURE, "Memory allocators pointer cannot be NULL");
    ASSERT(out_sounds != NULL, return RESULT_FAILURE, "Output sounds pointer cannot be NULL");
    memset(out_sounds, 0, sizeof(sounds));

    string executable_directory = get_executable_directory(&allocators->temp);
    string sound_directory = concat(executable_directory, (string)CSTR("assets\\sounds\\"), &allocators->temp);
    return create_sounds_from_files(allocators, sound_directory, out_sounds);
}

result load_images(memory_allocators* allocators, images* out_images) {
    ASSERT(allocators != NULL, return RESULT_FAILURE, "Memory allocators pointer cannot be NULL");
    ASSERT(out_images != NULL, return RESULT_FAILURE, "Output images pointer cannot be NULL");
    memset(out_images, 0, sizeof(images));

    string executable_directory = get_executable_directory(&allocators->temp);
    string image_directory = concat(executable_directory, (string)CSTR("assets\\images\\"), &allocators->temp);
    return create_images_from_files(allocators, image_directory, out_images);
}

void unload_images(images* images) {
    ASSERT(images != NULL, return, "Images pointer cannot be NULL");

    for (uint32_t i = 0; i < images->count; ++i) {
        if (images->elements[i].data != NULL) {
            stbi_image_free(images->elements[i].data);
        }
    }

    memset(images, 0, sizeof(images));
}
