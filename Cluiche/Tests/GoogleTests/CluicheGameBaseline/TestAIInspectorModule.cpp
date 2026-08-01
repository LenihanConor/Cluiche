#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <Modules/AIInspectorModule.h>
#include <DiaAIBudget/AIBudgetModule.h>
#include <DiaCore/CRC/StringCRC.h>

#include <memory>

// ---------------------------------------------------------------------------
// Testable subclass exposing protected ring-buffer state and PushBudget.
// Bypasses DoStart() — no Module framework wiring needed.
// ---------------------------------------------------------------------------

class AIInspectorModuleTestable : public Cluiche::AppFlow::AIInspectorModule
{
public:
    explicit AIInspectorModuleTestable(const char* id = "TestAIInspector")
        : AIInspectorModule(Dia::Core::StringCRC(id))
    {}

    void SetBudgetModule(Dia::AIBudget::AIBudgetModule* mod) { mBudgetModule = mod; }

    int   GetHistoryCount() const { return mBudgetHistoryCount; }
    int   GetHistoryHead()  const { return mBudgetHistoryHead; }
    float GetAccSec()       const { return mBudgetAccSec; }

    const BudgetFrame& GetHistoryEntry(int logicalIdx) const
    {
        const int count   = mBudgetHistoryCount;
        const int head    = mBudgetHistoryHead;
        const int physIdx = (head - count + logicalIdx + kBudgetHistoryDepth) % kBudgetHistoryDepth;
        return mBudgetHistory[physIdx];
    }

    using AIInspectorModule::PushBudget;
};

// Minimal stand-in for AIBudgetModule — exposes only Configure so tests can
// set budgetMs without ever touching the scheduler or calling DoUpdate.
class AIBudgetModuleStub : public Dia::AIBudget::AIBudgetModule
{
public:
    void Configure(const char* json) { OnConfigure(json); }
};

// ===========================================================================
// DiaAIInspectorModule_Budget — ring buffer and periodic push logic
//
// All objects are heap-allocated (Module subclasses carry large inline state).
// DoUpdate() and GetScheduler().Register() are NOT called in this TU —
// those paths are covered by TestAIBudgetModule.cpp.
// PushBudget reads GetLastResult() (zero-initialized) and GetBudgetMs()
// (set via Configure), both safe to call without DoStart() or DoUpdate().
// ===========================================================================

TEST(DiaAIInspectorModule_Budget, Construct_NoCrash)
{
    auto mod = std::make_unique<AIInspectorModuleTestable>();
    EXPECT_EQ(mod->GetHistoryCount(), 0);
}

TEST(DiaAIInspectorModule_Budget, NullBudgetModule_PushBudget_NoStateChange)
{
    auto mod = std::make_unique<AIInspectorModuleTestable>();
    mod->PushBudget(0.016f);
    EXPECT_EQ(mod->GetHistoryCount(), 0);
    EXPECT_EQ(mod->GetHistoryHead(),  0);
}

TEST(DiaAIInspectorModule_Budget, SinglePush_HistoryCountIsOne)
{
    auto mod       = std::make_unique<AIInspectorModuleTestable>();
    auto budgetMod = std::make_unique<AIBudgetModuleStub>();
    mod->SetBudgetModule(budgetMod.get());
    mod->PushBudget(0.016f);
    EXPECT_EQ(mod->GetHistoryCount(), 1);
}

TEST(DiaAIInspectorModule_Budget, NFrames_HistoryCountGrowsToDepth)
{
    auto mod       = std::make_unique<AIInspectorModuleTestable>();
    auto budgetMod = std::make_unique<AIBudgetModuleStub>();
    mod->SetBudgetModule(budgetMod.get());

    for (int i = 0; i < 30; ++i)
        mod->PushBudget(0.016f);
    EXPECT_EQ(mod->GetHistoryCount(), 30);

    for (int i = 0; i < 40; ++i)
        mod->PushBudget(0.016f);
    EXPECT_EQ(mod->GetHistoryCount(), 60);
}

TEST(DiaAIInspectorModule_Budget, WrapAt60_CountRemainsAtDepth_HeadAdvances)
{
    auto mod       = std::make_unique<AIInspectorModuleTestable>();
    auto budgetMod = std::make_unique<AIBudgetModuleStub>();
    mod->SetBudgetModule(budgetMod.get());

    for (int i = 0; i < 60; ++i)
        mod->PushBudget(0.016f);

    int headBefore = mod->GetHistoryHead();
    mod->PushBudget(0.016f);

    EXPECT_EQ(mod->GetHistoryCount(), 60);
    EXPECT_EQ(mod->GetHistoryHead(), (headBefore + 1) % 60);
}

TEST(DiaAIInspectorModule_Budget, Periodic_NoEmitBeforeOneSec_Accumulates)
{
    auto mod       = std::make_unique<AIInspectorModuleTestable>();
    auto budgetMod = std::make_unique<AIBudgetModuleStub>();
    mod->SetBudgetModule(budgetMod.get());

    for (int i = 0; i < 30; ++i)
        mod->PushBudget(0.016f);

    EXPECT_NEAR(mod->GetAccSec(), 0.48f, 0.01f);
    EXPECT_EQ(mod->GetHistoryCount(), 30);
}

TEST(DiaAIInspectorModule_Budget, Periodic_AfterOneSec_AccumDecremented)
{
    auto mod       = std::make_unique<AIInspectorModuleTestable>();
    auto budgetMod = std::make_unique<AIBudgetModuleStub>();
    mod->SetBudgetModule(budgetMod.get());

    for (int i = 0; i < 63; ++i)
        mod->PushBudget(0.016f);

    EXPECT_NEAR(mod->GetAccSec(), 63 * 0.016f - 1.0f, 0.005f);
}

TEST(DiaAIInspectorModule_Budget, OldestFirst_LogicalIndex0HasSmallestFrameIndex)
{
    auto mod       = std::make_unique<AIInspectorModuleTestable>();
    auto budgetMod = std::make_unique<AIBudgetModuleStub>();
    mod->SetBudgetModule(budgetMod.get());

    for (int i = 0; i < 70; ++i)
        mod->PushBudget(0.016f);

    ASSERT_EQ(mod->GetHistoryCount(), 60);
    for (int i = 0; i < 59; ++i)
        EXPECT_LT(mod->GetHistoryEntry(i).frameIndex, mod->GetHistoryEntry(i + 1).frameIndex);
}

TEST(DiaAIInspectorModule_Budget, FrameFields_BudgetMsFromConfigure)
{
    auto mod       = std::make_unique<AIInspectorModuleTestable>();
    auto budgetMod = std::make_unique<AIBudgetModuleStub>();
    budgetMod->Configure("{\"budgetUs\":2000}");
    mod->SetBudgetModule(budgetMod.get());
    mod->PushBudget(0.016f);

    ASSERT_EQ(mod->GetHistoryCount(), 1);
    const auto& frame = mod->GetHistoryEntry(0);

    // GetLastResult() is zero-initialized (DoUpdate not called).
    EXPECT_EQ(frame.systemsRun,      0);
    EXPECT_EQ(frame.systemsDeferred, 0);
    EXPECT_FLOAT_EQ(frame.budgetMs,  2.0f);
}

#endif // DIA_DEBUG
