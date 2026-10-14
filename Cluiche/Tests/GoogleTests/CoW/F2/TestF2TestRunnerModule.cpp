// Shell tests for CoW::F2TestRunnerModule: phase order, initial state, and bus events.
#include <gtest/gtest.h>
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

Module* CreateBus(const StringCRC& id) { g_bus = new Cluiche::AppFlow::MessageBusHostModule(id); return g_bus; }
Module* CreateRunner(const StringCRC& id) { g_runner = new F2TestRunnerModule(id); return g_runner; }

constexpr float kDt = 1.0f / 30.0f;

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    F2TestRunnerModule* runner = nullptr;
    Dia::MessageBus::BusSubscriptionHandle phaseSub, completedSub;
    std::vector<F2PhaseId> phases;
    std::vector<F2ScenarioCompleted> completions;

    Fixture()
    {
        reg.Register(StringCRC("MessageBusHostModule"), &CreateBus);
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

        ModuleDeclaration decl;
        decl.instanceId = StringCRC("F2TestRunnerModule");
        decl.typeId = StringCRC("F2TestRunnerModule");
        decl.stages.Add(StringCRC("all"));
        decl.dependencies.Add(StringCRC("MessageBusHostModule"));
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
                return phaseSub.IsValid() && completedSub.IsValid();
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
    EXPECT_EQ(f.completions[0].passCount, 0u);
    EXPECT_EQ(f.completions[0].failCount, 0u);
    EXPECT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::Idle);
    for (int i = 0; i < kF2CheckCount; ++i)
        EXPECT_EQ(f.runner->GetCheckStatus(static_cast<F2CheckId>(i)), F2CheckStatus::NotRun);
}
