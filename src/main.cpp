#include "pch.h"
import demo;

void* operator new[](size_t size) {
    void* ptr = _aligned_malloc(size, 16);
    assert(ptr);
    return ptr;
}

// Used by EASTL.
void* operator new[](
    size_t size,
    const char* /* name */,
    int /* flags */,
    unsigned /* debug_flags */,
    const char* /* file */,
    int /* line */
) {
    void* ptr = _aligned_malloc(size, 16);
    assert(ptr);
    return ptr;
}

// Used by EASTL.
void* operator new[](
    size_t size,
    size_t alignment,
    size_t alignment_offset,
    const char* /* name */,
    int /* flags */,
    unsigned /* debug_flags */,
    const char* /* file */,
    int /* line */
) {
    void* ptr = _aligned_offset_malloc(size, alignment, alignment_offset);
    assert(ptr);
    return ptr;
}

void operator delete[](void* ptr) {
    if (ptr) {
        _aligned_free(ptr);
    }
}

int main() {
    if (!demo::Run()) {
        return 1;
    }
    return 0;
}
