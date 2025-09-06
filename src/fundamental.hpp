#pragma once
#include <print>
#include <cstdint>
#include <cstddef>
#include <bit>
#include <type_traits>
#include <concepts>

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

#ifndef NDEBUG
#define BUG(...) std::print(__FILE__" :" TOSTRING(__LINE__) " Bug: " __VA_ARGS__); BREAKPOINT()
#else
#define BUG(...) ((void)0)
#endif


#define ASSERT(condition, fallback, ...) \
    if (!(condition)) [[unlikely]] { \
        BUG(__VA_ARGS__); \
        fallback; \
    }

#ifndef NDEBUG
#define DEBUG_ASSERT(condition, fallback, ...) ASSERT(condition, fallback, __VA_ARGS__)
#else
#define DEBUG_ASSERT(condition, fallback, ...) ((void)0)
#endif


template<typename T>
concept allocator_concept = requires(T a, void* ptr, size_t bytes, size_t realloc_bytes, size_t alignment) {
    {
        a.malloc(alignment, bytes)
    } -> std::same_as<void*>;
    {
        a.realloc(ptr, alignment, bytes, realloc_bytes)
    } -> std::same_as<void*>;
    {
        a.free(ptr, alignment, bytes)
    };
};

struct heap_allocator {
    void* malloc(size_t alignment, size_t bytes);
    void* realloc(void* ptr, size_t alignment, size_t old_bytes, size_t new_bytes);
    void free(void* ptr, size_t alignment, size_t bytes);
};

enum class result {
    failure,
    success
};

namespace detail {
    template<typename T>
    concept enum_count_concept = std::is_enum_v<T> && requires {
        { T::count } -> std::same_as<T>;
    };
}

template<typename T>
auto enum_to_underlying(T e) -> std::underlying_type_t<T> {
    return static_cast<std::underlying_type_t<T>>(e);
}

template<typename Enum, typename T>
class enum_array {
    static_assert(std::is_enum_v<Enum>, "Enum must be an enum type");
    static_assert(detail::enum_count_concept<Enum>, "Enum must have a count field to indicate number of values in enum_array");
    static constexpr size_t Capacity = static_cast<size_t>(Enum::count);
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    constexpr enum_array() requires (std::is_default_constructible_v<T>) : elements{} {
    }
    constexpr enum_array(std::initializer_list<T> init) {
        ASSERT(init.size() <= Capacity, return, "Initializer list size exceeds enum_array capacity.");
        size_t i = 0;
        for (const T& value : init) {
            new (&elements[i]) T(value);
            ++i;
        }
    }

    constexpr const_reference lookup(Enum e, const_reference fallback) const {
        size_type index = static_cast<size_type>(e);
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds in dynamic_array::lookup.");
            return fallback;
        }
        return elements[index];
    }

    constexpr reference lookup(Enum e, reference fallback) {
        size_type index = static_cast<size_type>(e);
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds in dynamic_array::lookup.");
            return fallback;
        }
        return elements[index];
    }

    constexpr const_reference lookup_unsafe(Enum e) const {
        size_type index = static_cast<size_type>(e);
        DEBUG_ASSERT(index < Capacity, return elements[0], "Index out of bounds in dynamic_array::lookup_unsafe.");
        return elements[index];
    }

    constexpr reference lookup_unsafe(Enum e) {
        size_type index = static_cast<size_type>(e);
        DEBUG_ASSERT(index < Capacity, return elements[0], "Index out of bounds in dynamic_array::lookup_unsafe.");
        return elements[index];
    }

    constexpr bool contains(const T& value) const {
        for (size_type i = 0; i < Capacity; ++i) {
            if (elements[i] == value) {
                return true;
            }
        }
        return false;
    }

    constexpr void set_everything(const T& value) {
        for (size_type i = 0; i < Capacity; ++i) {
            elements[i] = value;
        }
    }

    constexpr bool find(const T& value, Enum& out_enumeration) const {
        for (size_type i = 0; i < Capacity; ++i) {
            if (elements[i] == value) {
                out_enumeration = static_cast<Enum>(i);
                return true;
            }
        }
        return false;
    }

    template<typename Predicate>
        requires std::is_invocable_r_v<bool, Predicate, const T&>
    constexpr bool find_if(Predicate predicate, Enum& out_enumeration) const {
        for (size_type i = 0; i < Capacity; ++i) {
            if (predicate(elements[i])) {
                out_enumeration = static_cast<Enum>(i);
                return true;
            }
        }
        return false;
    }

    constexpr iterator begin() {
        return elements;
    }

    constexpr iterator end() {
        return elements + Capacity;
    }

    constexpr const_iterator begin() const {
        return elements;
    }

    constexpr const_iterator end() const {
        return elements + Capacity;
    }

private:
    T elements[Capacity];
};


template<typename T, size_t Capacity>
class fixed_array {
    static_assert(Capacity > 0, "Capacity must be greater than 0");
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    constexpr fixed_array() requires (std::is_default_constructible_v<T>) : elements{} {
    }

    constexpr fixed_array(std::initializer_list<T> init) {
        ASSERT(init.size() <= Capacity, return, "Initializer list size exceeds fixed_array capacity.");
        size_t i = 0;
        for (const T& value : init) {
            new (&elements[i]) T(value);
            ++i;
        }
    }

    constexpr const_reference lookup(size_type index, const_reference fallback) const {
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds in dynamic_array::lookup.");
            return fallback;
        }
        return elements[index];
    }

    constexpr reference lookup(size_type index, reference fallback) {
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds in dynamic_array::lookup.");
            return fallback;
        }
        return elements[index];
    }

    constexpr const_reference lookup_unsafe(size_type index) const {
        DEBUG_ASSERT(index < Capacity, return elements[0], "Index out of bounds in dynamic_array::lookup_unsafe.");
        return elements[index];
    }

    constexpr reference lookup_unsafe(size_type index) {
        DEBUG_ASSERT(index < Capacity, return elements[0], "Index out of bounds in dynamic_array::lookup_unsafe.");
        return elements[index];
    }

    constexpr bool bounds_check(size_type index) const {
        return index < Capacity;
    }

    constexpr bool contains(const T& value) const {
        for (size_type i = 0; i < Capacity; ++i) {
            if (elements[i] == value) {
                return true;
            }
        }
        return false;
    }

    constexpr void set_everything(const T& value) {
        for (size_type i = 0; i < Capacity; ++i) {
            elements[i] = value;
        }
    }

    constexpr bool find(const T& value, size_type& out_index) const {
        for (size_type i = 0; i < Capacity; ++i) {
            if (elements[i] == value) {
                out_index = i;
                return true;
            }
        }
        return false;
    }

    template<typename Predicate>
        requires std::is_invocable_r_v<bool, Predicate, const T&>
    constexpr bool find_if(Predicate predicate, size_type& out_index) const {
        for (size_type i = 0; i < Capacity; ++i) {
            if (predicate(elements[i])) {
                out_index = i;
                return true;
            }
        }
        return false;
    }

    constexpr iterator begin() {
        return elements;
    }

    constexpr iterator end() {
        return elements + Capacity;
    }

    constexpr const_iterator begin() const {
        return elements;
    }

    constexpr const_iterator end() const {
        return elements + Capacity;
    }

private:
    T elements[Capacity];
};


template<typename T, size_t Capacity>
class capped_array {
    static_assert(Capacity > 0, "Capacity must be greater than 0");
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    capped_array() : elements_data{}, count(0) {
    }


    template<typename Arg>
    result append(Arg&& value) {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        if (count < Capacity) {
            new (&elements[count]) T(std::forward<Arg>(value));
            ++count;
            return result::success;
        }
        return result::failure;
    }

    result append_multiple(const_pointer values, size_type num_values) {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        ASSERT(values != nullptr || num_values == 0, return result::failure, "values cannot be null if num_values is greater than 0 in capped_array::append_multiple.");
        if (count + num_values <= Capacity) {
            for (size_type i = 0; i < num_values; ++i, ++count) {
                new (&elements[count]) T(values[i]);
            }
            return result::success;
        }
        return result::failure;
    }

    template<size_t N>
    result append_multiple(const T(&values)[N]) {
        return append_multiple(values, N);
    }

    result remove(size_type index) {
        static_assert(std::is_nothrow_move_constructible_v<T>, "T must be nothrow move constructible to use dynamic_array::remove.");
        pointer elements = reinterpret_cast<pointer>(elements_data);
        ASSERT(index < count, return result::failure, "Index out of bounds in dynamic_array::remove.");
        std::destroy_at(&elements[index]);
        for (size_type i = index; i < count - 1; ++i) {
            new (&elements[i]) T(std::move(elements[i + 1]));
            std::destroy_at(&elements[i + 1]);
        }
        --count;
        return result::success;
    }

    result remove_swap(size_type index) {
        static_assert(std::is_nothrow_move_constructible_v<T>, "T must be nothrow move constructible to use dynamic_array::remove_swap.");
        pointer elements = reinterpret_cast<pointer>(elements_data);
        ASSERT(index < count, return result::failure, "Index out of bounds in dynamic_array::remove_swap.");
        std::destroy_at(&elements[index]);
        if (index != count - 1) {
            new (&elements[index]) T(std::move(elements[count - 1]));
            std::destroy_at(&elements[count - 1]);
        }
        --count;
        return result::success;
    }

    void clear() {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = 0; i < count; ++i) {
                std::destroy_at(&elements[i]);
            }
        }
        count = 0;
    }

    const_reference lookup(size_type index, const_reference fallback) const {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds in dynamic_array::lookup.");
            return fallback;
        }
        return elements[index];
    }

    reference lookup(size_type index, reference fallback) {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds in dynamic_array::lookup.");
            return fallback;
        }
        return elements[index];
    }

    const_reference lookup_unsafe(size_type index) const {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds in dynamic_array::lookup_unsafe.");
        return elements[index];
    }

    reference lookup_unsafe(size_type index) {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds in dynamic_array::lookup_unsafe.");
        return elements[index];
    }

    bool bounds_check(size_type index) const {
        return index < count;
    }

    bool contains(const T& value) const {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        for (size_type i = 0; i < count; ++i) {
            if (elements[i] == value) {
                return true;
            }
        }
        return false;
    }

    constexpr void set_everything(const T& value) {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        for (size_type i = 0; i < count; ++i) {
            elements[i] = value;
        }
    }

    bool find(const T& value, size_type& out_index) const {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        for (size_type i = 0; i < count; ++i) {
            if (elements[i] == value) {
                out_index = i;
                return true;
            }
        }
        return false;
    }

    template<typename Predicate>
        requires std::is_invocable_r_v<bool, Predicate, const T&>
    bool find_if(Predicate predicate, size_type& out_index) const {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        for (size_type i = 0; i < count; ++i) {
            if (predicate(elements[i])) {
                out_index = i;
                return true;
            }
        }
        return false;
    }

    iterator begin() {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        return elements;
    }

    iterator end() {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        return elements + count;
    }

    const_iterator begin() const {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        return elements;
    }

    const_iterator end() const {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        return elements + count;
    }

    pointer get_data() {
        return reinterpret_cast<pointer>(elements_data);
    }

    const_pointer get_data() const {
        return reinterpret_cast<const_pointer>(elements_data);
    }

    size_type get_count() const {
        return count;
    }

    constexpr size_type get_capacity() const {
        return Capacity;
    }

    bool operator==(const capped_array& other) const {
        T* elements = reinterpret_cast<T*>(elements_data);
        T* other_elements = reinterpret_cast<T*>(other.elements_data);
        if (count != other.count) {
            return false;
        }
        for (size_type i = 0; i < count; ++i) {
            if (elements[i] != other_elements[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const capped_array& other) const {
        return !(*this == other);
    }

private:
    union {
        // Using a union so that we can display the elements data in the debugger. (elements_debugger_visual is never used in the code itself, but can be seen in the debugger to see the internals of elements_data in a more user-friendly way)
        T elements_debugger_visual[Capacity];
        alignas(T) uint8_t elements_data[sizeof(T) * Capacity];
    };

    size_type count = 0;
};


/// @brief dynamic array for storing a sequence of elements of type T. Similar to std::vector but with a clearer interface and it assumes that your elements are trivially relocatable so that it can grow more efficiently.
/// @tparam T 
/// @tparam Allocator 
template<typename T, allocator_concept Allocator = heap_allocator>
class dynamic_array {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    Allocator& get_allocator() const {
        return allocator;
    }
    void set_allocator(const Allocator& new_allocator) {
        ASSERT(count == 0, return, "Cannot change allocator of dynamic_array when it is not empty.");
        if (elements != nullptr) {
            allocator.free(elements, alignof(T), sizeof(T) * capacity);
            elements = nullptr;
            count = 0;
            capacity = 0;
        }
        allocator = new_allocator;
    }

    dynamic_array() : allocator(), elements(nullptr), count(0), capacity(0) {
    }
    dynamic_array(const dynamic_array& other) {
        append_multiple(other.elements, other.count);
    }
    dynamic_array(dynamic_array&& other) noexcept {
        elements = other.elements;
        count = other.count;
        capacity = other.capacity;
        other.elements = nullptr;
        other.count = 0;
        other.capacity = 0;
    }
    ~dynamic_array() {
        clear();
    }

    dynamic_array& operator=(const dynamic_array& other) {
        if (this != &other) {
            clear();
            append_multiple(other.elements, other.count);
        }
        return *this;
    }

    dynamic_array& operator=(dynamic_array&& other) noexcept {
        if (this != &other) {
            clear();
            allocator.free(elements, alignof(T), sizeof(T) * capacity);

            elements = other.elements;
            count = other.count;
            capacity = other.capacity;
            other.elements = nullptr;
            other.count = 0;
            other.capacity = 0;
        }
        return *this;
    }

    template<typename Arg>
    result append(Arg&& value) {
        if (count == capacity) {
            size_type new_capacity = capacity == 0 ? 1 : (capacity << 1);
            pointer new_elements = reinterpret_cast<pointer>(allocator.realloc(
                elements,
                alignof(T),
                sizeof(T) * capacity,
                sizeof(T) * new_capacity
            ));
            ASSERT(new_elements != nullptr, return result::failure, "Out of memory, cannot append element to dynamic_array.");
            elements = new_elements;
            capacity = new_capacity;
        }
        new (&elements[count]) T(std::forward<Arg>(value));
        ++count;
        return result::success;
    }


    result append_multiple(const_pointer values, size_type num_values) {
        ASSERT(values != nullptr || num_values == 0, return result::failure, "values cannot be null if num_values is greater than 0 in dynamic_array::append_multiple.");
        while (count + num_values > capacity) {
            size_type new_capacity = std::bit_ceil(count + num_values);
            pointer new_elements = reinterpret_cast<pointer>(allocator.realloc(
                elements,
                alignof(T),
                sizeof(T) * capacity,
                sizeof(T) * new_capacity
            ));
            ASSERT(new_elements != nullptr, return result::failure, "Out of memory, cannot append multiple elements to dynamic_array.");
            elements = new_elements;
            capacity = new_capacity;
        }
        for (size_type i = 0; i < num_values; ++i, ++count) {
            new (&elements[count]) T(values[i]);
        }
        return result::success;
    }

    template<size_t N>
    result append_multiple(const T(&values)[N]) {
        return append_multiple(values, N);
    }

    result remove(size_type index) {
        static_assert(std::is_nothrow_move_constructible_v<T>, "T must be nothrow move constructible to use dynamic_array::remove.");
        ASSERT(index < count, return result::failure, "Index out of bounds in dynamic_array::remove.");
        std::destroy_at(&elements[index]);
        for (size_type i = index; i < count - 1; ++i) {
            new (&elements[i]) T(std::move(elements[i + 1]));
            std::destroy_at(&elements[i + 1]);
        }
        --count;
        return result::success;
    }

    result remove_swap(size_type index) {
        static_assert(std::is_nothrow_move_constructible_v<T>, "T must be nothrow move constructible to use dynamic_array::remove_swap.");
        ASSERT(index < count, return result::failure, "Index out of bounds in dynamic_array::remove_swap.");
        std::destroy_at(&elements[index]);
        if (index != count - 1) {
            new (&elements[index]) T(std::move(elements[count - 1]));
            std::destroy_at(&elements[count - 1]);
        }
        --count;
        return result::success;
    }

    void clear() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = 0; i < count; ++i) {
                std::destroy_at(&elements[i]);
            }
        }
        count = 0;
    }

    const_reference lookup(size_type index, const_reference fallback) const {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds in dynamic_array::lookup.");
            return fallback;
        }
        return elements[index];
    }

    reference lookup(size_type index, reference fallback) {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds in dynamic_array::lookup.");
            return fallback;
        }
        return elements[index];
    }

    const_reference lookup_unsafe(size_type index) const {
        DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds in dynamic_array::lookup_unsafe.");
        return elements[index];
    }

    reference lookup_unsafe(size_type index) {
        DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds in dynamic_array::lookup_unsafe.");
        return elements[index];
    }

    bool bounds_check(size_type index) const {
        return index < count;
    }

    bool contains(const T& value) const {
        for (size_type i = 0; i < count; ++i) {
            if (elements[i] == value) {
                return true;
            }
        }
        return false;
    }

    constexpr void set_everything(const T& value) {
        for (size_type i = 0; i < count; ++i) {
            elements[i] = value;
        }
    }

    bool find(const T& value, size_type& out_index) const {
        for (size_type i = 0; i < count; ++i) {
            if (elements[i] == value) {
                out_index = i;
                return true;
            }
        }
        return false;
    }

    template<typename Predicate>
        requires std::is_invocable_r_v<bool, Predicate, const T&>
    bool find_if(Predicate predicate, size_type& out_index) const {
        for (size_type i = 0; i < count; ++i) {
            if (predicate(elements[i])) {
                out_index = i;
                return true;
            }
        }
        return false;
    }

    iterator begin() {
        return elements;
    }

    iterator end() {
        return elements + count;
    }

    const_iterator begin() const {
        return elements;
    }

    const_iterator end() const {
        return elements + count;
    }

    bool operator==(const dynamic_array& other) const {
        if (count != other.count) {
            return false;
        }
        for (size_type i = 0; i < count; ++i) {
            if (elements[i] != other.elements[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const dynamic_array& other) const {
        return !(*this == other);
    }

    void preallocate_memory_for(size_type additional_expected_elements) {
        if (count + additional_expected_elements <= capacity) {
            return;
        }
        size_type new_capacity = std::bit_ceil(count + additional_expected_elements);
        pointer new_elements = static_cast<pointer>(allocator.realloc(
            elements,
            alignof(T),
            sizeof(T) * capacity,
            sizeof(T) * new_capacity
        ));
        ASSERT(new_elements != nullptr, return, "Out of memory, cannot preallocate space in dynamic_array.");
        elements = new_elements;
        capacity = new_capacity;
    }

    void preallocate_exact_memory_for(size_type additional_expected_elements) {
        if (count + additional_expected_elements <= capacity) {
            return;
        }
        size_type new_capacity = count + additional_expected_elements;
        pointer new_elements = static_cast<pointer>(allocator.realloc(
            elements,
            alignof(T),
            sizeof(T) * capacity,
            sizeof(T) * new_capacity
        ));
        ASSERT(new_elements != nullptr, return, "Out of memory, cannot preallocate space in dynamic_array.");
        elements = new_elements;
        capacity = new_capacity;
    }

    pointer get_data() {
        return elements;
    }

    size_type get_count() const {
        return count;
    }

    size_type get_capacity() const {
        return capacity;
    }

private:
    Allocator allocator;
    pointer elements = nullptr;
    size_type count = 0;
    size_type capacity = 0;
};


template <typename T>
class slice {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    constexpr slice() : elements(nullptr), count(0) {
    }
    constexpr slice(std::nullptr_t) : elements(nullptr), count(0) {
    }
    constexpr slice(pointer start, pointer end) : elements(start), count(end - start) {
    }
    constexpr slice(pointer elements, size_type count) : elements(elements), count(count) {
    }

    template<size_type N>
    constexpr slice(T(&array)[N]) : elements(array), count(N) {
    }

    constexpr iterator begin() {
        return elements;
    }

    constexpr iterator end() {
        return elements + count;
    }

    constexpr const_iterator begin() const {
        return elements;
    }

    constexpr const_iterator end() const {
        return elements + count;
    }

    bool operator==(const slice& other) const {
        if (count != other.count) {
            return false;
        }
        for (size_type i = 0; i < count; ++i) {
            if (elements[i] != other.elements[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const slice& other) const {
        return !(*this == other);
    }

    const_reference lookup(size_type index, const_reference fallback) const {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds in dynamic_array::lookup.");
            return fallback;
        }
        return elements[index];
    }

    reference lookup(size_type index, reference fallback) {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds in dynamic_array::lookup.");
            return fallback;
        }
        return elements[index];
    }

    const_reference lookup_unsafe(size_type index) const {
        DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds in dynamic_array::lookup_unsafe.");
        return elements[index];
    }

    reference lookup_unsafe(size_type index) {
        DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds in dynamic_array::lookup_unsafe.");
        return elements[index];
    }

    bool bounds_check(size_type index) const {
        return index < count;
    }

    bool contains(const T& value) const {
        for (size_type i = 0; i < count; ++i) {
            if (elements[i] == value) {
                return true;
            }
        }
        return false;
    }

    bool find(const T& value, size_type& out_index) const {
        for (size_type i = 0; i < count; ++i) {
            if (elements[i] == value) {
                out_index = i;
                return true;
            }
        }
        return false;
    }

    template<typename Predicate>
        requires std::is_invocable_r_v<bool, Predicate, const T&>
    bool find_if(Predicate predicate, size_type& out_index) const {
        for (size_type i = 0; i < count; ++i) {
            if (predicate(elements[i])) {
                out_index = i;
                return true;
            }
        }
        return false;
    }

    size_type get_count() const {
        return count;
    }

    T* get_data() {
        return elements;
    }

private:
    T* elements;
    size_type count;
};
using string_slice = slice<const char>;

enum class enum_value {
    /// @brief The enum values are 0, 1, 2, 3, ... and represent the bit index (need to be shifted).
    indices,
    /// @brief The enum values are 1, 2, 4, 8, ... and represent the bit mask directly.
    bitmasks
};

template<typename T, enum_value Kind>
class enum_bitset {
    static_assert(std::is_enum_v<T>, "T must be an enum type");
    static_assert(std::is_unsigned_v<std::underlying_type_t<T>>, "T must have an unsigned underlying type");
    static constexpr size_t bit_count = sizeof(std::underlying_type_t<T>) * 8;
    using integer = std::underlying_type_t<T>;
public:
    constexpr enum_bitset() : bits(0) {
    }

    constexpr explicit enum_bitset(integer bits) : bits(bits) {
    }

    constexpr enum_bitset(std::initializer_list<T> init) : bits(0) {
        for (const T& value : init) {
            integer index = static_cast<integer>(value);
            ASSERT(index < bit_count, continue, "Index out of bounds in enum_bitset initializer list.");
            if constexpr (Kind == enum_value::indices) {
                bits |= (static_cast<integer>(1) << index);
            }
            else {
                bits |= index;
            }
        }
    }
    constexpr bool contains(T value) const {
        integer index = static_cast<integer>(value);
        if (index >= bit_count) [[unlikely]] {
            BUG("Index out of bounds in enum_bitset::contains.");
            return false;
        }
        if constexpr (Kind == enum_value::indices) {
            return (bits & (static_cast<integer>(1) << index)) != 0;
        }
        else {
            return (bits & static_cast<integer>(index)) != 0;
        }
    }

    constexpr void insert(T value) {
        integer index = static_cast<integer>(value);
        ASSERT(index < bit_count, return, "Index out of bounds in enum_bitset::insert.");
        if constexpr (Kind == enum_value::indices) {
            bits |= (static_cast<integer>(1) << index);
        }
        else {
            bits |= index;
        }
    }

    constexpr void remove(T value) {
        integer index = static_cast<integer>(value);
        ASSERT(index < bit_count, return, "Index out of bounds in enum_bitset::remove.");
        if constexpr (Kind == enum_value::indices) {
            bits &= ~(static_cast<integer>(1) << index);
        }
        else {
            bits &= ~index;
        }
    }

    constexpr void clear() {
        bits = 0;
    }

    constexpr integer copy_to_integer() const {
        return bits;
    }

    constexpr capped_array<T, bit_count> copy_to_capped_array() const {
        capped_array<T, bit_count> result;
        for (size_t i = 0; i < bit_count; ++i) {
            if (contains(static_cast<T>(i))) {
                result.append(static_cast<T>(i));
            }
        }
        return result;
    }

private:
    integer bits;
};

template<typename T, allocator_concept Allocator = heap_allocator>
class heap_pointer {
public:
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using const_pointer = const T*;
    using const_reference = const T&;

    heap_pointer() : allocator(), ptr(nullptr) {
    }

    explicit heap_pointer(const T& value) requires std::is_copy_constructible_v<T> : allocator(), ptr(nullptr) {
        ptr = reinterpret_cast<pointer>(allocator.malloc(alignof(T), sizeof(T)));
        ASSERT(ptr != nullptr, return, "Out of memory, cannot allocate heap_pointer.");
        new (ptr) T(value);
    }

    explicit heap_pointer(T&& value) requires std::is_move_constructible_v<T> : allocator(), ptr(nullptr) {
        ptr = reinterpret_cast<pointer>(allocator.malloc(alignof(T), sizeof(T)));
        ASSERT(ptr != nullptr, return, "Out of memory, cannot allocate heap_pointer.");
        new (ptr) T(std::move(value));
    }

    heap_pointer(const heap_pointer& other) requires std::is_copy_constructible_v<T> : allocator(), ptr(nullptr) {
        if (other.ptr != nullptr) {
            ptr = reinterpret_cast<pointer>(allocator.malloc(alignof(T), sizeof(T)));
            ASSERT(ptr != nullptr, return, "Out of memory, cannot allocate heap_pointer.");
            new (ptr) T(*other.ptr);
        }
        else {
            ptr = nullptr;
        }
    }

    heap_pointer(heap_pointer&& other) noexcept : allocator(), ptr(other.ptr) {
        other.ptr = nullptr;
    }

    ~heap_pointer() {
        allocator.free(ptr, alignof(T), sizeof(T));
        ptr = nullptr;
    }

    result default_construct() requires std::is_default_constructible_v<T> {
        if (ptr == nullptr) {
            ptr = reinterpret_cast<pointer>(allocator.malloc(alignof(T), sizeof(T)));
            ASSERT(ptr != nullptr, return result::failure, "Out of memory, cannot allocate heap_pointer.");
            new (ptr) T();
        }
        else {
            *ptr = T();
        }
        return result::success;
    }

    template<typename... Args>
    result construct(Args&&... args) {
        if (ptr == nullptr) {
            ptr = reinterpret_cast<pointer>(allocator.malloc(alignof(T), sizeof(T)));
            ASSERT(ptr != nullptr, return result::failure, "Out of memory, cannot allocate heap_pointer.");
            new (ptr) T(std::forward<Args>(args)...);
        }
        else {
            *ptr = T(std::forward<Args>(args)...);
        }
        return result::success;
    }

    void destroy() {
        if (ptr != nullptr) {
            std::destroy_at(ptr);
            allocator.free(ptr, alignof(T), sizeof(T));
            ptr = nullptr;
        }
    }

    pointer leak() {
        pointer old_ptr = ptr;
        ptr = nullptr;
        return old_ptr;
    }

    bool is_null() const {
        return ptr == nullptr;
    }

    bool is_not_null() const {
        return ptr != nullptr;
    }

    bool value_equals(const T& other) const {
        if (ptr == nullptr) {
            return false;
        }
        return *ptr == other;
    }

    reference lookup(reference& fallback) {
        if (ptr == nullptr) [[unlikely]] {
            BUG("Dereferencing null heap_pointer.");
            return fallback;
        }
        return *ptr;
    }

    const_reference lookup(const_reference fallback) const {
        if (ptr == nullptr) [[unlikely]] {
            BUG("Dereferencing null heap_pointer.");
            return fallback;
        }
        return *ptr;
    }

    reference lookup_unsafe() {
        DEBUG_ASSERT(ptr != nullptr, return *reinterpret_cast<pointer>(nullptr), "Dereferencing null heap_pointer.");
        return *ptr;
    }

    const_reference lookup_unsafe() const {
        DEBUG_ASSERT(ptr != nullptr, return *reinterpret_cast<pointer>(nullptr), "Dereferencing null heap_pointer.");
        return *ptr;
    }

    pointer get_data() {
        return ptr;
    }

    bool operator==(const heap_pointer& other) const {
        return ptr == other.ptr;
    }

    bool operator!=(const heap_pointer& other) const {
        return ptr != other.ptr;
    }

private:
    Allocator allocator;
    pointer ptr;
};
