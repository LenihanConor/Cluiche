////////////////////////////////////////////////////////////////////////////////
// Filename: TestSimTimeBudget.cpp
// GoogleTest suite — DiaSimTime Task 4.2: SimTimeBudget + IOneShotWork.
//
// Covers:
//   - Deadline promotion (ST-008): a zero-budget kBackground system is deferred
//     across ticks, then force-run once its per-tier deadline is exceeded.
//   - Telemetry accuracy: used_ms / deferred_count / stale_ms_max.
//   - One-shot completion path (ST-012): transient IOneShotWork items run to
//     completion, completed items are removed, and one-shot volume is bounded by
//     its own generous capacity — NOT by the kMaxSystems steady-state cap.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaSimTime/SimTimeBudget.h>
#include <DiaSimTime/ISimTimeBudgetedSystem.h>
#include <DiaSimTime/IOneShotWork.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaSimTime/SimTimePriority.h>
#include <DiaCore/CRC/StringCRC.h>

#include <chrono>
#include <thread>
#include <vector>

using Dia::SimTime::SimTimeBudget;
using Dia::SimTime::ISimTimeBudgetedSystem;
using Dia::SimTime::IOneShotWork;
using Dia::SimTime::SimTimeContext;
using Dia::SimTime::SimTimePriority;

namespace {

    // A budgeted system that records how many times it ran and at what allowance.
    // Optionally busy-sleeps for a fixed duration inside UpdateBudgeted so used_ms
    // has something measurable to attribute to it.
    struct CountingSystem : public ISimTimeBudgetedSystem
    {
        CountingSystem(const char* id, SimTimePriority prio, int sleepMs = 0)
            : mId(id), mPriority(prio), mSleepMs(sleepMs) {}

        Dia::Core::StringCRC GetSystemId() const override { return mId; }
        SimTimePriority      GetPriority() const override { return mPriority; }

        void UpdateBudgeted(float budgetMs) override
        {
            ++mCallCount;
            mLastBudgetMs = budgetMs;
            if (mSleepMs > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(mSleepMs));
        }

        Dia::Core::StringCRC mId;
        SimTimePriority      mPriority;
        int                  mSleepMs;
        int                  mCallCount    = 0;
        float                mLastBudgetMs = -1.0f;
    };

    // A one-shot work item that completes after `stepsToComplete` Step() calls.
    // Asserts (via a counter checked by the test) that Step() is never called
    // again after it has returned true.
    struct FakeOneShot : public IOneShotWork
    {
        explicit FakeOneShot(int stepsToComplete) : mStepsToComplete(stepsToComplete) {}

        bool Step(float /*budgetMs*/) override
        {
            ++mStepCount;
            if (mCompleted)
                ++mCallsAfterDone;   // must remain 0 — completed items are removed
            ++mStepsTaken;
            if (mStepsTaken >= mStepsToComplete)
            {
                mCompleted = true;
                return true;
            }
            return false;
        }

        int  mStepsToComplete;
        int  mStepCount      = 0;
        int  mStepsTaken     = 0;
        int  mCallsAfterDone = 0;
        bool mCompleted      = false;
    };

    SimTimeContext MakeCtx()
    {
        // ST-010: SimTimeBudget ignores every ctx field (gating is wall-clock),
        // so exact values are irrelevant — just a well-formed context.
        return SimTimeContext{
            Dia::Core::TimeAbsolute::Zero(),
            Dia::Core::TimeRelative::Zero(),
            /*tick*/ 0,
            /*timeScale*/ 1.0f,
            /*isPaused*/ false
        };
    }

} // namespace

// -------------------------------------------------------------------------
// 1. Deadline promotion (ST-008)
// -------------------------------------------------------------------------

TEST(DiaSimTime_SimTimeBudget, BackgroundStarvedThenPromotedAtDeadline)
{
    SimTimeBudget budget;
    // Background tier gets zero budget: it can ONLY ever run via promotion.
    budget.SetTierBudgets(2.0f, 1.0f, 0.5f, 0.0f);
    // Tight-ish background deadline so the test stays fast.
    budget.SetTierDeadlines(4.0f, 16.0f, 100.0f, 30.0f);

    CountingSystem bg("bg.system", SimTimePriority::kBackground);
    budget.Register(&bg);

    // Several quick ticks well under the 30ms deadline: deferred, never run.
    for (int i = 0; i < 3; ++i)
    {
        budget.AllocateTick(MakeCtx());
        const bool ran = budget.RunIfCapacity(&bg);
        EXPECT_FALSE(ran);
        EXPECT_EQ(bg.mCallCount, 0);
        EXPECT_EQ(budget.GetLastDeferredCount(), 1);
        EXPECT_EQ(budget.GetLastPromotedCount(), 0);
    }

    // Let wall-clock staleness exceed the background deadline.
    std::this_thread::sleep_for(std::chrono::milliseconds(45));

    budget.AllocateTick(MakeCtx());
    const bool ranAfterDeadline = budget.RunIfCapacity(&bg);

    EXPECT_TRUE(ranAfterDeadline);           // promoted despite an empty pool
    EXPECT_EQ(bg.mCallCount, 1);             // it actually ran
    EXPECT_EQ(budget.GetLastPromotedCount(), 1);
    EXPECT_EQ(budget.GetLastDeferredCount(), 0);   // a promoted run is NOT deferred
}

// -------------------------------------------------------------------------
// 2a. deferred_count / stale_ms_max accuracy
// -------------------------------------------------------------------------

TEST(DiaSimTime_SimTimeBudget, DeferredCountAndStaleMsMaxAreAccurate)
{
    SimTimeBudget budget;
    budget.SetTierBudgets(2.0f, 1.0f, 0.5f, 0.0f);   // background zero
    // Huge background deadline: nothing promotes, everything stays deferred.
    budget.SetTierDeadlines(4.0f, 16.0f, 100.0f, 100000.0f);

    CountingSystem a("bg.a", SimTimePriority::kBackground);
    CountingSystem b("bg.b", SimTimePriority::kBackground);
    CountingSystem c("bg.c", SimTimePriority::kBackground);
    budget.Register(&a);
    budget.Register(&b);
    budget.Register(&c);

    const unsigned long long deferredBefore = budget.GetDeferredCountTotal();

    budget.AllocateTick(MakeCtx());
    EXPECT_FALSE(budget.RunIfCapacity(&a));
    EXPECT_FALSE(budget.RunIfCapacity(&b));
    EXPECT_FALSE(budget.RunIfCapacity(&c));

    EXPECT_EQ(budget.GetLastDeferredCount(), 3);
    EXPECT_EQ(a.mCallCount, 0);
    EXPECT_EQ(b.mCallCount, 0);
    EXPECT_EQ(c.mCallCount, 0);
    // Lifetime monotonic counter advanced by exactly the 3 deferral events.
    EXPECT_EQ(budget.GetDeferredCountTotal() - deferredBefore, 3ull);
    // Freshly registered -> staleness is tiny this first tick.
    EXPECT_LT(budget.GetLastStaleMsMax(), 25.0f);

    // Let a known amount of staleness accrue, then re-defer.
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    budget.AllocateTick(MakeCtx());
    EXPECT_FALSE(budget.RunIfCapacity(&a));
    EXPECT_FALSE(budget.RunIfCapacity(&b));
    EXPECT_FALSE(budget.RunIfCapacity(&c));

    EXPECT_EQ(budget.GetLastDeferredCount(), 3);
    // stale_ms_max reflects the oldest deferred work (>= the ~30ms we slept).
    EXPECT_GE(budget.GetLastStaleMsMax(), 25.0f);
    EXPECT_LT(budget.GetLastStaleMsMax(), 100000.0f);   // still under the deadline
}

// -------------------------------------------------------------------------
// 2b. used_ms accuracy
// -------------------------------------------------------------------------

TEST(DiaSimTime_SimTimeBudget, UsedMsReflectsWallClockConsumed)
{
    SimTimeBudget budget;   // default tier budgets

    CountingSystem slow("crit.slow", SimTimePriority::kCritical, /*sleepMs*/ 5);
    CountingSystem fast("norm.fast", SimTimePriority::kNormal,   /*sleepMs*/ 0);
    budget.Register(&slow);
    budget.Register(&fast);

    budget.AllocateTick(MakeCtx());
    EXPECT_TRUE(budget.RunIfCapacity(&slow));
    EXPECT_TRUE(budget.RunIfCapacity(&fast));

    EXPECT_EQ(slow.mCallCount, 1);
    EXPECT_EQ(fast.mCallCount, 1);
    // The slow system slept ~5ms; used_ms must reflect at least that.
    EXPECT_GE(budget.GetLastUsedMs(), 3.0f);
    // Critical tier had 2ms; the 5ms overrun floors the pool at zero (not negative).
    EXPECT_FLOAT_EQ(budget.GetTierRemainingMs(SimTimePriority::kCritical), 0.0f);

    // AllocateTick resets used_ms for the next tick.
    budget.AllocateTick(MakeCtx());
    EXPECT_FLOAT_EQ(budget.GetLastUsedMs(), 0.0f);
}

// -------------------------------------------------------------------------
// 3. One-shot completion path (ST-012)
// -------------------------------------------------------------------------

TEST(DiaSimTime_SimTimeBudget, OneShotItemsRunToCompletionAndAreRemoved)
{
    SimTimeBudget budget;

    FakeOneShot single1(1);
    FakeOneShot single2(1);
    FakeOneShot multi3(3);   // needs 3 RunOneShots calls to finish

    budget.SubmitOneShot(&single1);
    budget.SubmitOneShot(&multi3);
    budget.SubmitOneShot(&single2);
    EXPECT_EQ(budget.GetPendingOneShotCount(), 3);

    // Drive several generous slices; each call advances every still-queued item
    // by one Step().
    for (int i = 0; i < 5; ++i)
        budget.RunOneShots(1000.0f);

    // All completed and removed from the queue.
    EXPECT_EQ(budget.GetPendingOneShotCount(), 0);
    EXPECT_TRUE(single1.mCompleted);
    EXPECT_TRUE(single2.mCompleted);
    EXPECT_TRUE(multi3.mCompleted);

    // Completed items are never Step()'d again after returning true.
    EXPECT_EQ(single1.mCallsAfterDone, 0);
    EXPECT_EQ(single2.mCallsAfterDone, 0);
    EXPECT_EQ(multi3.mCallsAfterDone, 0);

    // Exact step accounting.
    EXPECT_EQ(single1.mStepCount, 1);
    EXPECT_EQ(single2.mStepCount, 1);
    EXPECT_EQ(multi3.mStepCount, 3);
}

// -------------------------------------------------------------------------
// 4. One-shot volume is NOT bounded by the steady-state kMaxSystems cap
// -------------------------------------------------------------------------

TEST(DiaSimTime_SimTimeBudget, OneShotQueueScalesBeyondSteadyStateCap)
{
    SimTimeBudget budget;

    // Far more one-shots than the steady-state registration cap allows.
    const int kCount = SimTimeBudget::kMaxSystems * 4;   // 64 >> 16
    ASSERT_GT(kCount, SimTimeBudget::kMaxSystems);
    ASSERT_LE(kCount, SimTimeBudget::kMaxOneShots);

    std::vector<FakeOneShot> items;
    items.reserve(kCount);
    for (int i = 0; i < kCount; ++i)
        items.emplace_back(1);
    for (int i = 0; i < kCount; ++i)
        budget.SubmitOneShot(&items[i]);

    // No overflow assert; steady-state registration is entirely unaffected.
    EXPECT_EQ(budget.GetPendingOneShotCount(), kCount);
    EXPECT_EQ(budget.GetRegisteredCount(), 0);

    budget.RunOneShots(100000.0f);

    EXPECT_EQ(budget.GetPendingOneShotCount(), 0);
    for (int i = 0; i < kCount; ++i)
    {
        EXPECT_TRUE(items[i].mCompleted);
        EXPECT_EQ(items[i].mStepCount, 1);
    }
}
