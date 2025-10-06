#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_c_file>\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    const char* output_c_file = argv[2];

    FILE* in = fopen(input_file, "rb");
    if (!in) {
        fprintf(stderr, "Error: Could not open input file %s\n", input_file);
        return 1;
    }

    fseek(in, 0, SEEK_END);
    long file_size = ftell(in);
    fseek(in, 0, SEEK_SET);

    unsigned char* buffer = (unsigned char*)malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Error: Could not allocate memory for file buffer\n");
        fclose(in);
        return 1;
    }

    fread(buffer, 1, file_size, in);
    fclose(in);

    FILE* out = fopen(output_c_file, "w");
    if (!out) {
        fprintf(stderr, "Error: Could not open output file %s\n", output_c_file);
        free(buffer);
        return 1;
    }

    fprintf(out, "#include <stdint.h>\n\n");
    fprintf(out, "const uint8_t file_data[%ld] = {", file_size);
    for (long i = 0; i < file_size; i++) {
        if (i % 12 == 0) {
            fprintf(out, "\n    ");
        }
        fprintf(out, "0x%02X", buffer[i]);
        if (i < file_size - 1) {
            fprintf(out, ", ");
        }
    }
    fprintf(out, "\n};\n");

    fclose(out);
    free(buffer);

    printf("Successfully converted %s to %s\n", input_file, output_c_file);
    return 0;
}