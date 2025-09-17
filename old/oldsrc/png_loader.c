#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "png_loader.h"

#define PNG_SIGNATURE_SIZE 8

result load_png(string file_content, image* image) {
    if (file_content.length < PNG_SIGNATURE_SIZE || file_content.characters == NULL) {
        return RESULT_FAILURE;
    }

    char* ptr = file_content.characters;
    if (memcmp(ptr, "\x89PNG\r\n\x1A\n", PNG_SIGNATURE_SIZE) != 0) {
        return RESULT_FAILURE; // Not a valid PNG file
    }
    ptr += PNG_SIGNATURE_SIZE;


    return RESULT_SUCCESS;
}
