#include <stdarg.h>
#include "fundamental.h"

result directory_of(string file_path, string* out_directory) {
    ASSERT(file_path.length > 0, return RESULT_FAILURE, "File path cannot be empty");
    ASSERT(out_directory != NULL, return RESULT_FAILURE, "Output directory pointer cannot be NULL");

    for (int32_t i = (int32_t)file_path.length - 1; i >= 0; --i) {
        if (file_path.text[i] == '/' || file_path.text[i] == '\\') {
            out_directory->text = file_path.text;
            out_directory->length = (uint32_t)i + 1; // Include the slash
            return RESULT_SUCCESS;
        }
    }

    out_directory->text = file_path.text;
    out_directory->length = file_path.length;
    return RESULT_SUCCESS;
}

result string_format(string_format_buffer* out_format_string, string format, ...) {
    ASSERT(out_format_string != NULL, return RESULT_FAILURE, "Output format string cannot be NULL");
    ASSERT(format.length < FORMAT_STRING_MAX, return RESULT_FAILURE, "Format string is too long");

    out_format_string->length = 0;
    va_list args;
    va_start(args, format);
    int written = vsnprintf(out_format_string->text, sizeof(out_format_string->text), format.text, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sizeof(out_format_string->text)) {
        BUG("String formatting error or output truncated.");
        return RESULT_FAILURE;
    }
    out_format_string->length = (uint32_t)written;
    return RESULT_SUCCESS;
}

