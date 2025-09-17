// #include "fundamental.hpp" <- precompiled header
#include <cstdlib>


/*
void* heap_allocator::malloc(size_t alignment, size_t bytes) {
    ASSERT(alignment <= alignof(std::max_align_t), return nullptr, "Alignment greater than alignof(std::max_align_t) is not supported by the default heap allocator.");
    void* ptr = std::malloc(bytes);
    ASSERT(ptr != nullptr, return nullptr, "Out of memory, cannot allocate {} bytes.", bytes);
    return ptr;
}

void* heap_allocator::realloc(void* ptr, size_t alignment, size_t old_bytes, size_t new_bytes) {
    ASSERT(alignment <= alignof(std::max_align_t), return nullptr, "Alignment greater than alignof(std::max_align_t) is not supported by the default heap allocator.");
    if (new_bytes == 0) {
        std::free(ptr);
        return nullptr;
    }

    void* new_ptr = std::realloc(ptr, new_bytes);
    ASSERT(new_ptr != nullptr, return nullptr, "Out of memory, cannot reallocate {} bytes.", new_bytes);
    return new_ptr;
}

void heap_allocator::free(void* ptr, size_t alignment, size_t bytes) {
    ASSERT(alignment <= alignof(std::max_align_t), return, "Alignment greater than alignof(std::max_align_t) is not supported by the default heap allocator.");
    std::free(ptr);
}
*/