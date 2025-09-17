#ifndef FUNDAMENTAL_H
#define FUNDAMENTAL_H
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#if defined(NDEBUG)
#define BREAKPOINT() ((void)0)
#elif defined(__GNUC__)
#define BREAKPOINT() __builtin_trap()
#elif defined(_MSC_VER)
#define BREAKPOINT() __debugbreak()
#else
#define BREAKPOINT() ((void)0)
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifndef BUG
#include <stdio.h>
#ifndef NDEBUG
#define BUG(...) printf(__FILE__":" TOSTRING(__LINE__) " Bug: " __VA_ARGS__); BREAKPOINT()
#else
#define BUG(...) ((void)0)
#endif
#endif

#define ASSERT(condition, fallback, ...) \
    if (!(condition)) { \
        BUG(__VA_ARGS__); \
        fallback; \
    }

#ifndef NDEBUG
#define DEBUG_ASSERT(condition, fallback, ...) ASSERT(condition, fallback, __VA_ARGS__)
#else
#define DEBUG_ASSERT(condition, fallback, ...) ((void)0) 
#endif

#define STATIC_ASSERT(condition, message) typedef uint8_t static_assertion_##message[(condition) ? 1 : -1];
typedef enum {
    RESULT_FAILURE,
    RESULT_SUCCESS
} result;

typedef struct {
    uint8_t r, g, b, a;
} color;

typedef struct {
    const char* text;
    uint32_t length;
} string;

#define CSTR(string_literal) (string) { .text = string_literal, .length = sizeof(string_literal) - 1 }
static inline bool string_equal(string a, string b) {
    return a.length == b.length && (a.length == 0 || memcmp(a.text, b.text, a.length) == 0);
}

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

#define DECLARE_SLICE(name, element_type) \
    typedef struct { \
        element_type* elements; \
        uint32_t count; \
    } name; \
    static inline element_type* name##_bounds_checked_lookup(name* slice, element_type* fallback, uint32_t index) { \
        ASSERT(slice != NULL, return fallback, "Slice " #name " cannot be NULL"); \
        ASSERT(index < slice->count, return fallback, "Index out of bounds: %u. Count = %u", index, slice->count); \
        return &slice->elements[index]; \
    } \
    static inline result name##_bounds_checked_get(name* slice, uint32_t index, element_type* out_element) { \
        ASSERT(slice != NULL, return RESULT_FAILURE, "Slice " #name " cannot be NULL"); \
        ASSERT(out_element != NULL, return RESULT_FAILURE, "Output element pointer cannot be NULL"); \
        ASSERT(index < slice->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %u", index, slice->count); \
        memcpy(out_element, &slice->elements[index], sizeof(element_type)); \
        return RESULT_SUCCESS; \
    } \
    static inline result name##_bounds_checked_set(name* slice, uint32_t index, element_type value) { \
        ASSERT(slice != NULL, return RESULT_FAILURE, "Slice " #name " cannot be NULL"); \
        ASSERT(index < slice->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %u", index, slice->count); \
        memcpy(&slice->elements[index], &value, sizeof(element_type)); \
        return RESULT_SUCCESS; \
    }

#define DECLARE_CAPPED_ARRAY(name, element_type, capacity) \
    typedef struct { \
        element_type elements[capacity]; \
        uint32_t count; \
    } name; \
    result name##_append(name* array, element_type element); \
    result name##_append_multiple(name* array, element_type* elements, uint32_t count); \
    result name##_insert(name* array, uint32_t index, element_type element); \
    result name##_insert_multiple(name* array, uint32_t index, element_type* elements, uint32_t count); \
    result name##_remove(name* array, uint32_t index); \
    result name##_remove_swap(name* array, uint32_t index); \
    result name##_find(name* array, element_type element, uint32_t* out_index); \
    static inline void name##_clear(name* array) { \
        ASSERT(array != NULL, return, "Capped array " #name " cannot be NULL"); \
        array->count = 0; \
    } \
    static inline element_type* name##_bounds_checked_lookup(name* array, element_type* fallback, uint32_t index) { \
        ASSERT(array != NULL, return fallback, "Capped array " #name " cannot be NULL"); \
        ASSERT(index < array->count, return fallback, "Index out of bounds: %u. Count = %u", index, array->count); \
        return &array->elements[index]; \
    } \
    static inline result name##_bounds_checked_get(name* array, uint32_t index, element_type* out_element) { \
        ASSERT(array != NULL, return RESULT_FAILURE, "Capped array " #name " cannot be NULL"); \
        ASSERT(out_element != NULL, return RESULT_FAILURE, "Output element pointer cannot be NULL"); \
        ASSERT(index < array->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %u", index, array->count); \
        memcpy(out_element, &array->elements[index], sizeof(element_type)); \
        return RESULT_SUCCESS; \
    } \
    static inline result name##_bounds_checked_set(name* array, uint32_t index, element_type value) { \
        ASSERT(array != NULL, return RESULT_FAILURE, "Capped array " #name " cannot be NULL"); \
        ASSERT(index < array->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %u", index, array->count); \
        memcpy(&array->elements[index], &value, sizeof(element_type)); \
        return RESULT_SUCCESS; \
    }

#define IMPLEMENT_CAPPED_ARRAY(name, element_type, capacity) \
    result name##_insert(name* array, uint32_t index, element_type element) { \
        ASSERT(array != NULL, return RESULT_FAILURE, "Capped array " #name " cannot be NULL"); \
        ASSERT(index <= array->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %u", index, array->count); \
        ASSERT(array->count < capacity, return RESULT_FAILURE, "Capped array " #name " capacity exceeded: %u, cannot insert element.", capacity); \
        if (array->count > 0 && index < array->count) { \
            memmove(&array->elements[index + 1], &array->elements[index], (array->count - index) * sizeof(element_type)); \
        } \
        memcpy(&array->elements[index], &element, sizeof(element_type)); \
        ++array->count; \
        return RESULT_SUCCESS; \
    } \
    result name##_insert_multiple(name* array, uint32_t index, element_type* elements, uint32_t count) { \
        ASSERT(array != NULL, return RESULT_FAILURE, "Capped array " #name " cannot be NULL"); \
        ASSERT(index <= array->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %u", index, array->count); \
        ASSERT(array->count + count <= capacity, return RESULT_FAILURE, "Capped array " #name " capacity exceeded: %u, cannot insert elements.", capacity); \
        if (array->count > 0 && index < array->count) { \
            memmove(&array->elements[index + count], &array->elements[index], (array->count - index) * sizeof(element_type)); \
        } \
        memcpy(&array->elements[index], elements, count * sizeof(element_type)); \
        array->count += count; \
        return RESULT_SUCCESS; \
    } \
    result name##_append(name* array, element_type element) { \
        ASSERT(array != NULL, return RESULT_FAILURE, "Capped array " #name " cannot be NULL"); \
        ASSERT(array->count < capacity, return RESULT_FAILURE, "Capped array " #name " capacity exceeded: %u, cannot append element.", capacity); \
        memcpy(&array->elements[array->count++], &element, sizeof(element_type)); \
        return RESULT_SUCCESS; \
    } \
    result name##_append_multiple(name* array, element_type* elements, uint32_t count) { \
        ASSERT(array != NULL, return RESULT_FAILURE, "Capped array " #name " cannot be NULL"); \
        ASSERT(array->count + count <= capacity, return RESULT_FAILURE, "Capped array " #name " capacity exceeded: %u, cannot append elements.", capacity); \
        memcpy(&array->elements[array->count], elements, count * sizeof(element_type)); \
        array->count += count; \
        return RESULT_SUCCESS; \
    } \
    result name##_remove(name* array, uint32_t index) { \
        ASSERT(array != NULL, return RESULT_FAILURE, "Capped array " #name " cannot be NULL"); \
        ASSERT(index < array->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %u", index, array->count); \
        if (array->count > 0) { \
            memmove(&array->elements[index], &array->elements[index + 1], (array->count - (index - 1)) * sizeof(element_type)); \
        } \
        --array->count; \
        return RESULT_SUCCESS; \
    } \
    result name##_remove_swap(name* array, uint32_t index) { \
        ASSERT(array != NULL, return RESULT_FAILURE, "Capped array " #name " cannot be NULL"); \
        ASSERT(index < array->count, return RESULT_FAILURE, "Index out of bounds: %u. Count = %u", index, array->count); \
        if (array->count > 0) { \
            memcpy(&array->elements[index], &array->elements[array->count - 1], sizeof(element_type)); \
        } \
        --array->count; \
        return RESULT_SUCCESS; \
    } \
    result name##_find(name* array, element_type element, uint32_t* out_index) {\
        ASSERT(array != NULL, return RESULT_FAILURE, "Capped array " #name " cannot be NULL"); \
        for (uint32_t i = 0; i < array->count; ++i) { \
            if (memcmp(&array->elements[i], &element, sizeof(element_type)) == 0) { \
                if (out_index) { \
                    *out_index = i; \
                } \
                return RESULT_SUCCESS; \
            } \
        } \
        return RESULT_FAILURE; \
    }

#define FOR_EACH_0(action)
#define FOR_EACH_1(action, op0) action(op0)
#define FOR_EACH_2(action, op0, op1) FOR_EACH_1(action, op0) action(op1)
#define FOR_EACH_3(action, op0, op1, op2) FOR_EACH_2(action, op0, op1) action(op2)
#define FOR_EACH_4(action, op0, op1, op2, op3) FOR_EACH_3(action, op0, op1, op2) action(op3)
#define FOR_EACH_5(action, op0, op1, op2, op3, op4) FOR_EACH_4(action, op0, op1, op2, op3) action(op4)
#define FOR_EACH_6(action, op0, op1, op2, op3, op4, op5) FOR_EACH_5(action, op0, op1, op2, op3, op4) action(op5)
#define FOR_EACH_7(action, op0, op1, op2, op3, op4, op5, op6) FOR_EACH_6(action, op0, op1, op2, op3, op4, op5) action(op6)
#define FOR_EACH_8(action, op0, op1, op2, op3, op4, op5, op6, op7) FOR_EACH_7(action, op0, op1, op2, op3, op4, op5, op6) action(op7)
#define FOR_EACH_9(action, op0, op1, op2, op3, op4, op5, op6, op7, op8) FOR_EACH_8(action, op0, op1, op2, op3, op4, op5, op6, op7) action(op8)
#define FOR_EACH_10(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9) FOR_EACH_9(action, op0, op1, op2, op3, op4, op5, op6, op7, op8) action(op9)
#define FOR_EACH_11(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10) FOR_EACH_10(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9) action(op10)
#define FOR_EACH_12(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11) FOR_EACH_11(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10) action(op11)
#define FOR_EACH_13(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12) FOR_EACH_12(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11) action(op12)
#define FOR_EACH_14(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13) FOR_EACH_13(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12) action(op13)
#define FOR_EACH_15(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14) FOR_EACH_14(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13) action(op14)
#define FOR_EACH_16(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15) FOR_EACH_15(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14) action(op15)
#define FOR_EACH_17(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16) FOR_EACH_16(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15) action(op16)
#define FOR_EACH_18(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17) FOR_EACH_17(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16) action(op17)
#define FOR_EACH_19(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18) FOR_EACH_18(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17) action(op18)
#define FOR_EACH_20(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19) FOR_EACH_19(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18) action(op19)
#define FOR_EACH_21(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20) FOR_EACH_20(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19) action(op20)
#define FOR_EACH_22(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21) FOR_EACH_21(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20) action(op21)
#define FOR_EACH_23(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22) FOR_EACH_22(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21) action(op22)
#define FOR_EACH_24(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23) FOR_EACH_23(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22) action(op23)
#define FOR_EACH_25(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24) FOR_EACH_24(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23) action(op24)
#define FOR_EACH_26(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25) FOR_EACH_25(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24) action(op25)
#define FOR_EACH_27(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26) FOR_EACH_26(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25) action(op26)
#define FOR_EACH_28(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26, op27) FOR_EACH_27(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26) action(op27)
#define FOR_EACH_29(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26, op27, op28) FOR_EACH_28(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26, op27) action(op28)
#define FOR_EACH_30(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26, op27, op28, op29) FOR_EACH_29(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26, op27, op28) action(op29)
#define FOR_EACH_31(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26, op27, op28, op29, op30) FOR_EACH_30(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26, op27, op28, op29) action(op30)
#define FOR_EACH_32(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26, op27, op28, op29, op30, op31) FOR_EACH_31(action, op0, op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15, op16, op17, op18, op19, op20, op21, op22, op23, op24, op25, op26, op27, op28, op29, op30) action(op31)

#define INTERNAL_GET_ARG_COUNT(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, N, ...) N

#define GET_ARG_COUNT(...) INTERNAL_GET_ARG_COUNT(__VA_ARGS__, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, )

#define CONCATENATE(a, b) a##b
#define IMPL_SELECT_FOR_EACH(count) CONCATENATE(FOR_EACH_, count)
#define FOR_EACH(action, ...) IMPL_SELECT_FOR_EACH(GET_ARG_COUNT(__VA_ARGS__))(action, __VA_ARGS__)

// generated code end

// ENUM_WITH_TO_STRING macro, so you can declare an enum with string conversion (uses FOR_EACH macro)

#define STRINGIFY_ELEMENT(x) STRINGIFY(x),
#define ENUM_WITH_TO_STRING(enum_name, ...) \
 typedef enum { \
    __VA_ARGS__, \
     enum_name##_count \
 } enum_name; \
 static const char* enum_name##_strings[enum_name##_count] = { \
     FOR_EACH(STRINGIFY_ELEMENT, __VA_ARGS__) \
 }; \
 static inline const char* enum_name##_to_string(enum_name value) { \
     if (value < 0 || value >= enum_name##_count) { \
         return "ENUM_UNKNOWN"; \
     } \
     return enum_name##_strings[value]; \
 }


#endif // FUNDAMENTAL_H