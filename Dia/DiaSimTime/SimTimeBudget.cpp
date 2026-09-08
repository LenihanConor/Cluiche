#include <DiaSimTime/SimTimeBudget.h>

#include <DiaSimTime/ISimTimeBudgetedSystem.h>
#include <DiaSimTime/IOneShotWork.h>

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>

#include <chrono>

namespace Dia::SimTime {

    // --- helpers -------------------------------------------------------------

    int SimTimeBudget::TierIndex(SimTimePriority tier)
    {
        // SimTimePriority enumerators are declared kCritical..kBackground in the
        // 0..3 order this indexes into (see SimTimePriority.h).
        return static_cast<int>(tier);
    }

    double SimTimeBudget::NowMs()
    {
        // Same wall-clock technique as AIBudgetScheduler::Update — steady_clock,
        // reported in fractional milliseconds. ST-010: gating is wall-clock, not
        // game-time.
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    // --- ctor ----------------------------------------------------------------

    SimTimeBudget::SimTimeBudget()
    {
        mTierBudgetMs[TierIndex(SimTimePriority::kCritical)]   = kDefaultCriticalMs;
        mTierBudgetMs[TierIndex(SimTimePriority::kHigh)]       = kDefaultHighMs;
        mTierBudgetMs[TierIndex(SimTimePriority::kNormal)]     = kDefaultNormalMs;
        mTierBudgetMs[TierIndex(SimTimePriority::kBackground)] = kDefaultBackgroundMs;

        mTierDeadlineMs[TierIndex(SimTimePriority::kCritical)]   = kDefaultCriticalDeadlineMs;
        mTierDeadlineMs[TierIndex(SimTimePriority::kHigh)]       = kDefaultHighDeadlineMs;
        mTierDeadlineMs[TierIndex(SimTimePriority::kNormal)]     = kDefaultNormalDeadlineMs;
        mTierDeadlineMs[TierIndex(SimTimePriority::kBackground)] = kDefaultBackgroundDeadlineMs;

        for (int i = 0; i < kTierCount; ++i)
            mTierRemainingMs[i] = 0.0f;

        auto& reg            = Dia::Observation::Metric::MetricRegistry::Instance();
        mMetricUsedMs        = reg.RegisterGauge  (Dia::Core::StringCRC("simtime.budget.used_ms"));
        mMetricDeferredCount = reg.RegisterCounter(Dia::Core::StringCRC("simtime.budget.deferred_count"));
        mMetricStaleMsMax    = reg.RegisterGauge  (Dia::Core::StringCRC("simtime.budget.stale_ms_max"));
    }

    // --- lookup --------------------------------------------------------------

    const SimTimeBudget::Entry* SimTimeBudget::FindEntry(const ISimTimeBudgetedSystem* system) const
    {
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            const Entry& e = mEntries.At(i);
            if (e.system == system)
                return &e;
        }
        return nullptr;
    }

    SimTimeBudget::Entry* SimTimeBudget::FindEntry(const ISimTimeBudgetedSystem* system)
    {
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            Entry& e = mEntries.At(i);
            if (e.system == system)
                return &e;
        }
        return nullptr;
    }

    // --- registration --------------------------------------------------------

    void SimTimeBudget::Register(ISimTimeBudgetedSystem* system)
    {
        DIA_ASSERT(system != nullptr, "SimTimeBudget::Register — null system");
        if (system == nullptr)
            return;

        DIA_ASSERT(FindEntry(system) == nullptr,
            "SimTimeBudget::Register — system '%s' is already registered",
            system->GetSystemId().AsChar());
        if (FindEntry(system) != nullptr)
            return;

        DIA_ASSERT(!mEntries.IsFull(),
            "SimTimeBudget::Register — at capacity (%d systems)", kMaxSystems);
        if (mEntries.IsFull())
        {
            DIA_LOG_ERROR("SimTime", "SimTimeBudget::Register failed: at capacity (%d)", kMaxSystems);
            return;
        }

        mEntries.AddDefault();
        Entry& entry          = mEntries.Back();
        entry.systemId        = system->GetSystemId();
        entry.system          = system;
        entry.priority        = system->GetPriority();
        // Staleness clock starts at registration: a fresh system is not instantly
        // "stale" (so it is not spuriously promoted), but is immediately eligible
        // to run via the budget path if its tier has capacity.
        entry.lastRanSteadyMs = NowMs();
    }

    void SimTimeBudget::Unregister(ISimTimeBudgetedSystem* system)
    {
        if (system == nullptr)
            return;
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            if (mEntries.At(i).system == system)
            {
                mEntries.RemoveAt(i);
                return;
            }
        }
    }

    // --- configuration -------------------------------------------------------

    void SimTimeBudget::SetTierBudgets(float criticalMs, float highMs, float normalMs, float backgroundMs)
    {
        auto clamp0 = [](float v) { return v < 0.0f ? 0.0f : v; };
        mTierBudgetMs[TierIndex(SimTimePriority::kCritical)]   = clamp0(criticalMs);
        mTierBudgetMs[TierIndex(SimTimePriority::kHigh)]       = clamp0(highMs);
        mTierBudgetMs[TierIndex(SimTimePriority::kNormal)]     = clamp0(normalMs);
        mTierBudgetMs[TierIndex(SimTimePriority::kBackground)] = clamp0(backgroundMs);
    }

    void SimTimeBudget::SetTierDeadlines(float criticalMs, float highMs, float normalMs, float backgroundMs)
    {
        auto clamp0 = [](float v) { return v < 0.0f ? 0.0f : v; };
        mTierDeadlineMs[TierIndex(SimTimePriority::kCritical)]   = clamp0(criticalMs);
        mTierDeadlineMs[TierIndex(SimTimePriority::kHigh)]       = clamp0(highMs);
        mTierDeadlineMs[TierIndex(SimTimePriority::kNormal)]     = clamp0(normalMs);
        mTierDeadlineMs[TierIndex(SimTimePriority::kBackground)] = clamp0(backgroundMs);
    }

    // --- per-tick allocation -------------------------------------------------

    void SimTimeBudget::AllocateTick(const SimTimeContext& /*ctx*/)
    {
        // ST-010: budget bookkeeping is wall-clock based, so no field of the
        // SimTimeContext is consumed. The parameter is accepted purely to match
        // Pattern C's call signature (Task 4.4).
        for (int i = 0; i < kTierCount; ++i)
            mTierRemainingMs[i] = mTierBudgetMs[i];

        mLastUsedMs        = 0.0f;
        mLastDeferredCount = 0;
        mLastPromotedCount = 0;
        mLastStaleMsMax    = 0.0f;

        if (mMetricUsedMs)     mMetricUsedMs->Set(0.0);
        if (mMetricStaleMsMax) mMetricStaleMsMax->Set(0.0);
    }

    // --- run helper ----------------------------------------------------------

    float SimTimeBudget::RunSystem(Entry& entry, float allowanceMs)
    {
        const auto t0 = std::chrono::steady_clock::now();
        entry.system->UpdateBudgeted(allowanceMs);
        const auto t1 = std::chrono::steady_clock::now();

        const float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

        entry.lastRanSteadyMs = NowMs();
        mLastUsedMs += elapsedMs;
        if (mMetricUsedMs) mMetricUsedMs->Set(static_cast<double>(mLastUsedMs));
        return elapsedMs;
    }

    // --- per-entry budget gate -----------------------------------------------

    bool SimTimeBudget::RunIfCapacity(ISimTimeBudgetedSystem* system)
    {
        Entry* entry = FindEntry(system);
        if (entry == nullptr)
            return false;   // unknown / unregistered: safe no-op

        const int tier = TierIndex(entry->priority);

        // Budget path: the tier still has capacity this tick.
        if (mTierRemainingMs[tier] > 0.0f)
        {
            const float allowance = mTierRemainingMs[tier];
            const float elapsedMs = RunSystem(*entry, allowance);

            mTierRemainingMs[tier] -= elapsedMs;
            if (mTierRemainingMs[tier] < 0.0f)
                mTierRemainingMs[tier] = 0.0f;   // capped at zero, never negative
            return true;
        }

        // Tier budget exhausted — consider deadline promotion (ST-008).
        const float stalenessMs = static_cast<float>(NowMs() - entry->lastRanSteadyMs);

        if (stalenessMs > mTierDeadlineMs[tier])
        {
            // Promote: force-run with the tier's configured per-tick allowance
            // ("carry-forward"), regardless of the empty pool. A promoted system
            // DID run (just late) — it is not counted as deferred.
            RunSystem(*entry, mTierBudgetMs[tier]);
            ++mLastPromotedCount;
            DIA_LOG_DEBUG("SimTime",
                "simtime.budget promote system=%s staleness_ms=%.3f deadline_ms=%.3f",
                entry->systemId.AsChar(), stalenessMs, mTierDeadlineMs[tier]);
            return true;
        }

        // Genuinely deferred this tick.
        ++mLastDeferredCount;
        if (mMetricDeferredCount) mMetricDeferredCount->Inc();
        if (stalenessMs > mLastStaleMsMax)
        {
            mLastStaleMsMax = stalenessMs;
            if (mMetricStaleMsMax) mMetricStaleMsMax->Set(static_cast<double>(mLastStaleMsMax));
        }
        return false;
    }

    // --- one-shot completion path (ST-012) -----------------------------------

    void SimTimeBudget::SubmitOneShot(IOneShotWork* work)
    {
        DIA_ASSERT(work != nullptr, "SimTimeBudget::SubmitOneShot — null work");
        if (work == nullptr)
            return;
        DIA_ASSERT(!mOneShots.IsFull(),
            "SimTimeBudget::SubmitOneShot — one-shot queue at capacity (%d)", kMaxOneShots);
        if (mOneShots.IsFull())
        {
            DIA_LOG_ERROR("SimTime", "SimTimeBudget::SubmitOneShot dropped: queue full (%d)", kMaxOneShots);
            return;
        }
        mOneShots.Add(work);
    }

    void SimTimeBudget::RunOneShots(float budgetMs)
    {
        // Drains in submission order until the queue is empty or the slice is
        // exhausted. Completed items (Step() == true) are removed; the rest stay
        // queued. Independent of the four priority-tier pools.
        float remainingMs = budgetMs < 0.0f ? 0.0f : budgetMs;

        unsigned int i = 0;
        while (i < mOneShots.Size())
        {
            if (remainingMs <= 0.0f)
                break;   // slice exhausted — leave the rest queued for next call

            IOneShotWork* work = mOneShots.At(i);

            const auto t0   = std::chrono::steady_clock::now();
            const bool done = work->Step(remainingMs);
            const auto t1   = std::chrono::steady_clock::now();

            remainingMs -= std::chrono::duration<float, std::milli>(t1 - t0).count();
            if (remainingMs < 0.0f)
                remainingMs = 0.0f;

            if (done)
            {
                mOneShots.RemoveAt(i);   // completed — remove, do not advance i
            }
            else
            {
                ++i;                     // stays queued for a later call
            }
        }
    }

    // --- introspection -------------------------------------------------------

    int SimTimeBudget::GetRegisteredCount() const
    {
        return static_cast<int>(mEntries.Size());
    }

    int SimTimeBudget::GetPendingOneShotCount() const
    {
        return static_cast<int>(mOneShots.Size());
    }

    float SimTimeBudget::GetLastUsedMs() const        { return mLastUsedMs; }
    int   SimTimeBudget::GetLastDeferredCount() const { return mLastDeferredCount; }
    int   SimTimeBudget::GetLastPromotedCount() const { return mLastPromotedCount; }
    float SimTimeBudget::GetLastStaleMsMax() const    { return mLastStaleMsMax; }

    unsigned long long SimTimeBudget::GetDeferredCountTotal() const
    {
        return mMetricDeferredCount
            ? static_cast<unsigned long long>(mMetricDeferredCount->Value())
            : 0ull;
    }

    float SimTimeBudget::GetTierRemainingMs(SimTimePriority tier) const
    {
        return mTierRemainingMs[TierIndex(tier)];
    }

} // namespace Dia::SimTime
