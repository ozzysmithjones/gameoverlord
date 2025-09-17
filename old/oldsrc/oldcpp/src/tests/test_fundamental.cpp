// #include "fundamental.hpp" <- precompiled header

static result test_dynamic_array_reallocation() {
    dynamic_array<int> arr;
    ASSERT(arr.get_capacity() == 0, return result::failure, "Initial capacity of dynamic_array should be 0.");
    ASSERT(arr.get_count() == 0, return result::failure, "Initial count of dynamic_array should be 0.");
    if (arr.append(0) != result::success) {
        BUG("Failed to append first element to dynamic_array.");
        return result::failure;
    }

    ASSERT(arr.get_count() == 1, return result::failure, "Count after appending one element should be 1.");
    size_t initial_capacity = arr.get_capacity();
    ASSERT(initial_capacity == 4, return result::failure, "Initial capacity after first append should be 4.");

    // test reallocation
    for (int i = 1; i < initial_capacity; ++i) {
        if (arr.append(i) != result::success) {
            BUG("Failed to append element to dynamic_array before capacity is exceeded.");
            return result::failure;
        }
    }

    if (arr.append((int)initial_capacity) != result::success) {
        BUG("Failed to append element to dynamic_array when capacity is exceeded.");
        return result::failure;
    }

    ASSERT(arr.get_count() == initial_capacity + 1, return result::failure, "Count after appending elements should be {} but got {}.", initial_capacity + 1, arr.get_count());
    ASSERT(arr.get_capacity() >= (initial_capacity * 2), return result::failure, "Capacity after reallocation should be at least {} but got {}.", initial_capacity * 2, arr.get_capacity());

    for (size_t i = 0; i < arr.get_count(); ++i) {
        int value = 0;
        if (arr.attempt_copy_element(i, value) != result::success) {
            BUG("Failed to attempt_copy_element element from dynamic_array.");
            return result::failure;
        }
        ASSERT(value == static_cast<int>(i), return result::failure, "Expected element to be {} after reallocation, but got {}.", i, value);
    }

    return result::success;
}

// Additional unit tests for fundamental.hpp
static result test_fixed_array() {
    fixed_array<int, 3> arr{ 1, 2, 3 };
    int v = 0;
    ASSERT(arr.attempt_copy_element(0, v) == result::success && v == 1, return result::failure, "fixed_array attempt_copy_element failed");
    ASSERT(arr.attempt_copy_element(2, v) == result::success && v == 3, return result::failure, "fixed_array attempt_copy_element failed");
    ASSERT(arr.attempt_set_element(1, 42) == result::success, return result::failure, "fixed_array attempt_set_element failed");
    ASSERT(arr.element_or(1, -1) == 42, return result::failure, "fixed_array element_or failed");
    return result::success;
}

static result test_capped_array() {
    capped_array<int, 2> arr;
    ASSERT(arr.append(5) == result::success, return result::failure, "capped_array append failed");
    ASSERT(arr.append(7) == result::success, return result::failure, "capped_array append failed");
    int v = 0;
    ASSERT(arr.attempt_copy_element(1, v) == result::success && v == 7, return result::failure, "capped_array attempt_copy_element failed");
    ASSERT(arr.remove(0) == result::success, return result::failure, "capped_array remove failed");
    ASSERT(arr.get_count() == 1, return result::failure, "capped_array count after remove");
    arr.clear();
    ASSERT(arr.get_count() == 0, return result::failure, "capped_array clear failed");
    return result::success;
}

enum class TestEnum {
    A, B, C, count
};
static result test_enum_array() {
    enum_array<TestEnum, int> arr{ 10, 20, 30 };
    int v = 0;
    ASSERT(arr.attempt_copy_element(TestEnum::B, v) == result::success && v == 20, return result::failure, "enum_array attempt_copy_element failed");
    ASSERT(arr.attempt_set_element(TestEnum::C, 99) == result::success, return result::failure, "enum_array attempt_set_element failed");
    ASSERT(arr.element_or(TestEnum::C, -1) == 99, return result::failure, "enum_array element_or failed");
    return result::success;
}

static result test_heap_pointer() {
    heap_pointer<int> ptr;
    ASSERT(ptr.is_null(), return result::failure, "heap_pointer should be null after default construction");
    ASSERT(ptr.default_construct() == result::success, return result::failure, "heap_pointer default_construct failed");
    ASSERT(ptr.is_not_null(), return result::failure, "heap_pointer should not be null after default_construct");
    int& ref = ptr.value_unsafe();
    ref = 123;
    ASSERT(ptr.value_unsafe() == 123, return result::failure, "heap_pointer value_unsafe failed");
    ptr.destroy();
    ASSERT(ptr.is_null(), return result::failure, "heap_pointer should be null after destroy");
    return result::success;
}

enum class BitEnum : uint8_t {
    X = 0, Y, Z, count
};
static result test_enum_bitset() {
    // Test indices mode
    enum_bitset<BitEnum, enum_value::indices> bits;
    bits.insert(BitEnum::X);
    bits.insert(BitEnum::Z);
    ASSERT(bits.contains(BitEnum::X), return result::failure, "enum_bitset should contain X");
    ASSERT(!bits.contains(BitEnum::Y), return result::failure, "enum_bitset should not contain Y");
    ASSERT(bits.contains(BitEnum::Z), return result::failure, "enum_bitset should contain Z");
    bits.remove(BitEnum::X);
    ASSERT(!bits.contains(BitEnum::X), return result::failure, "enum_bitset should not contain X after remove");
    bits.clear();
    ASSERT(bits.copy_to_integer() == 0, return result::failure, "enum_bitset should be empty after clear");
    ASSERT(!bits.contains(BitEnum::Z), return result::failure, "enum_bitset should not contain Z after clear");
    enum_bitset<BitEnum, enum_value::bitmasks> bits2;
    bits2.insert(BitEnum::Y);
    ASSERT(bits2.contains(BitEnum::Y), return result::failure, "enum_bitset (bitmasks) should contain Y");
    return result::success;
}

static result test_fixed_heap_array() {
    fixed_heap_array<int> arr(3);
    ASSERT(arr.get_capacity() == 3, return result::failure, "fixed_heap_array capacity should be 3");
    ASSERT(arr.attempt_set_element(0, 11) == result::success, return result::failure, "fixed_heap_array attempt_set_element failed");
    ASSERT(arr.attempt_set_element(2, 33) == result::success, return result::failure, "fixed_heap_array attempt_set_element failed");
    int v = 0;
    ASSERT(arr.attempt_copy_element(0, v) == result::success && v == 11, return result::failure, "fixed_heap_array attempt_copy_element failed");
    ASSERT(arr.attempt_copy_element(2, v) == result::success && v == 33, return result::failure, "fixed_heap_array attempt_copy_element failed");
    return result::success;
}

namespace test {
    void all_fundamental() {
        ASSERT(test_dynamic_array_reallocation() == result::success, return, "Dynamic array reallocation test failed.");
        ASSERT(test_fixed_array() == result::success, return, "fixed_array test failed.");
        ASSERT(test_capped_array() == result::success, return, "capped_array test failed.");
        ASSERT(test_enum_array() == result::success, return, "enum_array test failed.");
        ASSERT(test_heap_pointer() == result::success, return, "heap_pointer test failed.");
        ASSERT(test_enum_bitset() == result::success, return, "enum_bitset test failed.");
        ASSERT(test_fixed_heap_array() == result::success, return, "fixed_heap_array test failed.");
        std::print("All tests for fundamental.hpp passed.\n");
    }
}