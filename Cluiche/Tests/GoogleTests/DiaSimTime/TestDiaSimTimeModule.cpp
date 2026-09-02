////////////////////////////////////////////////////////////////////////////////
// Filename: TestDiaSimTimeModule.cpp
// GoogleTest suite — DiaSimTime Task 4.4: DiaSimTimeModule (the umbrella SimPU
// module that assembles scheduler + budget + registry into Pattern C's per-tick
// gate loop and publishes SimTimeContext onto the "SimTime" FrameStream).
//
// Unlike the sibling classes' standalone tests, this needs a real
// ProcessingUnit / Application to exercise DoUpdate — the SimModule seals
// DoUpdate(float) and pulls the SimTimeContext the owning PU cached this
// Update(), so it can only be driven through a live single-PU Application (the
// pattern established by TestProcessingUnitAccumulator's Application-driven
// lifecycle cases and TestDiaRenderTime).
//
// Coverage:
//   1. Full tick sequence: a registered ISimTimeBudgetedSystem's UpdateBudgeted
//      is called when it is awake and due.
//   2. A kSleeping system (via Sleep()) is skipped — UpdateBudgeted not called.
//   3. A tier-throttled (kHigh) system is skipped between runs, then runs again
//      once its tier interval elapses.
//   4. SimTimeContext is actually published: a sibling StreamReader on the same
//      "SimTime" stream observes it via FetchLatest() after a DoUpdate.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaSimTime/DiaSimTimeModule.h>
#include <DiaSimTime/ISimTimeBudgetedSystem.h>
#include <DiaSimTime/SimTimePolicy.h>
#include <DiaSimTime/SimTimeState.h>
#include <DiaSimTime/SimTimeTier.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/StreamReader.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;
using Dia::SimTime::DiaSimTimeModule;
using Dia::SimTime::ISimTimeBudgetedSystem;
using Dia::SimTime::SimTimePolicy;
using Dia::SimTime::SimTimeState;
using Dia::SimTime::SimTimeTier;
using Dia::SimTime::SimTimeContext;

namespace {

    constexpr float kFixed60 = 1.0f / 60.0f;

    // ---------------------------------------------------------------------------
    // Minimal budgeted system: identity + a counting no-op budgeted update.
    // ---------------------------------------------------------------------------
    class CountingSystem : public ISimTimeBudgetedSystem
    {
    public:
        explicit CountingSystem(const char* id) : mId(id) {}
        StringCRC GetSystemId() const override { return mId; }
        void UpdateBudgeted(float /*budgetMs*/) override { ++updateCalls; }

        int updateCalls = 0;
    private:
        StringCRC mId;
    };

    // ---------------------------------------------------------------------------
    // Sibling SimModule that reads the "SimTime" FrameStream — stands in for a
    // real reader (DiaRenderTime lives on RenderPU). Placed AFTER DiaSimTimeModule
    // in the SimPU module array so it observes the same-tick publish.
    // ---------------------------------------------------------------------------
    struct SimTimeReaderModule : SimModule
    {
        using SimModule::SimModule;
        static const StringCRC kTypeId;

        StreamReader<SimTimeContext> mReader{ this, StringCRC("SimTime") };
        bool               observed = false;
        SimTimeContext     lastCtx{ TimeAbsolute::Zero(), TimeRelative::Zero(), 0, 1.0f, false };

        void OnConnectStreams(Application& app) override { mReader.Connect(app); }
        StartResult DoStart() override { return StartResult::kReady; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void DoUpdate(const SimTimeContext&) override
        {
            const SimTimeContext* c = mReader.FetchLatest();
            if (c) { observed = true; lastCtx = *c; }
        }
    };
    const StringCRC SimTimeReaderModule::kTypeId("STM_ReaderModule");

    DiaSimTimeModule*    g_module = nullptr;
    SimTimeReaderModule* g_reader = nullptr;

    Module* CreateModule(const StringCRC& id) { auto* m = new DiaSimTimeModule(id); g_module = m; return m; }
    Module* CreateReader(const StringCRC& id) { auto* m = new SimTimeReaderModule(id); g_reader = m; return m; }

    ModuleDeclaration MakeModule(const StringCRC& typeId, const StringCRC& instanceId)
    {
        ModuleDeclaration mod;
        mod.instanceId     = instanceId;
        mod.typeId         = typeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        return mod;
    }

    // Builds a single SimPU manifest. If withReader, adds SimTimeReaderModule
    // after the DiaSimTimeModule.
    ApplicationManifestV3 BuildManifest(bool withReader)
    {
        ApplicationManifestV3 manifest;
        manifest.version = 3;

        StageDeclaration boot;
        boot.name = StringCRC("Boot");
        manifest.stages.Add(boot);
        manifest.initialStage = StringCRC("Boot");

        StreamDeclaration simTime;
        simTime.id   = StringCRC("SimTime");
        simTime.kind = StringCRC("FrameStream");
        manifest.streams.Add(simTime);

        StreamDeclaration fire;
        fire.id   = StringCRC("SimTimeSchedulerFire");
        fire.kind = StringCRC("EventStream");
        manifest.streams.Add(fire);

        ProcessingUnitDeclaration pu;
        pu.instanceId      = StringCRC("SimPU");
        pu.frequencyHz     = 60.0f;
        pu.dedicatedThread = false;   // ticked inline by Application::Update
        pu.modules.Add(MakeModule(DiaSimTimeModule::kTypeId, StringCRC("simTime")));
        if (withReader)
            pu.modules.Add(MakeModule(SimTimeReaderModule::kTypeId, StringCRC("reader")));
        manifest.processingUnits.Add(pu);
        return manifest;
    }

    bool PumpUntilActive(Application& app, int limit = 100)
    {
        for (int i = 0; i < limit; ++i)
        {
            DynamicArrayC<ModuleStateInfo, 64> infos;
            app.GetActiveModules(StringCRC("SimPU"), infos);
            if (infos.Size() > 0)
            {
                bool allActive = true;
                for (unsigned int m = 0; m < infos.Size(); ++m)
                    if (infos[m].state != ModuleState::kActive) { allActive = false; break; }
                if (allActive) return true;
            }
            app.Update(kFixed60);
        }
        return false;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// 1. A registered, awake, immediate-tier system's UpdateBudgeted is called.
// ---------------------------------------------------------------------------
TEST(DiaSimTimeModuleTest, RegisteredSystemIsUpdatedThroughGateLoop)
{
    g_module = nullptr;
    TypeRegistry reg;
    reg.Register(DiaSimTimeModule::kTypeId, CreateModule);

    auto manifest = BuildManifest(/*withReader*/ false);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(PumpUntilActive(app));
    ASSERT_NE(g_module, nullptr);

    CountingSystem sys("STM_ImmediateSys");
    SimTimePolicy policy;                        // default tier == kImmediate
    g_module->Register(&sys, policy);

    for (int i = 0; i < 5 && sys.updateCalls == 0; ++i)
        app.Update(kFixed60);

    EXPECT_GT(sys.updateCalls, 0)
        << "An awake, immediate-tier registered system must be driven via UpdateBudgeted";

    g_module->Unregister(&sys);   // avoid dangling pointer past this scope
}

// ---------------------------------------------------------------------------
// 2. A kSleeping system is skipped — UpdateBudgeted is never called.
// ---------------------------------------------------------------------------
TEST(DiaSimTimeModuleTest, SleepingSystemIsSkipped)
{
    g_module = nullptr;
    TypeRegistry reg;
    reg.Register(DiaSimTimeModule::kTypeId, CreateModule);

    auto manifest = BuildManifest(false);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(PumpUntilActive(app));
    ASSERT_NE(g_module, nullptr);

    CountingSystem sys("STM_SleeperSys");
    g_module->Register(&sys, SimTimePolicy{});
    g_module->Sleep(sys.GetSystemId());
    ASSERT_EQ(g_module->GetState(sys.GetSystemId()), SimTimeState::kSleeping);

    for (int i = 0; i < 5; ++i)
        app.Update(kFixed60);

    EXPECT_EQ(sys.updateCalls, 0)
        << "A sleeping system must be skipped by the gate loop (step 4a)";

    g_module->Unregister(&sys);
}

// ---------------------------------------------------------------------------
// 3. A kHigh (~30Hz) system is throttled: it is skipped between runs, then runs
//    again once its tier interval (> one 60Hz fixed step) has elapsed. A
//    co-registered kImmediate "ticker" gives an exact DoUpdate count.
// ---------------------------------------------------------------------------
TEST(DiaSimTimeModuleTest, TierThrottledSystemSkipsThenRunsWhenDue)
{
    g_module = nullptr;
    TypeRegistry reg;
    reg.Register(DiaSimTimeModule::kTypeId, CreateModule);

    auto manifest = BuildManifest(false);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(PumpUntilActive(app));
    ASSERT_NE(g_module, nullptr);

    CountingSystem ticker("STM_TickerSys");   // kImmediate — runs every DoUpdate
    CountingSystem high("STM_HighSys");
    SimTimePolicy highPolicy; highPolicy.tier = SimTimeTier::kHigh;   // ~33ms > 16.7ms step
    g_module->Register(&ticker, SimTimePolicy{});
    g_module->Register(&high,   highPolicy);

    int  prevTicker      = ticker.updateCalls;
    int  prevHigh        = high.updateCalls;
    bool sawSkipAfterRun = false;
    bool sawRunAfterSkip = false;

    for (int i = 0; i < 60 && !sawRunAfterSkip; ++i)
    {
        app.Update(kFixed60);
        const int dTick = ticker.updateCalls - prevTicker;
        const int dHigh = high.updateCalls   - prevHigh;
        if (dTick == 1)   // exactly one DoUpdate this call — classify it
        {
            if (prevHigh >= 1 && dHigh == 0)              sawSkipAfterRun = true;
            else if (sawSkipAfterRun && dHigh == 1)       sawRunAfterSkip = true;
        }
        prevTicker = ticker.updateCalls;
        prevHigh   = high.updateCalls;
    }

    EXPECT_TRUE(sawSkipAfterRun)
        << "A kHigh system must be throttled (skipped) on ticks within its interval";
    EXPECT_TRUE(sawRunAfterSkip)
        << "A kHigh system must run again once its tier interval elapses";
    EXPECT_LT(high.updateCalls, ticker.updateCalls)
        << "The throttled system must run strictly fewer times than the immediate ticker";

    g_module->Unregister(&ticker);
    g_module->Unregister(&high);
}

// ---------------------------------------------------------------------------
// 4. SimTimeContext is published to the "SimTime" FrameStream: a sibling reader
//    module observes it via FetchLatest() after DoUpdate.
// ---------------------------------------------------------------------------
TEST(DiaSimTimeModuleTest, PublishesSimTimeContextToFrameStream)
{
    g_module = nullptr;
    g_reader = nullptr;
    TypeRegistry reg;
    reg.Register(DiaSimTimeModule::kTypeId, CreateModule);
    reg.Register(SimTimeReaderModule::kTypeId, CreateReader);

    auto manifest = BuildManifest(/*withReader*/ true);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(PumpUntilActive(app));
    ASSERT_NE(g_module, nullptr);
    ASSERT_NE(g_reader, nullptr);

    // Drive several ticks so the sim world clock advances and the module writes.
    for (int i = 0; i < 5; ++i)
        app.Update(kFixed60);

    EXPECT_TRUE(g_reader->observed)
        << "The sibling reader must observe a SimTimeContext published to the SimTime stream";
    EXPECT_TRUE(g_reader->lastCtx.gameTime > TimeAbsolute::Zero())
        << "The published SimTimeContext must carry the advancing sim game clock";
}
