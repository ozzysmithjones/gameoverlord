#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <bit>
#include <type_traits>
#include <concepts>
#include <print>

// This is a big header file with a lot of fundamental utilities and data structures.
// It's probably a good idea to stick this in a precompiled header file, to improve compile times and use it in all source files.

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
#define BUG(...) std::print(__FILE__":" TOSTRING(__LINE__) " \"{}\" bug found.\n", __func__); std::print("" __VA_ARGS__); std::fflush(stdout); BREAKPOINT()
#else
#define BUG(...) ((void)0)
#endif

#define ASSERT(condition, fallback, ...) \
    if (!(condition)) [[unlikely]] { \
        BUG(__VA_ARGS__); \
        fallback; \
    }

#ifndef NDEBUG
#define ST_DEBUG_ASSERT(condition, fallback, ...) ASSERT(condition, fallback, __VA_ARGS__)
#else
#define ST_DEBUG_ASSERT(condition, fallback, ...) ((void)0)
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
    static inline void* malloc(size_t alignment, size_t bytes) {
        ASSERT(alignment <= alignof(std::max_align_t), return nullptr, "Requested alignment {} exceeds max supported alignment {} for default heap allocator.", alignment, alignof(std::max_align_t));
        return std::malloc(bytes);
    }

    static inline void* realloc(void* ptr, size_t alignment, size_t old_bytes, size_t new_bytes) {
        ASSERT(alignment <= alignof(std::max_align_t), return nullptr, "Requested alignment {} exceeds max supported alignment {} for default heap allocator.", alignment, alignof(std::max_align_t));
        if (new_bytes == 0) {
            std::free(ptr);
            return nullptr;
        }
        return std::realloc(ptr, new_bytes);
    }
    static inline void free(void* ptr, size_t alignment, size_t bytes) {
        ASSERT(alignment <= alignof(std::max_align_t), return, "Requested alignment {} exceeds max supported alignment {} for default heap allocator.", alignment, alignof(std::max_align_t));
        std::free(ptr);
    }
};

template<size_t Alignment, size_t Block_Size, size_t Block_Count>
struct block_allocator {
    block_allocator() : last_block(nullptr) {
        for (size_t i = 0; i < Block_Count; ++i) {
            blocks[i].prev = last_block;
            last_block = &blocks[i];
        }
    }

    void* malloc(size_t alignment, size_t bytes) {
        ASSERT(last_block != nullptr, return nullptr, "Out of memory in block_allocator.");
        ASSERT(alignment <= Alignment, return nullptr, "Requested alignment {} exceeds block allocator alignment {}", alignment, Alignment);
        ASSERT(bytes <= Block_Size, return nullptr, "Requested size {} exceeds block size {}", bytes, Block_Size);
        block* b = last_block;
        last_block = last_block->prev;
        return b->data;
    }

    void* realloc(void* ptr, size_t alignment, size_t old_bytes, size_t new_bytes) {
        if (ptr == nullptr) {
            return malloc(alignment, new_bytes);
        }

        if (new_bytes == 0) {
            free(ptr, alignment, old_bytes);
            return nullptr;
        }

        if (new_bytes <= old_bytes || new_bytes <= Block_Size) {
            return ptr;
        }

        BUG("Block allocator cannot grow allocations beyond block size.");
        return nullptr;
    }

    void free(void* ptr, size_t alignment, size_t bytes) {
        if (ptr == nullptr) {
            return;
        }

        block* b = reinterpret_cast<block*>(reinterpret_cast<std::byte*>(ptr));
        b->prev = last_block;
        last_block = b;

        ST_DEBUG_ASSERT(alignment <= Alignment, return, "Requested alignment {} exceeds block allocator alignment {}", alignment, Alignment);
        ST_DEBUG_ASSERT(bytes <= Block_Size, return, "Requested size {} exceeds block size {}", bytes, Block_Size);
    }

private:
    struct block {
        alignas(Alignment) std::byte data[Block_Size];
        block* prev = nullptr;
    };

    block* last_block = nullptr;
    block blocks[Block_Count];
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
    constexpr enum_array(std::initializer_list<T> init)  requires (std::is_default_constructible_v<T>) : elements{} {
        ASSERT(init.size() <= Capacity, return, "Initializer list size exceeds enum_array capacity.");
        size_t i = 0;
        for (const T& value : init) {
            elements[i] = value;
            ++i;
        }
    }

    constexpr result attempt_copy_element(Enum e, T& out_value) const {
        size_type index = static_cast<size_type>(e);
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, Capacity);
            return result::failure;
        }
        out_value = elements[index];
        return result::success;
    }

    constexpr result attempt_set_element(Enum e, const T& value) {
        size_type index = static_cast<size_type>(e);
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, Capacity);
            return result::failure;
        }
        elements[index] = value;
        return result::success;
    }

    constexpr const_reference element_or(Enum e, const_reference fallback) const {
        size_type index = static_cast<size_type>(e);
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, Capacity);
            return fallback;
        }
        return elements[index];
    }

    constexpr reference element_or(Enum e, reference fallback) {
        size_type index = static_cast<size_type>(e);
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, Capacity);
            return fallback;
        }
        return elements[index];
    }

    constexpr const_reference element_unsafe(Enum e) const {
        size_type index = static_cast<size_type>(e);
        ST_DEBUG_ASSERT(index < Capacity, return elements[0], "Index out of bounds (index={}, limit={})", index, Capacity);
        return elements[index];
    }

    constexpr reference element_unsafe(Enum e) {
        size_type index = static_cast<size_type>(e);
        ST_DEBUG_ASSERT(index < Capacity, return elements[0], "Index out of bounds (index={}, limit={})", index, Capacity);
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

    constexpr const_reference element_or(size_type index, const_reference fallback) const {
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, Capacity);
            return fallback;
        }
        return elements[index];
    }

    constexpr result attempt_copy_element(size_type index, T& out_value) const {
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, Capacity);
            return result::failure;
        }
        out_value = elements[index];
        return result::success;
    }

    constexpr result attempt_set_element(size_type index, const T& value) {
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, Capacity);
            return result::failure;
        }
        elements[index] = value;
        return result::success;
    }

    constexpr reference element_or(size_type index, reference fallback) {
        if (index >= Capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, Capacity);
            return fallback;
        }
        return elements[index];
    }

    constexpr const_reference element_unsafe(size_type index) const {
        ST_DEBUG_ASSERT(index < Capacity, return elements[0], "Index out of bounds (index={}, limit={})", index, Capacity);
        return elements[index];
    }

    constexpr reference element_unsafe(size_type index) {
        ST_DEBUG_ASSERT(index < Capacity, return elements[0], "Index out of bounds (index={}, limit={})", index, Capacity);
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

template<typename T, allocator_concept Allocator = heap_allocator>
class fixed_heap_array {
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

    fixed_heap_array() : allocator(), elements(nullptr), capacity(0) {
    }
    explicit fixed_heap_array(size_type capacity) requires (std::is_default_constructible_v<T>) : allocator(), elements(nullptr), capacity(0) {
        if (capacity == 0) {
            return;
        }

        elements = reinterpret_cast<pointer>(allocator.malloc(alignof(T), sizeof(T) * capacity));
        ASSERT(elements != nullptr, return, "Out of memory, cannot allocate fixed_heap_array.");
        this->capacity = capacity;
        for (size_type i = 0; i < capacity; ++i) {
            new (&elements[i]) T();
        }
    }

    explicit fixed_heap_array(std::initializer_list<T> init) : allocator(), elements(nullptr), capacity(0) {
        if (init.size() == 0) {
            return;
        }
        elements = reinterpret_cast<pointer>(allocator.malloc(alignof(T), sizeof(T) * init.size()));
        ASSERT(elements != nullptr, return, "Out of memory, cannot allocate fixed_heap_array.");
        this->capacity = init.size();
        size_type i = 0;
        for (const T& value : init) {
            new (&elements[i]) T(value);
            ++i;
        }
    }

    fixed_heap_array(const fixed_heap_array& other) : allocator(), elements(nullptr), capacity(0) {
        if (other.capacity == 0) {
            return;
        }
        elements = reinterpret_cast<pointer>(allocator.malloc(alignof(T), sizeof(T) * other.capacity));
        ASSERT(elements != nullptr, return, "Out of memory, cannot allocate fixed_heap_array.");
        this->capacity = other.capacity;
        for (size_type i = 0; i < other.capacity; ++i) {
            new (&elements[i]) T(other.elements[i]);
        }
    }

    fixed_heap_array(fixed_heap_array&& other) noexcept : allocator(), elements(other.elements), capacity(other.capacity) {
        other.elements = nullptr;
        other.capacity = 0;
    }

    ~fixed_heap_array() {
        if (elements == nullptr) {
            return;
        }
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = 0; i < capacity; ++i) {
                std::destroy_at(&elements[i]);
            }
        }
        allocator.free(elements, alignof(T), sizeof(T) * capacity);
        elements = nullptr;
        capacity = 0;
    }

    fixed_heap_array& operator=(const fixed_heap_array& other) {
        if (this == &other) {
            return *this;
        }
        if (elements != nullptr) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_type i = 0; i < capacity; ++i) {
                    std::destroy_at(&elements[i]);
                }
            }
            allocator.free(elements, alignof(T), sizeof(T) * capacity);
            elements = nullptr;
            capacity = 0;
        }
        if (other.capacity == 0) {
            return *this;
        }
        elements = reinterpret_cast<pointer>(allocator.malloc(alignof(T), sizeof(T) * other.capacity));
        ASSERT(elements != nullptr, return *this, "Out of memory, cannot allocate fixed_heap_array.");
        this->capacity = other.capacity;
        for (size_type i = 0; i < other.capacity; ++i) {
            new (&elements[i]) T(other.elements[i]);
        }
        return *this;
    }

    fixed_heap_array& operator=(fixed_heap_array&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (elements != nullptr) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_type i = 0; i < capacity; ++i) {
                    std::destroy_at(&elements[i]);
                }
            }
            allocator.free(elements, alignof(T), sizeof(T) * capacity);
            elements = nullptr;
            capacity = 0;
        }
        elements = other.elements;
        capacity = other.capacity;
        other.elements = nullptr;
        other.capacity = 0;
        return *this;
    }

    constexpr result attempt_copy_element(size_type index, T& out_value) const {
        if (index >= capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, capacity);
            return result::failure;
        }
        out_value = elements[index];
        return result::success;
    }

    constexpr result attempt_set_element(size_type index, const T& value) {
        if (index >= capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, capacity);
            return result::failure;
        }
        elements[index] = value;
        return result::success;
    }

    constexpr const_reference element_or(size_type index, const_reference fallback) const {
        if (index >= capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, capacity);
            return fallback;
        }
        return elements[index];
    }

    constexpr reference element_or(size_type index, reference fallback) {
        if (index >= capacity) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, capacity);
            return fallback;
        }
        return elements[index];
    }

    constexpr const_reference element_unsafe(size_type index) const {
        ST_DEBUG_ASSERT(index < capacity, return elements[0], "Index out of bounds (index={}, limit={})", index, capacity);
        return elements[index];
    }

    constexpr reference element_unsafe(size_type index) {
        ST_DEBUG_ASSERT(index < capacity, return elements[0], "Index out of bounds (index={}, limit={})", index, capacity);
        return elements[index];
    }

    constexpr bool bounds_check(size_type index) const {
        return index < capacity;
    }

    constexpr bool contains(const T& value) const {
        for (size_type i = 0; i < capacity; ++i) {
            if (elements[i] == value) {
                return true;
            }
        }
        return false;
    }

    constexpr void set_everything(const T& value) {
        for (size_type i = 0; i < capacity; ++i) {
            elements[i] = value;
        }
    }

    constexpr bool find(const T& value, size_type& out_index) const {
        for (size_type i = 0; i < capacity; ++i) {
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
        for (size_type i = 0; i < capacity; ++i) {
            if (predicate(elements[i])) {
                out_index = i;
                return true;
            }
        }
        return false;
    }

    pointer get_data() {
        return elements;
    }

    const_pointer get_data() const {
        return elements;
    }

    size_type get_capacity() const {
        return capacity;
    }

    constexpr iterator begin() {
        return elements;
    }

    constexpr iterator end() {
        return elements + capacity;
    }

    constexpr const_iterator begin() const {
        return elements;
    }

    constexpr const_iterator end() const {
        return elements + capacity;
    }

private:
    Allocator allocator;
    T* elements = nullptr;
    size_t capacity = 0;
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
        ASSERT(values != nullptr || num_values == 0, return result::failure, "values cannot be null if num_values is greater than 0.");
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
        ASSERT(index < count, return result::failure, "Index out of bounds (index={}, limit={})", index, count);
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
        ASSERT(index < count, return result::failure, "Index out of bounds (index={}, limit={})", index, count);
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

    result attempt_copy_element(size_type index, T& out_value) const {
        const_pointer elements = reinterpret_cast<const_pointer>(elements_data);
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return result::failure;
        }
        out_value = elements[index];
        return result::success;
    }

    result attempt_set_element(size_type index, const T& value) {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return result::failure;
        }
        elements[index] = value;
        return result::success;
    }

    const_reference element_or(size_type index, const_reference fallback) const {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return fallback;
        }
        return elements[index];
    }

    reference element_or(size_type index, reference fallback) {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return fallback;
        }
        return elements[index];
    }

    const_reference element_unsafe(size_type index) const {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        ST_DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds (index={}, limit={})", index, count);
        return elements[index];
    }

    reference element_unsafe(size_type index) {
        pointer elements = reinterpret_cast<pointer>(elements_data);
        ST_DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds (index={}, limit={})", index, count);
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
/// This means that if an element is memcpy'd to a different memory location without invoking any constructors or assignment-operators, that it should still work as expected. This is the case most of the time anyway unless you are doing really fancy things like classes with circular references to themselves.
/// Just in general do not assume that a pointer to an element in a dynamic_array will remain stable. Any time a dynamic array grows, it could move the elements somewhere else to increase its internal capacity. You must consider that an element might be relocated to a different address.
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
    result append(Arg&& value) requires (std::is_same_v<std::remove_cvref_t<Arg>, T>) {
        if (count == capacity) {
            size_type new_capacity = capacity == 0 ? 4 : (capacity << 1);
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
            if (new_capacity < 4) {
                new_capacity = 4;
            }
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
        ASSERT(index < count, return result::failure, "Index out of bounds (index={}, limit={})", index, count);
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
        ASSERT(index < count, return result::failure, "Index out of bounds (index={}, limit={})", index, count);
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

    result attempt_copy_element(size_type index, T& out_value) const {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return result::failure;
        }
        out_value = elements[index];
        return result::success;
    }

    result attempt_set_element(size_type index, const T& value) {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return result::failure;
        }
        elements[index] = value;
        return result::success;
    }

    const_reference element_or(size_type index, const_reference fallback) const {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return fallback;
        }
        return elements[index];
    }

    reference element_or(size_type index, reference fallback) {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return fallback;
        }
        return elements[index];
    }

    const_reference element_unsafe(size_type index) const {
        ST_DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds (index={}, limit={})", index, count);
        return elements[index];
    }

    reference element_unsafe(size_type index) {
        ST_DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds (index={}, limit={})", index, count);
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

    result attempt_copy_element(size_type index, T& out_value) const {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return result::failure;
        }
        out_value = elements[index];
        return result::success;
    }

    result attempt_set_element(size_type index, const T& value) {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return result::failure;
        }
        elements[index] = value;
        return result::success;
    }

    const_reference element_or(size_type index, const_reference fallback) const {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return fallback;
        }
        return elements[index];
    }

    reference element_or(size_type index, reference fallback) {
        if (index >= count) [[unlikely]] {
            BUG("Index out of bounds (index={}, limit={})", index, count);
            return fallback;
        }
        return elements[index];
    }

    const_reference element_unsafe(size_type index) const {
        ST_DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds (index={}, limit={})", index, count);
        return elements[index];
    }

    reference element_unsafe(size_type index) {
        ST_DEBUG_ASSERT(index < count, return elements[0], "Index out of bounds (index={}, limit={})", index, count);
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
            BUG("Index out of bounds (index={}, limit={})", index, bit_count);
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
        ASSERT(index < bit_count, return, "Index out of bounds (index={}, limit={})", index, bit_count);
        if constexpr (Kind == enum_value::indices) {
            bits |= (static_cast<integer>(1) << index);
        }
        else {
            bits |= index;
        }
    }

    constexpr void remove(T value) {
        integer index = static_cast<integer>(value);
        ASSERT(index < bit_count, return, "Index out of bounds (index={}, limit={})", index, bit_count);
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
        if constexpr (!std::is_trivially_destructible_v<T>) {
            if (ptr != nullptr) {
                std::destroy_at(ptr);
            }
        }
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

    reference value_or(reference& fallback) {
        if (ptr == nullptr) [[unlikely]] {
            BUG("Dereferencing null heap_pointer.");
            return fallback;
        }
        return *ptr;
    }

    const_reference value_or(const_reference fallback) const {
        if (ptr == nullptr) [[unlikely]] {
            BUG("Dereferencing null heap_pointer.");
            return fallback;
        }
        return *ptr;
    }

    reference value_unsafe() {
        ST_DEBUG_ASSERT(ptr != nullptr, return *reinterpret_cast<pointer>(nullptr), "Dereferencing null heap_pointer.");
        return *ptr;
    }

    const_reference value_unsafe() const {
        ST_DEBUG_ASSERT(ptr != nullptr, return *reinterpret_cast<pointer>(nullptr), "Dereferencing null heap_pointer.");
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

template<typename T>
class maybe {
private:
    alignas(T) std::byte value_data[sizeof(T)];
    bool has_value;
public:
    maybe() : has_value(false), value_data{ 0 } {
    }
    explicit maybe(const T& value) requires (std::is_copy_constructible_v<T>) : has_value(true) {
        new (&value_data) T(value);
    }
    explicit maybe(T&& value) noexcept requires (std::is_nothrow_constructible_v<T>) : has_value(true) {
        new (&value_data) T(std::move(value));
    }
    explicit maybe(const maybe& other) requires (std::is_copy_constructible_v<T>) : has_value(other.has_value) {
        if (other.has_value) {
            new (&value_data) T(other.value);
        }
    }
    explicit maybe(maybe&& other) noexcept requires (std::is_nothrow_constructible_v<T>) : has_value(other.has_value) {
        if (other.has_value) {
            new (&value_data) T(std::move(other.value));
            other.has_value = false;
        }
    }
    ~maybe() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            if (has_value) {
                T* value_ptr = reinterpret_cast<T*>(&value_data);
                std::destroy_at(value_ptr);
            }
        }
    }

    maybe& operator=(const maybe& other) {
        if (this != &other) {
            if (has_value && !other.has_value) {
                T* value_ptr = reinterpret_cast<T*>(&value_data);
                std::destroy_at(value_ptr);
                has_value = false;
            }
            else if (!has_value && other.has_value) {
                new (&value_data) T(other.value);
                has_value = true;
            }
            else if (has_value && other.has_value) {
                T* value_ptr = reinterpret_cast<T*>(&value_data);
                *value_ptr = other.value;
            }
        }
        return *this;
    }
    maybe& operator=(maybe&& other) noexcept {
        if (this != &other) {
            if (has_value && !other.has_value) {
                T* value_ptr = reinterpret_cast<T*>(&value_data);
                std::destroy_at(value_ptr);
                has_value = false;
            }
            else if (!has_value && other.has_value) {
                new (&value_data) T(std::move(other.value));
                has_value = true;
                other.has_value = false;
            }
            else if (has_value && other.has_value) {
                T* value_ptr = reinterpret_cast<T*>(&value_data);
                *value_ptr = std::move(other.value);
                other.has_value = false;
            }
        }
        return *this;
    }

    void set_value(const T& new_value) {
        if (has_value) {
            T* value_ptr = reinterpret_cast<T*>(&value_data);
            *value_ptr = new_value;
        }
        else {
            new (&value_data) T(new_value);
            has_value = true;
        }
    }

    result attempt_copy_value(T& out_value) const {
        if (!has_value) [[unlikely]] {
            BUG("Maybe type does not contain a value.");
            return result::failure;
        }
        out_value = *reinterpret_cast<const T*>(&value_data);
        return result::success;
    }

    result attempt_move_value(T& out_value) {
        if (!has_value) [[unlikely]] {
            BUG("Maybe type does not contain a value.");
            return result::failure;
        }
        out_value = std::move(*reinterpret_cast<T*>(&value_data));
        std::destroy_at(reinterpret_cast<T*>(&value_data));
        has_value = false;
        return result::success;
    }

    [[nodiscard]] const T& value_or(const T& fallback) const {
        if (!has_value) [[unlikely]] {
            BUG("Maybe type does not contain a value.");
            return fallback;
        }
        return *reinterpret_cast<const T*>(&value_data);
    }

    [[nodiscard]] T& value_or(T& fallback) {
        if (!has_value) [[unlikely]] {
            BUG("Maybe type does not contain a value.");
            return fallback;
        }
        return *reinterpret_cast<T*>(&value_data);
    }

    [[nodiscard]] T& value_unsafe() {
        ST_DEBUG_ASSERT(has_value, return *reinterpret_cast<T*>(&value_data), "Maybe type does not contain a value.");
        return *reinterpret_cast<T*>(&value_data);
    }

    [[nodiscard]] bool contains_value() const {
        return has_value;
    }

    template<typename Predicate>
        requires std::is_invocable_r_v<void, Predicate, T&>
    void if_valid_do(Predicate predicate) {
        if (has_value) {
            predicate(*reinterpret_cast<T*>(&value_data));
        }
    }

    template<typename FirstPredicate, typename SecondPredicate>
        requires std::is_invocable_r_v<void, FirstPredicate, T&>&& std::is_invocable_r_v<void, SecondPredicate>
    void if_valid_do_else(FirstPredicate predicate, SecondPredicate else_predicate) {
        if (has_value) {
            predicate(*reinterpret_cast<T*>(&value_data));
        }
        else {
            else_predicate();
        }
    }

    [[nodiscard]] std::byte* get_data() {
        return value_data;
    }

    [[nodiscard]] const std::byte* get_data() const {
        return value_data;
    }
};

template<size_t Alignment, size_t Size>
class raw_storage {
    template<typename T>
    T* as() {
        static_assert(alignof(T) <= Alignment, "T must have alignment less than or equal to Alignment");
        static_assert(sizeof(T) <= Size, "T must have size less than or equal to Size");
        return reinterpret_cast<T*>(data);
    }
private:
    alignas(Alignment) std::byte data[Size];
};
