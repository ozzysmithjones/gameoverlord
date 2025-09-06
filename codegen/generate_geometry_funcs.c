#include <stdio.h>

void generate_geometry_code(void);

#define DIMENSION_COUNT 4
#define DIMENSION_TYPE_COUNT 3
#define OPERATION_COUNT 4

char char_by_dimension[DIMENSION_COUNT] = { 'x', 'y', 'z', 'w' };
const char* dimension_types[DIMENSION_TYPE_COUNT] = { "float", "double", "int32_t" };
char dimension_postfixes[DIMENSION_TYPE_COUNT] = { 'f', 'd', '\0' };
const char* name_by_operation[OPERATION_COUNT] = { "add", "sub", "mul", "div" };
char char_by_operation[OPERATION_COUNT] = { '+', '-', '*', '/' };

typedef enum {
  TYPE_FLOAT,
  TYPE_DOUBLE,
  TYPE_INT32
} dimension_type;

typedef enum {
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV
} op;


static void generate_vec_binary_func(FILE* file, const char* vec_type_name, int dimensions, op operation) {
  fprintf(file, "static inline %s %s_%s(%s a, %s b) {\n", vec_type_name, vec_type_name, name_by_operation[operation], vec_type_name, vec_type_name);
  fprintf(file, "\t%s result;\n", vec_type_name);
  for (int i = 0; i < dimensions; ++i) {
    fprintf(file, "\tresult.%c = a.%c %c b.%c;\n", char_by_dimension[i], char_by_dimension[i], char_by_operation[operation], char_by_dimension[i]);
  }
  fprintf(file, "\treturn result;\n");
  fprintf(file, "}\n\n");
}

static void generate_vec_dot_product_func(FILE* file, const char* vec_type_name, int dimensions, int type) {
  fprintf(file, "static inline %s %s_dot(%s a, %s b) {\n", dimension_types[type], vec_type_name, vec_type_name, vec_type_name);
  fprintf(file, "\t%s result = 0;\n", dimension_types[type]);
  for (int i = 0; i < dimensions; ++i) {
    fprintf(file, "\tresult += a.%c * b.%c;\n", char_by_dimension[i], char_by_dimension[i]);
  }
  fprintf(file, "\treturn result;\n");
  fprintf(file, "}\n\n");
}

static void generate_vec_length_func(FILE* file, const char* vec_type_name) {
  fprintf(file, "static inline double %s_length(%s v) {\n", vec_type_name, vec_type_name);
  fprintf(file, "\treturn sqrt(%s_dot(v, v));\n", vec_type_name);
  fprintf(file, "}\n\n");
}

static void generate_vec_normalize_func(FILE* file, const char* vec_type_name, int dimensions) {
  fprintf(file, "static inline %s %s_normalize(%s v) {\n", vec_type_name, vec_type_name, vec_type_name);
  fprintf(file, "\tdouble length = %s_length(v);\n", vec_type_name);
  fprintf(file, "\tif (length == 0) return v; // Avoid division by zero\n");

  fprintf(file, "\t%s result;\n", vec_type_name);
  for (int i = 0; i < dimensions; ++i) {
    fprintf(file, "\tresult.%c = v.%c / length;\n", char_by_dimension[i], char_by_dimension[i]);
  }
  fprintf(file, "\treturn result;\n");
  fprintf(file, "}\n\n");
}

static void generate_vec_sqr_length_func(FILE* file, const char* vec_type_name, int type) {
  fprintf(file, "static inline %s %s_sqr_length(%s v) {\n", dimension_types[type], vec_type_name, vec_type_name);
  fprintf(file, "\treturn %s_dot(v, v);\n", vec_type_name);
  fprintf(file, "}\n\n");
}

static void generate_vec_code(FILE* file, int dimensions, int type) {
  // generate vec name and type
  char vec_type_name[32] = { 0 };
  for (int i = 0; i < dimensions; ++i) {
    vec_type_name[i] = char_by_dimension[i];
  }

  vec_type_name[dimensions] = dimension_postfixes[type];
  vec_type_name[dimensions + 1] = '\0';


  fprintf(file, "typedef struct {\n");
  for (int i = 0; i < dimensions; ++i) {
    fprintf(file, "\t%s %c;\n", dimension_types[type], char_by_dimension[i]);
  }
  fprintf(file, "} %s;\n\n", vec_type_name);

  // generate common functions
  generate_vec_binary_func(file, vec_type_name, dimensions, OP_ADD);
  generate_vec_binary_func(file, vec_type_name, dimensions, OP_SUB);
  generate_vec_binary_func(file, vec_type_name, dimensions, OP_MUL);
  generate_vec_binary_func(file, vec_type_name, dimensions, OP_DIV);
  generate_vec_dot_product_func(file, vec_type_name, dimensions, type);
  generate_vec_length_func(file, vec_type_name);
  generate_vec_normalize_func(file, vec_type_name, dimensions);
  generate_vec_sqr_length_func(file, vec_type_name, type);
}


void generate_geometry_code(void) {
  FILE* file = fopen("geometry.h", "w");
  fprintf(file, "// Auto-generated geometry code\n\n");
  fprintf(file, "#ifndef GEOMETRY_H\n#define GEOMETRY_H\n\n");
  fprintf(file, "#include <stdint.h>\n#include <math.h>\n\n");
  for (size_t i = 2; i < (DIMENSION_COUNT + 1); ++i) {
    generate_vec_code(file, i, TYPE_FLOAT);
    generate_vec_code(file, i, TYPE_DOUBLE);
    generate_vec_code(file, i, TYPE_INT32);
  }
  fprintf(file, "#endif // GEOMETRY_H\n");
  fclose(file);
}

