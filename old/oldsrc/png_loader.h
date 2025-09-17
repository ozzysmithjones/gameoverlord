#include <stdint.h>
#include <stdbool.h>

typedef enum {
    RESULT_FAILURE,
    RESULT_SUCCESS,
} result;

typedef struct {
    char* characters;
    size_t length;
} string;

typedef struct {
    int32_t width;
    int32_t height;
    int32_t channels;
    uint8_t* data;
} image;

result load_png(string file_content, image* image);