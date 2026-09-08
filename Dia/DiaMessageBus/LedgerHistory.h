#pragma once
#ifdef DIA_DEBUG
#include <cstdint>
#include <memory>
#include <DiaMessageBus/LedgerSnapshot.h>

namespace Dia::MessageBus {

    // ==========================================================================
    // LedgerHistory
    //
    // Debug-only, fixed-size ring buffer retaining the last kLedgerCapacity
    // completed-tick LedgerSnapshots so a future visual debugger's History tab
    // can show message flow over time. Standalone type owned by
    // MessageBusModule (NOT embedded in Bus) — Bus stays focused purely on
    // routing and remains fully testable without any Debug-only members.
    //
    // Mirrors the head/count/wraparound ring pattern used by
    // Dia::Mailbox::Mailbox's TypedQueueDescriptor (see DiaMailbox/Mailbox.h):
    // fixed capacity, plus head + count indices for FIFO eviction.
    //
    // Storage note (deviation from the naive `LedgerSnapshot mSlots[3600]`
    // inline-array sketch): at ~1.3 KB per LedgerSnapshot, 3600 slots is
    // ~4.7 MB. An inline array of that size makes LedgerHistory itself ~4.7 MB
    // — and MessageBusModule composes one by value, so any stack instance of
    // MessageBusModule (the existing TestDiaMessageBusCore.cpp tests
    // stack-declare `TestableMessageBusModule module;`) would blow the
    // default ~1 MB thread stack. Instead the backing buffer is a single
    // fixed-size heap allocation made once in the constructor and freed in
    // the destructor — capacity never changes after that, and there is zero
    // heap activity on the Push/ForEachSnapshot hot path, satisfying "zero
    // heap allocation after LedgerHistory construction" (AC-6) without the
    // stack-overflow hazard of embedding the array inline.
    //
    // Compiles out of Release entirely (wrapped #ifdef DIA_DEBUG) — this is
    // diagnostic-only storage, not gameplay-relevant.
    // ==========================================================================
    class LedgerHistory {
    public:
        static constexpr uint32_t kLedgerCapacity = 3600;

        LedgerHistory() : mSlots(std::make_unique<LedgerSnapshot[]>(kLedgerCapacity)) {}

        // Pushes one completed-tick snapshot. Once the ring is at capacity,
        // the oldest snapshot is evicted (FIFO) to make room.
        void Push(const LedgerSnapshot& snapshot);

        // Number of snapshots currently retained, in [0, kLedgerCapacity].
        uint32_t Count() const;

        // Visits every retained snapshot oldest-first (ascending tickIndex).
        // No copy, no scratch buffer — walks the ring directly.
        template <class Fn>
        void ForEachSnapshot(Fn&& fn) const {
            for (uint32_t i = 0; i < mCount; ++i) {
                const uint32_t idx = (mHead + i) % kLedgerCapacity;
                fn(mSlots[idx]);
            }
        }

        // AC-4: convenience filter over ForEachSnapshot — visits only
        // snapshots where droppedCount > 0, still oldest-first. A dedicated
        // method rather than pushing the filter onto every call site, since
        // "which ticks dropped messages" is a first-class question for the
        // History tab.
        template <class Fn>
        void ForEachDroppedSnapshot(Fn&& fn) const {
            ForEachSnapshot([&fn](const LedgerSnapshot& snapshot) {
                if (snapshot.droppedCount > 0) {
                    fn(snapshot);
                }
            });
        }

    private:
        // Single fixed-capacity heap allocation, made once at construction —
        // see the class comment above for why this isn't an inline C array.
        std::unique_ptr<LedgerSnapshot[]> mSlots;
        uint32_t       mHead  = 0;
        uint32_t       mCount = 0;
    };

} // namespace Dia::MessageBus
#endif // DIA_DEBUG
