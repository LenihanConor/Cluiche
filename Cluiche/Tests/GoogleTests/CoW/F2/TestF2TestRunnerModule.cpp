// Shell tests for CoW::F2TestRunnerModule: phase order, initial state, and bus events.
#include <gtest/gtest.h>
#include <Modules/RandomStreamModule.h>   // before Application headers: RandomStream.h needs std::min/max unmacroed
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <Modules/F2TestRunnerModule.h>
#include <Modules/MessageBusHostModule.h>
#include <memory>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace CoW;

namespace {

Cluiche::AppFlow::MessageBusHostModule* g_bus = nullptr;
F2TestRunnerModule* g_runner = nullptr;
RandomStreamModule* g_random = nullptr;

Module* CreateBus(const StringCRC& id) { g_bus = new Cluiche::AppFlow::MessageBusHostModule(id); return g_bus; }
Module* CreateRunner(const StringCRC& id) { g_runner = new F2TestRunnerModule(id); return g_runner; }

Module* CreateRandom(const StringCRC& id) { g_random = new RandomStreamModule(id); return g_random; }

constexpr float kDt = 1.0f / 30.0f;

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    F2TestRunnerModule* runner = nullptr;
    Dia::MessageBus::BusSubscriptionHandle phaseSub, completedSub, checkSub;
    std::vector<F2CheckCompleted> checks;
    std::vector<F2PhaseId> phases;
    std::vector<F2ScenarioCompleted> completions;

    Fixture()
    {
        reg.Register(StringCRC("MessageBusHostModule"), &CreateBus);
        reg.Register(StringCRC("RandomStreamModule"), &CreateRandom);
        reg.Register(StringCRC("F2TestRunnerModule"), &CreateRunner);

        manifest.version = 3;
        StageDeclaration run; run.name = StringCRC("Run");
        manifest.stages.Add(run);
        manifest.initialStage = StringCRC("Run");

        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("SimPU");
        pu.frequencyHz = 30.0f;
        pu.dedicatedThread = false;

        ModuleDeclaration busDecl;
        busDecl.instanceId = StringCRC("MessageBusHostModule");
        busDecl.typeId = StringCRC("MessageBusHostModule");
        busDecl.stages.Add(StringCRC("all"));
        pu.modules.Add(busDecl);

        ModuleDeclaration randDecl;
        randDecl.instanceId = StringCRC("RandomStreamModule");
        randDecl.typeId = StringCRC("RandomStreamModule");
        randDecl.stages.Add(StringCRC("all"));
        pu.modules.Add(randDecl);

        ModuleDeclaration decl;
        decl.instanceId = StringCRC("F2TestRunnerModule");
        decl.typeId = StringCRC("F2TestRunnerModule");
        decl.stages.Add(StringCRC("all"));
        decl.dependencies.Add(StringCRC("MessageBusHostModule"));
        decl.dependencies.Add(StringCRC("RandomStreamModule"));
        pu.modules.Add(decl);

        manifest.processingUnits.Add(pu);
    }

    ~Fixture()
    {
        if (app)
        {
            app->RequestShutdown();
            for (int i = 0; i < 50 && app->Update(kDt); ++i) {}
        }
    }

    bool Boot()
    {
        g_bus = nullptr;
        g_runner = nullptr;
        g_random = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start() || !g_bus || !g_runner)
            return false;
        runner = g_runner;
        for (int i = 0; i < 20; ++i)
        {
            app->Update(kDt);
            if (runner->GetState() == ModuleState::kActive)
            {
                phaseSub = g_bus->GetBus().Subscribe<F2PhaseEntered>(StringCRC("TestPhaseSink"),
                    [this](const F2PhaseEntered& m) { phases.push_back(m.phase); });
                completedSub = g_bus->GetBus().Subscribe<F2ScenarioCompleted>(StringCRC("TestDoneSink"),
                    [this](const F2ScenarioCompleted& m) { completions.push_back(m); });
                checkSub = g_bus->GetBus().Subscribe<F2CheckCompleted>(StringCRC("TestCheckSink"),
                    [this](const F2CheckCompleted& m) { checks.push_back(m); });
                return checkSub.IsValid() && phaseSub.IsValid() && completedSub.IsValid();
            }
        }
        return false;
    }

    void Pump(int frames) { for (int i = 0; i < frames; ++i) app->Update(kDt); }
};

} // namespace

TEST(F2TestRunnerModule, ChecksStartNotRun)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    for (int i = 0; i < kF2CheckCount; ++i)
    {
        EXPECT_EQ(f.runner->GetCheckStatus(static_cast<F2CheckId>(i)), F2CheckStatus::NotRun) << i;
        EXPECT_STREQ(f.runner->GetLastResult(static_cast<F2CheckId>(i)), "") << i;
    }
}

TEST(F2TestRunnerModule, PhaseStartsIdle)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(5);
    EXPECT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::Idle);
    EXPECT_TRUE(f.phases.empty());
}

TEST(F2TestRunnerModule, PhasesVisitedInOrder)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    EXPECT_FALSE(f.runner->RunScenario());   // already in progress

    f.Pump(1);
    EXPECT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::Isolation);
    for (int expected = 1; expected < kF2PhaseCount; ++expected)
    {
        f.Pump(1);
        EXPECT_EQ(static_cast<int>(f.runner->GetCurrentPhase()), expected);
    }
}

TEST(F2TestRunnerModule, EventsFireInOrderAndScenarioCompletesOnce)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    f.Pump(20);

    ASSERT_EQ(f.phases.size(), 8u);
    for (int i = 0; i < kF2PhaseCount; ++i)
        EXPECT_EQ(static_cast<int>(f.phases[i]), i);

    ASSERT_EQ(f.completions.size(), 1u);
    EXPECT_EQ(f.completions[0].failCount, 0u);
    EXPECT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::Idle);
    // Only C1 is implemented so far; the other five stay NotRun.
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::StreamIndependence), F2CheckStatus::Pass);
    EXPECT_EQ(f.completions[0].passCount, 1u);
    for (int i = 1; i < kF2CheckCount; ++i)
        EXPECT_EQ(f.runner->GetCheckStatus(static_cast<F2CheckId>(i)), F2CheckStatus::NotRun) << i;
}

TEST(F2TestRunnerModule, IsolationResolvesC1StreamIndependence)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    ASSERT_NE(g_random, nullptr);

    // Snapshot live stream state; the check must not disturb it.
    uint64_t counts[RandomStreamModule::kStreamCount];
    for (int i = 0; i < RandomStreamModule::kStreamCount; ++i)
        counts[i] = g_random->GetStreamByName(StringCRC(RandomStreamModule::kStreamNames[i]))->GetDrawCount();

    ASSERT_TRUE(f.runner->RunScenario());
    f.Pump(2);   // Isolation runs on the first update; bus delivery may lag a frame

    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::StreamIndependence), F2CheckStatus::Pass);
    EXPECT_STREQ(f.runner->GetLastResult(F2CheckId::StreamIndependence), "8 / 8 streams independent over 64 draws");
    ASSERT_EQ(f.checks.size(), 1u);
    EXPECT_EQ(f.checks[0].check, F2CheckId::StreamIndependence);
    EXPECT_TRUE(f.checks[0].passed);
    EXPECT_STREQ(f.checks[0].summary, "8 / 8 streams independent over 64 draws");

    for (int i = 0; i < RandomStreamModule::kStreamCount; ++i)
        EXPECT_EQ(g_random->GetStreamByName(StringCRC(RandomStreamModule::kStreamNames[i]))->GetDrawCount(), counts[i]) << i;
}
