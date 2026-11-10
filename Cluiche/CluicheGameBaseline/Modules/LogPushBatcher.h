#pragma once

#include <cstdint>

namespace Cluiche { namespace AppFlow {

// Pure pacing logic for the debug panel's appendLogLines pushes (no UI, no ring access; the caller supplies the clock and
// the ring cursor/total). At most one push per interval, at most maxLines lines per push; when more are pending the newest
// maxLines are kept and the rest are reported as dropped so the caller can emit a single summary line.
//
// Why: each appendLogLines CallJSFunction costs tens of ms on Main. Pushing every frame made an over-budget Main tick log
// `pu.over_budget` every tick, which fed the ring, which forced another push next frame (a feedback loop).
class LogPushBatcher
{
public:
    struct Plan
    {
        bool     push        = false;
        uint64_t startCursor = 0;   // drain from here (cursor advanced past the dropped lines)
        unsigned take        = 0;   // lines to drain (<= maxLines)
        uint64_t dropped     = 0;   // lines skipped between the old cursor and startCursor
    };

    explicit LogPushBatcher(double minIntervalMs = 250.0, unsigned maxLines = 200)
        : mMinIntervalMs(minIntervalMs), mMaxLines(maxLines) {}

    Plan Evaluate(double nowMs, uint64_t cursor, uint64_t totalWritten) const;
    void OnPushed(double nowMs) { mLastPushMs = nowMs; mHasPushed = true; }
    void Reset() { mHasPushed = false; mLastPushMs = 0.0; }

private:
    double   mMinIntervalMs;
    unsigned mMaxLines;
    double   mLastPushMs = 0.0;
    bool     mHasPushed  = false;
};

} } // namespace Cluiche::AppFlow
