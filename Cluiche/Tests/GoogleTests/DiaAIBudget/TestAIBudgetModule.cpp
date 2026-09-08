#include <gtest/gtest.h>

#include <DiaAIBudget/IAIBudgetedSystem.h>
#include <DiaAIBudget/AIBudgetScheduler.h>
#include <DiaAIBudget/AIBudgetModule.h>

// -------------------------------------------------------------------------
// Mock
// -------------------------------------------------------------------------

struct MockBudgetedSystemMod : public Dia::AIBudget::IAIBudgetedSystem
{
    explicit MockBudgetedSystemMod(const char* id)
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

// -------------------------------------------------------------------------
// Testable subclass exposing protected methods
// -------------------------------------------------------------------------

class AIBudgetModuleTestable : public Dia::AIBudget::AIBudgetModule
{
public:
    void Configure(const char* json) { OnConfigure(json); }
    void Update(float dt)
    {
        Dia::SimTime::SimTimeContext ctx{
            Dia::Core::TimeAbsolute::Zero(),
            Dia::Core::TimeRelative::CreateFromSeconds(dt),
            0, 1.0f, false
        };
        DoUpdate(ctx);
    }
};

// -------------------------------------------------------------------------
// OnConfigure — default budget
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Module, OnConfigure_NoCall_DefaultBudget_Is1Ms)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.GetScheduler().Register(&sys);

    // DoUpdate with the default budget (no OnConfigure call)
    mod.Update(0.0f);

    // Default is 1000us = 1.0f ms
    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 1.0f);
}

TEST(DiaAIBudget_Module, OnConfigure_EmptyString_UsesDefault)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("");
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 1.0f);
}

TEST(DiaAIBudget_Module, OnConfigure_NullString_UsesDefault)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure(nullptr);
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 1.0f);
}

// -------------------------------------------------------------------------
// OnConfigure — explicit budget values
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Module, OnConfigure_1000Us_ResultsIn1MsBudget)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("{\"budgetUs\":1000}");
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 1.0f);
}

TEST(DiaAIBudget_Module, OnConfigure_2000Us_ResultsIn2MsBudget)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("{\"budgetUs\":2000}");
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 2.0f);
}

TEST(DiaAIBudget_Module, OnConfigure_500Us_ResultsIn0_5MsBudget)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("{\"budgetUs\":500}");
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    EXPECT_NEAR(sys.mLastBudgetMs, 0.5f, 1e-3f);
}

TEST(DiaAIBudget_Module, OnConfigure_ZeroUs_SystemIsDeferred)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    // budgetUs=0 configures mBudgetMs=0.0f; the scheduler defers all systems
    // when remaining <= 0 before the first call, so UpdateBudgeted is never invoked.
    mod.Configure("{\"budgetUs\":0}");
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    EXPECT_EQ(sys.mCallCount, 0);
}

// -------------------------------------------------------------------------
// OnConfigure — malformed / missing key
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Module, OnConfigure_InvalidJson_UsesDefault)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("not json at all");
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 1.0f);
}

TEST(DiaAIBudget_Module, OnConfigure_MissingBudgetUsKey_UsesDefault)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("{\"otherKey\":500}");
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 1.0f);
}

TEST(DiaAIBudget_Module, OnConfigure_WrongType_UsesDefault)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("{\"budgetUs\":\"not_an_int\"}");
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 1.0f);
}

// -------------------------------------------------------------------------
// GetScheduler
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Module, GetScheduler_ReturnsMutableReference)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.GetScheduler().Register(&sys);

    EXPECT_EQ(mod.GetScheduler().GetRegisteredCount(), 1);
}

TEST(DiaAIBudget_Module, GetScheduler_Const_ReturnsConstReference)
{
    AIBudgetModuleTestable mod;

    const AIBudgetModuleTestable& cmod = mod;

    EXPECT_EQ(cmod.GetScheduler().GetRegisteredCount(), 0);
}

// -------------------------------------------------------------------------
// Integration — configure + register + update
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Module, Integration_ConfigureRegisterUpdate_SystemReceivesCorrectBudget)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("{\"budgetUs\":2000}");
    mod.GetScheduler().Register(&sys);

    // deltaTime is irrelevant — the scheduler uses the configured mBudgetMs
    mod.Update(0.0f);

    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 2.0f);
    EXPECT_EQ(sys.mCallCount, 1);
}

TEST(DiaAIBudget_Module, Integration_MultipleSystemsRegistered_AllReceiveBudget)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sysA("SysA");
    MockBudgetedSystemMod sysB("SysB");
    MockBudgetedSystemMod sysC("SysC");

    mod.Configure("{\"budgetUs\":1000}");
    mod.GetScheduler().Register(&sysA);
    mod.GetScheduler().Register(&sysB);
    mod.GetScheduler().Register(&sysC);

    mod.Update(0.0f);

    // All three systems must have been called
    EXPECT_EQ(sysA.mCallCount, 1);
    EXPECT_EQ(sysB.mCallCount, 1);
    EXPECT_EQ(sysC.mCallCount, 1);

    // Each system receives at most the total configured budget
    EXPECT_LE(sysA.mLastBudgetMs, 1.0f);
    EXPECT_LE(sysB.mLastBudgetMs, 1.0f);
    EXPECT_LE(sysC.mLastBudgetMs, 1.0f);
}

// -------------------------------------------------------------------------
// kInstanceId
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Module, KInstanceId_EqualsAIBudgetModule)
{
    EXPECT_EQ(Dia::AIBudget::AIBudgetModule::kInstanceId, Dia::Core::StringCRC("AIBudgetModule"));
}

// -------------------------------------------------------------------------
// GetLastResult
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Module, GetLastResult_BeforeUpdate_IsZeroInitialised)
{
    AIBudgetModuleTestable mod;

    const Dia::AIBudget::AIBudgetResult& result = mod.GetLastResult();

    EXPECT_EQ(result.systemsRun,       0);
    EXPECT_EQ(result.systemsDeferred,  0);
    EXPECT_FLOAT_EQ(result.usedMs,     0.0f);
}

TEST(DiaAIBudget_Module, GetLastResult_AfterUpdate_SystemsRunIsOne_WhenOneBudgetedSystem)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("{\"budgetUs\":1000}");
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    const Dia::AIBudget::AIBudgetResult& result = mod.GetLastResult();
    EXPECT_EQ(result.systemsRun,      1);
    EXPECT_EQ(result.systemsDeferred, 0);
}

TEST(DiaAIBudget_Module, GetLastResult_AfterUpdate_SystemsDeferredIsOne_WhenZeroBudget)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("{\"budgetUs\":0}");  // zero budget — all systems deferred
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    const Dia::AIBudget::AIBudgetResult& result = mod.GetLastResult();
    EXPECT_EQ(result.systemsRun,      0);
    EXPECT_EQ(result.systemsDeferred, 1);
}

TEST(DiaAIBudget_Module, GetLastResult_UpdatedEachFrame_ReflectsMostRecentResult)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("{\"budgetUs\":1000}");
    mod.GetScheduler().Register(&sys);

    mod.Update(0.0f);
    EXPECT_EQ(mod.GetLastResult().systemsRun, 1);

    // Unregister then update — zero systems this time
    mod.GetScheduler().Unregister(&sys);
    mod.Update(0.0f);
    EXPECT_EQ(mod.GetLastResult().systemsRun, 0);
}

TEST(DiaAIBudget_Module, GetLastResult_ThreeSystems_AllRun_CountIsThree)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sysA("A"), sysB("B"), sysC("C");

    mod.Configure("{\"budgetUs\":5000}");  // generous budget — all three run
    mod.GetScheduler().Register(&sysA);
    mod.GetScheduler().Register(&sysB);
    mod.GetScheduler().Register(&sysC);
    mod.Update(0.0f);

    const Dia::AIBudget::AIBudgetResult& result = mod.GetLastResult();
    EXPECT_EQ(result.systemsRun,      3);
    EXPECT_EQ(result.systemsDeferred, 0);
}

// -------------------------------------------------------------------------
// GetBudgetMs
// -------------------------------------------------------------------------

TEST(DiaAIBudget_Module, GetBudgetMs_BeforeConfigure_Is1Ms)
{
    AIBudgetModuleTestable mod;
    EXPECT_FLOAT_EQ(mod.GetBudgetMs(), 1.0f);
}

TEST(DiaAIBudget_Module, GetBudgetMs_After1000UsConfig_Is1Ms)
{
    AIBudgetModuleTestable mod;
    mod.Configure("{\"budgetUs\":1000}");
    EXPECT_FLOAT_EQ(mod.GetBudgetMs(), 1.0f);
}

TEST(DiaAIBudget_Module, GetBudgetMs_After2000UsConfig_Is2Ms)
{
    AIBudgetModuleTestable mod;
    mod.Configure("{\"budgetUs\":2000}");
    EXPECT_FLOAT_EQ(mod.GetBudgetMs(), 2.0f);
}

TEST(DiaAIBudget_Module, GetBudgetMs_After500UsConfig_Is0_5Ms)
{
    AIBudgetModuleTestable mod;
    mod.Configure("{\"budgetUs\":500}");
    EXPECT_NEAR(mod.GetBudgetMs(), 0.5f, 1e-3f);
}

TEST(DiaAIBudget_Module, GetBudgetMs_AfterEmptyConfig_Is1Ms)
{
    AIBudgetModuleTestable mod;
    mod.Configure("");
    EXPECT_FLOAT_EQ(mod.GetBudgetMs(), 1.0f);
}

TEST(DiaAIBudget_Module, GetBudgetMs_AfterInvalidConfig_Is1Ms)
{
    AIBudgetModuleTestable mod;
    mod.Configure("not json");
    EXPECT_FLOAT_EQ(mod.GetBudgetMs(), 1.0f);
}

TEST(DiaAIBudget_Module, GetBudgetMs_MatchesBudgetReceivedBySystem)
{
    AIBudgetModuleTestable mod;
    MockBudgetedSystemMod sys("Sys");

    mod.Configure("{\"budgetUs\":3000}");
    mod.GetScheduler().Register(&sys);
    mod.Update(0.0f);

    // GetBudgetMs() must agree with the slice received by the only system
    EXPECT_FLOAT_EQ(mod.GetBudgetMs(), 3.0f);
    EXPECT_FLOAT_EQ(sys.mLastBudgetMs, 3.0f);
}
