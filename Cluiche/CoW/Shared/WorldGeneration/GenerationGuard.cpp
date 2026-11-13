// S-1 Timeout and Failure Handling: the abort checks of a generation run. See GenerationGuard.h.
#include <Shared/WorldGeneration/GenerationGuard.h>

#include <chrono>
#include <cstdio>

namespace CoW {
namespace WorldGeneration {

namespace
{
    class SteadyClock final : public IGenerationClock
    {
    public:
        double NowSeconds() const override
        {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    };
}

const IGenerationClock& SteadyGenerationClock()
{
    static const SteadyClock clock;
    return clock;
}

GenerationGuard::GenerationGuard(float timeoutSeconds, const IGenerationClock* clock, IGenerationObserver* next)
    : mClock(clock != nullptr ? clock : &SteadyGenerationClock())
    , mNext(next)
    , mTimeoutSeconds(timeoutSeconds)
    , mStart(mClock->NowSeconds())
{
}

double GenerationGuard::ElapsedSeconds() const
{
    return mClock->NowSeconds() - mStart;
}

bool GenerationGuard::TimedOut() const
{
    return ElapsedSeconds() > static_cast<double>(mTimeoutSeconds);
}

bool GenerationGuard::ShouldAbort(GenerationOutcome& reason)
{
    if (mCancelRequested)
    {
        reason = GenerationOutcome::Cancelled;
        std::snprintf(mMessage, sizeof(mMessage), "CANCEL: World generation cancelled by user");
        return true;
    }
    if (TimedOut())
    {
        reason = GenerationOutcome::Timeout;
        std::snprintf(mMessage, sizeof(mMessage), "TIMEOUT: World generation exceeded %.1f seconds", static_cast<double>(mTimeoutSeconds));
        return true;
    }
    return false;
}

void GenerationGuard::OnAttemptFinished(const AttemptRecord& record, uint32_t maxAttempts)
{
    if (mNext != nullptr) { mNext->OnAttemptFinished(record, maxAttempts); }
}

} // namespace WorldGeneration
} // namespace CoW
