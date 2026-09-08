#include <gtest/gtest.h>

#include <DiaAIBudget/IAIBudgetedSystem.h>
#include <DiaAIBudget/AIBudgetScheduler.h>

// -------------------------------------------------------------------------
// Mock
// -------------------------------------------------------------------------

struct MockBudgetedSystem : public Dia::AIBudget::IAIBudgetedSystem
{
    explicit MockBudgetedSystem(const char* id)
        : mId(id), mCallCount(0), mLastBudgetMs(0.0f), mCallOrder(-1) {}

    Dia::Core::StringCRC GetSystemId() const override { return mId; }

    void UpdateBudgeted(float budgetMs) override
    {
        ++mCallCount;
        mLastBudgetMs = budgetMs;
        if (sCallSequenceCounter != nullptr)
        {
            mCallOrder = (*sCallSequenceCounter)++;
        }
    }

    Dia::Core::StringCRC mId;
    int   mCallCount;
    float mLastBudgetMs;
    int   mCallOrder;

    // Set this before Update() to record call ordering
    static int* sCallSequenceCounter;
};

int* MockBudgetedSystem::sCallSequenceCounter = nullptr;

// -------------------------------------------------------------------------
// Register / Unregister / GetRegisteredCount
// -------------------------------------------------------------------------

TEST(DiaAIBudget, Register_EmptyScheduler_CountIsZero)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    EXPECT_EQ(scheduler.GetRegisteredCount(), 0);
}

TEST(DiaAIBudget, Register_OneSystem_CountIsOne)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sys("SystemA");

    bool result = scheduler.Register(&sys);

    EXPECT_TRUE(result);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 1);
}

TEST(DiaAIBudget, Register_TwoSystems_CountIsTwo)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sysA("SystemA");
    MockBudgetedSystem sysB("SystemB");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);

    EXPECT_EQ(scheduler.GetRegisteredCount(), 2);
}

TEST(DiaAIBudget, Unregister_KnownSystem_CountDecrements)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sysA("SystemA");
    MockBudgetedSystem sysB("SystemB");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 2);

    scheduler.Unregister(&sysA);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 1);
}

TEST(DiaAIBudget, Unregister_UnknownPointer_IsNoOp)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sysA("SystemA");
    MockBudgetedSystem notRegistered("Ghost");

    scheduler.Register(&sysA);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 1);

    // Unregistering a pointer that was never registered must not crash
    scheduler.Unregister(&notRegistered);

    EXPECT_EQ(scheduler.GetRegisteredCount(), 1);
}

TEST(DiaAIBudget, Unregister_FromEmpty_IsNoOp)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sys("SystemA");

    // Must not crash
    scheduler.Unregister(&sys);

    EXPECT_EQ(scheduler.GetRegisteredCount(), 0);
}

// -------------------------------------------------------------------------
// Re-register after unregister
// -------------------------------------------------------------------------

TEST(DiaAIBudget, ReRegisterAfterUnregister_CountCorrect)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sys("SystemA");

    scheduler.Register(&sys);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 1);

    scheduler.Unregister(&sys);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 0);

    bool result = scheduler.Register(&sys);
    EXPECT_TRUE(result);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 1);
}

// -------------------------------------------------------------------------
// Register returns false at capacity (release path)
// -------------------------------------------------------------------------

#ifndef _DEBUG
TEST(DiaAIBudget, Register_AtCapacity_ReturnsFalse)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;

    // Fill to kMaxSystems (16)
    MockBudgetedSystem systems[Dia::AIBudget::AIBudgetScheduler::kMaxSystems] = {
        MockBudgetedSystem("S00"), MockBudgetedSystem("S01"), MockBudgetedSystem("S02"), MockBudgetedSystem("S03"),
        MockBudgetedSystem("S04"), MockBudgetedSystem("S05"), MockBudgetedSystem("S06"), MockBudgetedSystem("S07"),
        MockBudgetedSystem("S08"), MockBudgetedSystem("S09"), MockBudgetedSystem("S10"), MockBudgetedSystem("S11"),
        MockBudgetedSystem("S12"), MockBudgetedSystem("S13"), MockBudgetedSystem("S14"), MockBudgetedSystem("S15"),
    };

    for (int i = 0; i < Dia::AIBudget::AIBudgetScheduler::kMaxSystems; ++i)
    {
        ASSERT_TRUE(scheduler.Register(&systems[i]));
    }

    EXPECT_EQ(scheduler.GetRegisteredCount(), Dia::AIBudget::AIBudgetScheduler::kMaxSystems);

    // 17th registration must return false
    MockBudgetedSystem overflow("Overflow");
    bool result = scheduler.Register(&overflow);
    EXPECT_FALSE(result);

    // Count must not have changed
    EXPECT_EQ(scheduler.GetRegisteredCount(), Dia::AIBudget::AIBudgetScheduler::kMaxSystems);
}
#endif // !_DEBUG

// -------------------------------------------------------------------------
// Capacity overflow asserts in debug
// -------------------------------------------------------------------------

#ifdef _DEBUG
TEST(SLOW_DiaAIBudget_Boundary, Register_AtCapacity_Asserts)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;

    MockBudgetedSystem systems[Dia::AIBudget::AIBudgetScheduler::kMaxSystems] = {
        MockBudgetedSystem("S00"), MockBudgetedSystem("S01"), MockBudgetedSystem("S02"), MockBudgetedSystem("S03"),
        MockBudgetedSystem("S04"), MockBudgetedSystem("S05"), MockBudgetedSystem("S06"), MockBudgetedSystem("S07"),
        MockBudgetedSystem("S08"), MockBudgetedSystem("S09"), MockBudgetedSystem("S10"), MockBudgetedSystem("S11"),
        MockBudgetedSystem("S12"), MockBudgetedSystem("S13"), MockBudgetedSystem("S14"), MockBudgetedSystem("S15"),
    };

    for (int i = 0; i < Dia::AIBudget::AIBudgetScheduler::kMaxSystems; ++i)
    {
        scheduler.Register(&systems[i]);
    }

    MockBudgetedSystem overflow("Overflow");
    EXPECT_DEATH(scheduler.Register(&overflow), "");
}
#endif // _DEBUG

// -------------------------------------------------------------------------
// Update with large budget — all systems run
// -------------------------------------------------------------------------

TEST(DiaAIBudget, Update_LargeBudget_AllSystemsRun)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sysA("SysA");
    MockBudgetedSystem sysB("SysB");
    MockBudgetedSystem sysC("SysC");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(1000.0f);

    EXPECT_EQ(result.systemsRun, 3);
    EXPECT_EQ(result.systemsDeferred, 0);
    EXPECT_EQ(sysA.mCallCount, 1);
    EXPECT_EQ(sysB.mCallCount, 1);
    EXPECT_EQ(sysC.mCallCount, 1);
}

TEST(DiaAIBudget, Update_LargeBudget_UsedMsNonNegative)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sys("Sys");
    scheduler.Register(&sys);

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(1000.0f);

    EXPECT_GE(result.usedMs, 0.0f);
}

// -------------------------------------------------------------------------
// Update(0.0f) — all systems deferred
// -------------------------------------------------------------------------

TEST(DiaAIBudget, Update_ZeroBudget_AllSystemsDeferred)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sysA("SysA");
    MockBudgetedSystem sysB("SysB");
    MockBudgetedSystem sysC("SysC");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(0.0f);

    EXPECT_EQ(result.systemsRun, 0);
    EXPECT_EQ(result.systemsDeferred, 3);

    // None of the systems should have been called
    EXPECT_EQ(sysA.mCallCount, 0);
    EXPECT_EQ(sysB.mCallCount, 0);
    EXPECT_EQ(sysC.mCallCount, 0);
}

TEST(DiaAIBudget, Update_ZeroBudget_EmptyScheduler_NoSystems)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(0.0f);

    EXPECT_EQ(result.systemsRun, 0);
    EXPECT_EQ(result.systemsDeferred, 0);
}

// -------------------------------------------------------------------------
// Update on empty scheduler
// -------------------------------------------------------------------------

TEST(DiaAIBudget, Update_EmptyScheduler_ZeroTelemetry)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(100.0f);

    EXPECT_EQ(result.systemsRun, 0);
    EXPECT_EQ(result.systemsDeferred, 0);
    EXPECT_FLOAT_EQ(result.usedMs, 0.0f);
}

// -------------------------------------------------------------------------
// Call order — systems called in registration order
// -------------------------------------------------------------------------

TEST(DiaAIBudget, Update_CallOrder_RegistrationOrder)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sysA("SysA");
    MockBudgetedSystem sysB("SysB");
    MockBudgetedSystem sysC("SysC");

    // Register in A, B, C order
    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    int counter = 0;
    MockBudgetedSystem::sCallSequenceCounter = &counter;

    scheduler.Update(1000.0f);

    MockBudgetedSystem::sCallSequenceCounter = nullptr;

    EXPECT_EQ(sysA.mCallOrder, 0);
    EXPECT_EQ(sysB.mCallOrder, 1);
    EXPECT_EQ(sysC.mCallOrder, 2);
}

// -------------------------------------------------------------------------
// UpdateBudgeted receives correct slice — first gets full, second gets remainder
// -------------------------------------------------------------------------

TEST(DiaAIBudget, Update_FirstSystemReceivesFullBudget)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sysA("SysA");
    MockBudgetedSystem sysB("SysB");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);

    // With an instant mock, remaining barely decreases, but the first system
    // must receive the full totalBudgetMs.
    scheduler.Update(50.0f);

    EXPECT_FLOAT_EQ(sysA.mLastBudgetMs, 50.0f);
}

TEST(DiaAIBudget, Update_SecondSystemReceivesReducedBudget)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sysA("SysA");
    MockBudgetedSystem sysB("SysB");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);

    scheduler.Update(50.0f);

    // The second system receives (50.0f - time_for_sysA).
    // Since the mock returns instantly, elapsed is near-zero but strictly
    // the second budget must be <= 50.0f.
    EXPECT_LE(sysB.mLastBudgetMs, 50.0f);
    EXPECT_GT(sysB.mLastBudgetMs, 0.0f);
}

// -------------------------------------------------------------------------
// Unregister reduces the count and the remaining system still runs
// -------------------------------------------------------------------------

TEST(DiaAIBudget, Unregister_RemainingSystemStillRuns)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystem sysA("SysA");
    MockBudgetedSystem sysB("SysB");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);

    scheduler.Unregister(&sysA);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 1);

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(100.0f);

    EXPECT_EQ(result.systemsRun, 1);
    EXPECT_EQ(result.systemsDeferred, 0);
    EXPECT_EQ(sysA.mCallCount, 0);
    EXPECT_EQ(sysB.mCallCount, 1);
}
