#include <gtest/gtest.h>

#include <DiaAIBudget/IAIBudgetedSystem.h>
#include <DiaAIBudget/AIBudgetScheduler.h>

// -------------------------------------------------------------------------
// Mock
// -------------------------------------------------------------------------

struct MockBudgetedSystemExt : public Dia::AIBudget::IAIBudgetedSystem
{
    explicit MockBudgetedSystemExt(const char* id)
        : mId(id), mCallCount(0), mLastBudgetMs(-1.0f), mCallOrder(-1) {}

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

    static int* sCallSequenceCounter;
};

int* MockBudgetedSystemExt::sCallSequenceCounter = nullptr;

// -------------------------------------------------------------------------
// Unit — zero budget (valid edge case: all systems deferred, no crash)
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Scheduler, Update_ZeroBudget_AllSystemsDeferred_NoCrash)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");
    MockBudgetedSystemExt sysB("SysB");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(0.0f);

    EXPECT_EQ(result.systemsRun, 0);
    EXPECT_EQ(result.systemsDeferred, 2);
    EXPECT_EQ(sysA.mCallCount, 0);
    EXPECT_EQ(sysB.mCallCount, 0);
}

// -------------------------------------------------------------------------
// Unit — unregister position variants
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Scheduler, Unregister_FirstOfThree_RemainingRunInOrder)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");
    MockBudgetedSystemExt sysB("SysB");
    MockBudgetedSystemExt sysC("SysC");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    scheduler.Unregister(&sysA);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 2);

    int counter = 0;
    MockBudgetedSystemExt::sCallSequenceCounter = &counter;

    scheduler.Update(1000.0f);

    MockBudgetedSystemExt::sCallSequenceCounter = nullptr;

    EXPECT_EQ(sysA.mCallCount, 0);
    EXPECT_EQ(sysB.mCallOrder, 0);
    EXPECT_EQ(sysC.mCallOrder, 1);
}

TEST(DiaAIBudget_Scheduler, Unregister_LastOfThree_RemainingRunInOrder)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");
    MockBudgetedSystemExt sysB("SysB");
    MockBudgetedSystemExt sysC("SysC");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    scheduler.Unregister(&sysC);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 2);

    int counter = 0;
    MockBudgetedSystemExt::sCallSequenceCounter = &counter;

    scheduler.Update(1000.0f);

    MockBudgetedSystemExt::sCallSequenceCounter = nullptr;

    EXPECT_EQ(sysC.mCallCount, 0);
    EXPECT_EQ(sysA.mCallOrder, 0);
    EXPECT_EQ(sysB.mCallOrder, 1);
}

TEST(DiaAIBudget_Scheduler, Unregister_MiddleOfThree_RemainingRunInOrder)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");
    MockBudgetedSystemExt sysB("SysB");
    MockBudgetedSystemExt sysC("SysC");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    scheduler.Unregister(&sysB);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 2);

    int counter = 0;
    MockBudgetedSystemExt::sCallSequenceCounter = &counter;

    scheduler.Update(1000.0f);

    MockBudgetedSystemExt::sCallSequenceCounter = nullptr;

    EXPECT_EQ(sysB.mCallCount, 0);
    EXPECT_EQ(sysA.mCallOrder, 0);
    EXPECT_EQ(sysC.mCallOrder, 1);
}

// -------------------------------------------------------------------------
// Unit — double unregister
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Scheduler, Unregister_SamePointerTwice_SecondIsNoOp)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");

    scheduler.Register(&sysA);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 1);

    scheduler.Unregister(&sysA);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 0);

    // Second unregister must not crash
    scheduler.Unregister(&sysA);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 0);
}

// -------------------------------------------------------------------------
// Unit — unregister all then update
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Scheduler, Unregister_AllSystems_UpdateReturnsZeros)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");
    MockBudgetedSystemExt sysB("SysB");
    MockBudgetedSystemExt sysC("SysC");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    scheduler.Unregister(&sysA);
    scheduler.Unregister(&sysB);
    scheduler.Unregister(&sysC);

    EXPECT_EQ(scheduler.GetRegisteredCount(), 0);

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(100.0f);

    EXPECT_EQ(result.systemsRun, 0);
    EXPECT_EQ(result.systemsDeferred, 0);
    EXPECT_FLOAT_EQ(result.usedMs, 0.0f);
}

// -------------------------------------------------------------------------
// Unit — consecutive updates
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Scheduler, Update_ConsecutiveCalls_TelemetryFreshEachCall)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");
    MockBudgetedSystemExt sysB("SysB");
    MockBudgetedSystemExt sysC("SysC");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    Dia::AIBudget::AIBudgetResult result1 = scheduler.Update(100.0f);
    Dia::AIBudget::AIBudgetResult result2 = scheduler.Update(100.0f);

    EXPECT_EQ(result1.systemsRun, 3);
    EXPECT_EQ(result1.systemsDeferred, 0);

    EXPECT_EQ(result2.systemsRun, 3);
    EXPECT_EQ(result2.systemsDeferred, 0);

    // Each system must have been called once per Update call, so twice total
    EXPECT_EQ(sysA.mCallCount, 2);
    EXPECT_EQ(sysB.mCallCount, 2);
    EXPECT_EQ(sysC.mCallCount, 2);
}

// -------------------------------------------------------------------------
// Unit — result invariant: systemsRun + systemsDeferred == GetRegisteredCount()
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Scheduler, Update_ZeroBudget_RunPlusDeferredEqualsTotal)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");
    MockBudgetedSystemExt sysB("SysB");
    MockBudgetedSystemExt sysC("SysC");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(0.0f);

    EXPECT_EQ(result.systemsRun + result.systemsDeferred, scheduler.GetRegisteredCount());
    EXPECT_EQ(result.systemsRun, 0);
    EXPECT_EQ(result.systemsDeferred, 3);
}

TEST(DiaAIBudget_Scheduler, Update_LargeBudget_RunPlusDeferredEqualsTotal)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");
    MockBudgetedSystemExt sysB("SysB");
    MockBudgetedSystemExt sysC("SysC");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(1000.0f);

    EXPECT_EQ(result.systemsRun + result.systemsDeferred, scheduler.GetRegisteredCount());
    EXPECT_EQ(result.systemsRun, 3);
    EXPECT_EQ(result.systemsDeferred, 0);
}

// -------------------------------------------------------------------------
// Unit — usedMs
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Scheduler, Update_EmptyScheduler_UsedMsIsZero)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(100.0f);

    EXPECT_FLOAT_EQ(result.usedMs, 0.0f);
}

TEST(DiaAIBudget_Scheduler, Update_SingleSystem_UsedMsIsNonNegative)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sys("Sys");

    scheduler.Register(&sys);

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(100.0f);

    EXPECT_GE(result.usedMs, 0.0f);
}

// -------------------------------------------------------------------------
// Unit — GetRegisteredCount with mixed ops
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Scheduler, GetRegisteredCount_MixedOps_Correct)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");
    MockBudgetedSystemExt sysB("SysB");
    MockBudgetedSystemExt sysC("SysC");
    MockBudgetedSystemExt sysD("SysD");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 3);

    scheduler.Unregister(&sysB);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 2);

    scheduler.Register(&sysD);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 3);
}

// -------------------------------------------------------------------------
// Debug death test — null pointer
// -------------------------------------------------------------------------

#ifdef _DEBUG
TEST(SLOW_DiaAIBudget_Boundary, Register_NullPointer_Asserts)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    EXPECT_DEATH(scheduler.Register(nullptr), "");
}
#endif // _DEBUG

// -------------------------------------------------------------------------
// Stress — register full capacity, all run
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Stress, RegisterFullCapacity_AllRun)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;

    MockBudgetedSystemExt systems[Dia::AIBudget::AIBudgetScheduler::kMaxSystems] = {
        MockBudgetedSystemExt("S00"), MockBudgetedSystemExt("S01"), MockBudgetedSystemExt("S02"), MockBudgetedSystemExt("S03"),
        MockBudgetedSystemExt("S04"), MockBudgetedSystemExt("S05"), MockBudgetedSystemExt("S06"), MockBudgetedSystemExt("S07"),
        MockBudgetedSystemExt("S08"), MockBudgetedSystemExt("S09"), MockBudgetedSystemExt("S10"), MockBudgetedSystemExt("S11"),
        MockBudgetedSystemExt("S12"), MockBudgetedSystemExt("S13"), MockBudgetedSystemExt("S14"), MockBudgetedSystemExt("S15"),
    };

    for (int i = 0; i < Dia::AIBudget::AIBudgetScheduler::kMaxSystems; ++i)
    {
        ASSERT_TRUE(scheduler.Register(&systems[i]));
    }

    EXPECT_EQ(scheduler.GetRegisteredCount(), Dia::AIBudget::AIBudgetScheduler::kMaxSystems);

    Dia::AIBudget::AIBudgetResult result = scheduler.Update(1000.0f);

    EXPECT_EQ(result.systemsRun, Dia::AIBudget::AIBudgetScheduler::kMaxSystems);
    EXPECT_EQ(result.systemsDeferred, 0);

    for (int i = 0; i < Dia::AIBudget::AIBudgetScheduler::kMaxSystems; ++i)
    {
        EXPECT_EQ(systems[i].mCallCount, 1) << "System " << i << " was not called exactly once";
    }
}

// -------------------------------------------------------------------------
// Stress — register/unregister 1000 cycles
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Stress, RegisterUnregister_1000Cycles_CountConsistent)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sys("CycleSys");

    for (int i = 0; i < 1000; ++i)
    {
        bool registered = scheduler.Register(&sys);
        ASSERT_TRUE(registered) << "Register failed on cycle " << i;
        ASSERT_EQ(scheduler.GetRegisteredCount(), 1) << "Count wrong after register on cycle " << i;

        scheduler.Unregister(&sys);
        ASSERT_EQ(scheduler.GetRegisteredCount(), 0) << "Count wrong after unregister on cycle " << i;
    }
}

// -------------------------------------------------------------------------
// Stress — 1000 consecutive updates, no state corruption
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Stress, Update_1000Times_NoStateCorruption)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MockBudgetedSystemExt sysA("SysA");
    MockBudgetedSystemExt sysB("SysB");
    MockBudgetedSystemExt sysC("SysC");

    scheduler.Register(&sysA);
    scheduler.Register(&sysB);
    scheduler.Register(&sysC);

    for (int i = 0; i < 1000; ++i)
    {
        Dia::AIBudget::AIBudgetResult result = scheduler.Update(100.0f);
        ASSERT_EQ(result.systemsRun, 3) << "systemsRun wrong on iteration " << i;
        ASSERT_EQ(result.systemsDeferred, 0) << "systemsDeferred wrong on iteration " << i;
    }

    EXPECT_EQ(sysA.mCallCount, 1000);
    EXPECT_EQ(sysB.mCallCount, 1000);
    EXPECT_EQ(sysC.mCallCount, 1000);
}
