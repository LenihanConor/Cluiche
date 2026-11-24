#pragma once

#include <atomic>
#include <stdint.h>

namespace CoW {
namespace WorldStep {

// Hands one background spin-up job's result to SimPU.
// Threading: one worker thread (IsCancelled/Complete) + one SimPU consumer
// (TryBegin/Cancel/TryTake/Reset).
//
// Policy: TryBegin(newRevision) while Running cancels the old job and begins
// the new revision in place (returns true). The old worker detects this via
// IsCancelled(oldRevision) and its late Complete(sim, oldRevision) is rejected,
// so the worker keeps ownership of (and must delete) the discarded sim.
// TryBegin returns false while a finished result is unconsumed (Done) or being
// published; take or Reset first.
template <class TSim>
class SpinUpSlot
{
public:
    enum : uint8_t
    {
        kIdle = 0,
        kRunning = 1,
        kDone = 2,
        kCancelled = 3,
        kPublishing = 4,  // transient: worker is storing its result
    };

    SpinUpSlot()
        : mWord(Pack(kIdle, 0))
        , mResult(nullptr)
    {
    }

    bool TryBegin(uint32_t revision)
    {
        uint64_t w = mWord.load(std::memory_order_acquire);
        for (;;)
        {
            const uint8_t s = StateOf(w);
            if (s == kDone || s == kPublishing)
            {
                return false;
            }
            // Idle, Cancelled, or Running (old job superseded). One CAS covers state and
            // revision, so a racing Complete(sim, oldRevision) either wins first or fails.
            if (mWord.compare_exchange_weak(w, Pack(kRunning, revision), std::memory_order_acq_rel))
            {
                return true;
            }
        }
    }

    void Cancel()
    {
        uint64_t w = mWord.load(std::memory_order_acquire);
        while (StateOf(w) == kRunning)
        {
            if (mWord.compare_exchange_weak(w, Pack(kCancelled, RevisionOf(w)), std::memory_order_acq_rel))
            {
                return;
            }
        }
    }

    bool IsCancelled() const { return StateOf(mWord.load(std::memory_order_acquire)) == kCancelled; }

    // Worker form: also true when a newer revision has superseded this job.
    bool IsCancelled(uint32_t revision) const
    {
        const uint64_t w = mWord.load(std::memory_order_acquire);
        return StateOf(w) == kCancelled || RevisionOf(w) != revision;
    }

    // Returns true if accepted (Running -> Done); false if cancelled/stale,
    // in which case the caller still owns sim.
    bool Complete(TSim* sim) { return Complete(sim, Revision()); }

    bool Complete(TSim* sim, uint32_t revision)
    {
        uint64_t expected = Pack(kRunning, revision);
        if (!mWord.compare_exchange_strong(expected, Pack(kPublishing, revision), std::memory_order_acq_rel))
        {
            return false;
        }
        mResult.store(sim, std::memory_order_relaxed);
        mWord.store(Pack(kDone, revision), std::memory_order_release);
        return true;
    }

    bool TryTake(TSim*& sim, uint32_t& revision)
    {
        const uint64_t w = mWord.load(std::memory_order_acquire);
        if (StateOf(w) != kDone)
        {
            return false;
        }
        sim = mResult.load(std::memory_order_relaxed);
        revision = RevisionOf(w);
        mResult.store(nullptr, std::memory_order_relaxed);
        mWord.store(Pack(kIdle, revision), std::memory_order_release);
        return true;
    }

    void Reset()
    {
        mResult.store(nullptr, std::memory_order_relaxed);
        mWord.store(Pack(kIdle, Revision()), std::memory_order_release);
    }

    uint8_t State() const { return StateOf(mWord.load(std::memory_order_acquire)); }
    uint32_t Revision() const { return RevisionOf(mWord.load(std::memory_order_acquire)); }

private:
    static uint64_t Pack(uint8_t state, uint32_t revision) { return (static_cast<uint64_t>(revision) << 8) | state; }
    static uint8_t StateOf(uint64_t w) { return static_cast<uint8_t>(w & 0xFF); }
    static uint32_t RevisionOf(uint64_t w) { return static_cast<uint32_t>(w >> 8); }

    std::atomic<uint64_t> mWord;  // revision << 8 | state
    std::atomic<TSim*> mResult;
};

}  // namespace WorldStep
}  // namespace CoW
