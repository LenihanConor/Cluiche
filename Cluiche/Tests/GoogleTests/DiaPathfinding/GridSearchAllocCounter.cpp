// GridSearchAllocCounter.cpp - the single global operator new/delete replacement for the test binary.
#include "GridSearchAllocCounter.h"

#include <cstdlib>
#include <malloc.h>
#include <new>

namespace {
    thread_local int tArmed = 0;
    thread_local uint64_t tCount = 0;

    void* CountedAlloc(std::size_t size)
    {
        if (tArmed > 0) { ++tCount; }
        void* p = std::malloc(size ? size : 1);
        if (!p) { throw std::bad_alloc(); }
        return p;
    }
    void* CountedAlignedAlloc(std::size_t size, std::size_t align)
    {
        if (tArmed > 0) { ++tCount; }
        if (size == 0) size = 1;
        void* p = _aligned_malloc(size, align);
        if (!p) { throw std::bad_alloc(); }
        return p;
    }
}

namespace GridSearchTest {
    AllocCounter::AllocCounter() : mStart(tCount) { ++tArmed; }
    AllocCounter::~AllocCounter() { --tArmed; }
    uint64_t AllocCounter::Count() const { return tCount - mStart; }
}

void* operator new(std::size_t size) { return CountedAlloc(size); }
void* operator new[](std::size_t size) { return CountedAlloc(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { try { return CountedAlloc(size); } catch (...) { return nullptr; } }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { try { return CountedAlloc(size); } catch (...) { return nullptr; } }
void* operator new(std::size_t size, std::align_val_t a) { return CountedAlignedAlloc(size, static_cast<std::size_t>(a)); }
void* operator new[](std::size_t size, std::align_val_t a) { return CountedAlignedAlloc(size, static_cast<std::size_t>(a)); }

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { _aligned_free(p); }
