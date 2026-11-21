#include "Modules/TestResultChangeTracker.h"

namespace CluicheTest {

bool TestResultChangeTracker::MapState(StageResult::State state, uint64_t utcMs, Cluiche::AppFlow::StageResult& out)
{
    using Severity = Cluiche::AppFlow::StageResultSeverity;
    switch (state)
    {
    case StageResult::State::kPassed:  out.severity = Severity::kOk;   out.SetLabel("PASS");    break;
    case StageResult::State::kFailed:  out.severity = Severity::kFail; out.SetLabel("FAIL");    break;
    case StageResult::State::kTimeout: out.severity = Severity::kFail; out.SetLabel("TIMEOUT"); break;
    case StageResult::State::kRunning: out.severity = Severity::kNone; out.SetLabel("RUNNING"); break;
    default: return false;
    }
    out.utcMs = utcMs;
    return true;
}

unsigned int TestResultChangeTracker::Poll(const TestResultsRegistry& registry, uint64_t utcMs,
                                           Cluiche::AppFlow::StageResultEvent* outEvents, unsigned int maxEvents)
{
    unsigned int written = 0;
    const unsigned int count = registry.GetResultCount();
    for (unsigned int i = 0; i < count && written < maxEvents; ++i)
    {
        const StageResult& result = registry.GetResultAt(i);
        if (result.state == StageResult::State::kNotRun)
            continue;

        Tracked* tracked = nullptr;
        for (unsigned int t = 0; t < mTracked.Size(); ++t)
        {
            if (mTracked[t].stage == result.name) { tracked = &mTracked[t]; break; }
        }
        if (tracked == nullptr)
        {
            if (mTracked.Size() >= kMaxTracked)
                continue;  // bounded storage; the registry itself is capped lower
            Tracked fresh;
            fresh.stage = result.name;
            mTracked.Add(fresh);
            tracked = &mTracked[mTracked.Size() - 1];
        }
        if (tracked->state == result.state)
            continue;

        Cluiche::AppFlow::StageResultEvent ev;
        if (!MapState(result.state, utcMs, ev.result))
            continue;
        ev.stage = result.name;
        outEvents[written++] = ev;
        tracked->state = result.state;
    }
    return written;
}

} // namespace CluicheTest
