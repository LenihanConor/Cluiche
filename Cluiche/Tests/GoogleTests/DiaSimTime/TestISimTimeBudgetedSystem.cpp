////////////////////////////////////////////////////////////////////////////////
// Filename: TestISimTimeBudgetedSystem.cpp
// GoogleTest suite — DiaSimTime Task 4.1: ISimTimeBudgetedSystem + SimTimePriority.
//
// Covers:
//   - GetPriority() defaults to SimTimePriority::kNormal when a concrete subclass
//     only implements the inherited pure-virtual GetSystemId()/UpdateBudgeted().
//   - An ISimTimeBudgetedSystem* upcasts to Dia::AIBudget::IAIBudgetedSystem* and
//     can be registered with a real AIBudgetScheduler (backward-compat proof).
//   - UpdateBudgeted(0.0f) is a valid no-op (AB-005), inherited unchanged.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaSimTime/ISimTimeBudgetedSystem.h>
#include <DiaSimTime/SimTimePriority.h>
#include <DiaAIBudget/AIBudgetScheduler.h>
#include <DiaAIBudget/IAIBudgetedSystem.h>
#include <DiaCore/CRC/StringCRC.h>

using Dia::SimTime::ISimTimeBudgetedSystem;
using Dia::SimTime::SimTimePriority;

namespace {

    // Minimal concrete implementation: only overrides the inherited pure-virtuals,
    // leaving GetPriority() to fall back to the base class default.
    struct MinimalSimTimeSystem : public ISimTimeBudgetedSystem
    {
        explicit MinimalSimTimeSystem(const char* id)
            : mId(id), mCallCount(0), mLastBudgetMs(-1.0f) {}

        Dia::Core::StringCRC GetSystemId() const override { return mId; }

        void UpdateBudgeted(float budgetMs) override
        {
            ++mCallCount;
            mLastBudgetMs = budgetMs;
        }

        Dia::Core::StringCRC mId;
        int   mCallCount;
        float mLastBudgetMs;
    };

} // namespace

// -------------------------------------------------------------------------
// GetPriority() default
// -------------------------------------------------------------------------

TEST(DiaSimTime_ISimTimeBudgetedSystem, GetPriority_NotOverridden_DefaultsToNormal)
{
    MinimalSimTimeSystem sys("MinimalSystem");
    EXPECT_EQ(sys.GetPriority(), SimTimePriority::kNormal);
}

// -------------------------------------------------------------------------
// Backward compatibility with Dia::AIBudget::AIBudgetScheduler
// -------------------------------------------------------------------------

TEST(DiaSimTime_ISimTimeBudgetedSystem, UpcastToIAIBudgetedSystem_RegistersWithAIBudgetScheduler)
{
    Dia::AIBudget::AIBudgetScheduler scheduler;
    MinimalSimTimeSystem sys("MinimalSystem");

    // ISimTimeBudgetedSystem* -> Dia::AIBudget::IAIBudgetedSystem* upcast, passed
    // directly to the existing scheduler with zero adapter code.
    Dia::AIBudget::IAIBudgetedSystem* asBase = &sys;
    bool result = scheduler.Register(asBase);

    EXPECT_TRUE(result);
    EXPECT_EQ(scheduler.GetRegisteredCount(), 1);

    scheduler.Update(10.0f);
    EXPECT_EQ(sys.mCallCount, 1);
}

// -------------------------------------------------------------------------
// AB-005: UpdateBudgeted(0.0f) is a valid no-op
// -------------------------------------------------------------------------

TEST(DiaSimTime_ISimTimeBudgetedSystem, UpdateBudgeted_ZeroBudget_IsValidNoOp)
{
    MinimalSimTimeSystem sys("MinimalSystem");

    EXPECT_NO_FATAL_FAILURE(sys.UpdateBudgeted(0.0f));
    EXPECT_EQ(sys.mCallCount, 1);
    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 0.0f);
}
