#pragma once
////////////////////////////////////////////////////////////////////////////////
// SimAnnounceTracker.h
//
// Main-side pure state machine for the Sim-Hosted announcement protocol (see
// SimRegistrationMessage.h).  It decides what to do with each parsed message;
// the caller (DebugServerHostModule) performs the action.  No streams, no
// DebugServer, no clock: time is passed in as nowMs.  Main-thread only.
//
// Rules:
//  - register/unregister with seq == lastSeq + 1 (uint32 wrap) -> Apply.
//  - while out of sync even a consecutive message re-requests once the window has elapsed.
//  - any other seq (gap, duplicate, reorder) -> announceGaps++, inSync=false,
//    the message is still applied and lastSeq = seq (newer info); a resync
//    request is returned at most once per kSimResyncRequestIntervalMs.
//  - digest: caller passes Main's own (count, hash).  Mismatch if
//    digest.seq != lastSeq or count/hash differ -> digestMismatches++,
//    inSync=false, RequestResync (rate limited).  A match changes nothing.
//  - resync_begin opens staging; Replay messages fill it; when the number of
//    DISTINCT (name, kind) pairs reaches the expected count the action is
//    InstallSet (caller swaps its live table for the staged list), then
//    lastSeq = begin.seq, inSync = true.  Duplicate replays of the same
//    (name, kind) are not counted.  Extra replays after completion, or with no
//    staging, are ignored.  count==0 installs an empty set at once.
//    count > kSimAnnounceMaxStaged is rejected (None, nothing changes).
//    A second resync_begin restarts staging.
//  - Staging older than kSimResyncTimeoutMs is discarded by Tick() (or by a
//    late Replay): DiscardStaging, resyncTimeouts++.  Live state is untouched.
//  - Completing a resync clears the request rate limit so a fresh fault is
//    reported immediately.  If the caller fails to post a requested resync it
//    calls NotifyRequestFailed(), which clears the limit so the next trigger
//    retries at once.
//  - Announcements that arrive while staging is active are handled normally
//    (applied to the live table); the install then replaces the table with the
//    snapshot, and any later loss is caught by the next digest.
////////////////////////////////////////////////////////////////////////////////

#include <Types/SimRegistrationMessage.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Cluiche { namespace AppFlow {

static constexpr uint64_t kSimResyncRequestIntervalMs = 500;
static constexpr uint64_t kSimResyncTimeoutMs         = 1000;
static constexpr uint32_t kSimAnnounceMaxStaged       = 4096;

enum class SimAnnounceAction : uint32_t
{
    None = 0,
    Apply,                  // apply the register/unregister to the live table
    ApplyAndRequestResync,  // apply it, and post a resync request
    RequestResync,          // post a resync request
    InstallSet,             // swap the live table with the staged list
    DiscardStaging          // staging timed out; live table untouched
};

class SimAnnounceTracker
{
public:
    SimAnnounceAction OnRegistration(const SimRegistration& msg, uint64_t nowMs)
    {
        if (msg.op != SimRegistrationOp::Register && msg.op != SimRegistrationOp::Unregister)
            return SimAnnounceAction::None;

        const bool consecutive = (msg.seq == mLastSeq + 1u);
        mLastSeq = msg.seq;
        if (consecutive && mInSync)
            return SimAnnounceAction::Apply;

        if (!consecutive)
        {
            ++mAnnounceGaps;
            mInSync = false;
        }
        // Out of sync (new gap, or still waiting for a resync): re-request once per window.
        return TryRequest(nowMs) ? SimAnnounceAction::ApplyAndRequestResync : SimAnnounceAction::Apply;
    }

    SimAnnounceAction OnDigest(const SimRegistration& msg, uint32_t localCount, uint32_t localHash, uint64_t nowMs)
    {
        if (msg.op != SimRegistrationOp::Digest)
            return SimAnnounceAction::None;
        if (msg.seq == mLastSeq && msg.count == localCount && msg.hash == localHash)
            return SimAnnounceAction::None;

        ++mDigestMismatches;
        mInSync = false;
        return TryRequest(nowMs) ? SimAnnounceAction::RequestResync : SimAnnounceAction::None;
    }

    SimAnnounceAction OnResyncBegin(const SimRegistration& msg, uint64_t nowMs)
    {
        if (msg.op != SimRegistrationOp::ResyncBegin || msg.count > kSimAnnounceMaxStaged)
            return SimAnnounceAction::None;

        mStaged.clear();
        mStagingActive = true;
        mStagingSeq    = msg.seq;
        mExpected      = msg.count;
        mStartedMs     = nowMs;
        if (mExpected == 0)
            return Complete();
        return SimAnnounceAction::None;
    }

    SimAnnounceAction OnReplay(const SimRegistration& msg, uint64_t nowMs)
    {
        if (msg.op != SimRegistrationOp::Replay || !mStagingActive)
            return SimAnnounceAction::None;
        if (Expired(nowMs))
            return Discard();

        for (const Entry& e : mStaged)
        {
            if (e.kind == msg.kind && e.name == msg.name)
                return SimAnnounceAction::None;   // duplicate: not counted
        }
        mStaged.push_back(Entry{ std::string(msg.name), msg.kind });
        if (mStaged.size() >= mExpected)
            return Complete();
        return SimAnnounceAction::None;
    }

    SimAnnounceAction Tick(uint64_t nowMs)
    {
        if (mStagingActive && Expired(nowMs))
            return Discard();
        return SimAnnounceAction::None;
    }

    // The caller could not post the resync request: allow an immediate retry.
    void NotifyRequestFailed()
    {
        mHasRequested = false;
        if (mResyncsRequested > 0)
            --mResyncsRequested;
    }

    uint32_t LastSeq() const { return mLastSeq; }
    bool     InSync() const  { return mInSync; }
    bool     Staging() const { return mStagingActive; }

    // Staged list: valid after InstallSet (until the next OnResyncBegin) and while staging.
    uint32_t                  StagedCount() const { return static_cast<uint32_t>(mStaged.size()); }
    const char*               StagedName(uint32_t i) const { return mStaged[i].name.c_str(); }
    Dia::DebugServer::SimKind StagedKind(uint32_t i) const { return mStaged[i].kind; }

    uint32_t AnnounceGaps() const      { return mAnnounceGaps; }
    uint32_t DigestMismatches() const  { return mDigestMismatches; }
    uint32_t ResyncsRequested() const  { return mResyncsRequested; }
    uint32_t ResyncsCompleted() const  { return mResyncsCompleted; }
    uint32_t ResyncTimeouts() const    { return mResyncTimeouts; }

    void SeedLastSeq(uint32_t seq) { mLastSeq = seq; }

private:
    struct Entry
    {
        std::string               name;
        Dia::DebugServer::SimKind kind;
    };

    bool TryRequest(uint64_t nowMs)
    {
        if (mHasRequested && nowMs - mLastRequestMs < kSimResyncRequestIntervalMs)
            return false;
        mHasRequested   = true;
        mLastRequestMs  = nowMs;
        ++mResyncsRequested;
        return true;
    }

    bool Expired(uint64_t nowMs) const
    {
        return nowMs >= mStartedMs && nowMs - mStartedMs >= kSimResyncTimeoutMs;
    }

    SimAnnounceAction Discard()
    {
        mStaged.clear();
        mStagingActive = false;
        ++mResyncTimeouts;
        return SimAnnounceAction::DiscardStaging;
    }

    SimAnnounceAction Complete()
    {
        mStagingActive = false;
        mLastSeq       = mStagingSeq;
        mInSync        = true;
        mHasRequested  = false;
        ++mResyncsCompleted;
        return SimAnnounceAction::InstallSet;
    }

    uint32_t mLastSeq = 0;
    bool     mInSync  = true;

    bool     mHasRequested  = false;
    uint64_t mLastRequestMs = 0;

    bool               mStagingActive = false;
    uint32_t           mStagingSeq    = 0;
    uint32_t           mExpected      = 0;
    uint64_t           mStartedMs     = 0;
    std::vector<Entry> mStaged;

    uint32_t mAnnounceGaps     = 0;
    uint32_t mDigestMismatches = 0;
    uint32_t mResyncsRequested = 0;
    uint32_t mResyncsCompleted = 0;
    uint32_t mResyncTimeouts   = 0;
};

} } // namespace Cluiche::AppFlow
