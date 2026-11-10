#pragma once
#include <DiaCellHistory/ISnapshotStore.h>
#include <cstdint>

namespace CoW {

class SaveLoadModule;

// CoW's Dia::CellHistory::ISnapshotStore: takes and restores a named game state through
// SaveLoadModule, in <base>/snapshots/<name>.sav (outside E1's slots).
// Owned by SaveLoadModule, which calls RunFrameBoundary() from its DoUpdate after the user's own
// save/load, so a snapshot never overwrites a pending cow.save / cow.load: it waits (up to
// kDeferLimitFrames boundaries) and then fails with a reason.
// One request at a time; a second request while one is queued is refused ("busy") at the next
// boundary, without disturbing the first. Every callback fires exactly once, on SimPU, from
// RunFrameBoundary. Same-PU only; no locking.
class SnapshotStoreAdapter : public Dia::CellHistory::ISnapshotStore
{
public:
    static constexpr uint32_t kDeferLimitFrames = 120;   // Open Design Question 1: revisit with Large-size save timings
    static constexpr uint32_t kMaxNameLength    = 63;

    explicit SnapshotStoreAdapter(SaveLoadModule& owner) : mOwner(owner) {}

    // Non-empty, <= 63 chars, [A-Za-z0-9_-] only, and a valid E1 slot id (so no reserved device names).
    static bool IsValidName(const char* name);

    void RequestTake   (const char* name, Dia::CellHistory::SnapshotCallback cb, void* user) override { Queue(Kind::Take, name, cb, user); }
    void RequestRestore(const char* name, Dia::CellHistory::SnapshotCallback cb, void* user) override { Queue(Kind::Restore, name, cb, user); }
    bool Exists(const char* name) const override;
    bool Delete(const char* name) override;
    const char* Folder() const override;

    bool HasPending() const { return mHasPending; }

    // Runs the queued request (or defers it) and fires callbacks. Called once per sim frame.
    void RunFrameBoundary();

    // Fires every queued callback once with ok = false (module stopping), so none is lost.
    void FailAll(const char* reason);

private:
    enum class Kind : uint8_t { Take, Restore };
    struct Request
    {
        Kind                                  kind = Kind::Take;
        char                                  name[kMaxNameLength + 1] = {};
        bool                                  nameValid = false;
        Dia::CellHistory::SnapshotCallback    cb = nullptr;
        void*                                 user = nullptr;
    };

    void Queue(Kind kind, const char* name, Dia::CellHistory::SnapshotCallback cb, void* user);
    static void Fire(const Request& r, bool ok, const char* message);

    SaveLoadModule& mOwner;
    Request         mPending;
    bool            mHasPending = false;
    uint32_t        mDeferredFrames = 0;
    Request         mRejected[4];        // busy refusals waiting for the next boundary
    uint32_t        mRejectedCount = 0;
};

} // namespace CoW
