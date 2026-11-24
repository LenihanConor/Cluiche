#pragma once
// GridSearchAllocCounter.h - test-local allocation counter (declaration only).
// The global operator new/delete replacement lives in GridSearchAllocCounter.cpp (exactly one TU).
// It counts only while at least one AllocCounter scope is alive; otherwise it delegates to malloc/free.

#include <cstdint>

namespace GridSearchTest {
    class AllocCounter {
    public:
        AllocCounter();
        ~AllocCounter();
        AllocCounter(const AllocCounter&) = delete;
        AllocCounter& operator=(const AllocCounter&) = delete;
        // Allocations (operator new calls) since this guard was constructed.
        uint64_t Count() const;
    private:
        uint64_t mStart;
    };
}
