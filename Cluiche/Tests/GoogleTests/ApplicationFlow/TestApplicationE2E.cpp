////////////////////////////////////////////////////////////////////////////////
// Filename: TestApplicationE2E.cpp
// GoogleTest suite — DiaApplicationFlow v2 end-to-end lifecycle
//
// Exercises the realistic boot → run → transition → shutdown flow that unit
// tests in TestModuleLifecycle / TestStageSystem cover only in isolation.
// Replaces the end-to-end coverage previously in the deleted v1 tests
// (TestApplicationLifecycle / TestBasicApplication / TestApplicationLoader).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Trackable module — records every lifecycle call so the test can assert on
// the exact sequence of DoStart/DoUpdate/DoStop across the application run.
// ---------------------------------------------------------------------------

struct E2E_TrackableModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    int startCalls  = 0;
    int updateCalls = 0;
    int stopCalls   = 0;

    StartResult DoStart() override  { ++startCalls;  return StartResult::kReady; }
    void        DoUpdate(float)  override { ++updateCalls; }
    StopResult  DoStop()  override  { ++stopCalls;   return StopResult::kDone; }
};
const StringCRC E2E_TrackableModule::kTypeId("E2E_TrackableModule");

// File-scope pointers let the factory capture the two module instances for
// assertions without needing capturing lambdas (the factory is a plain fn ptr).
static E2E_TrackableModule* g_bootModule = nullptr;
static E2E_TrackableModule* g_runModule  = nullptr;

static Module* CreateBootModule(const StringCRC& id)
{
    g_bootModule = new E2E_TrackableModule(id);
    return g_bootModule;
}
static Module* CreateRunModule(const StringCRC& id)
{
    g_runModule = new E2E_TrackableModule(id);
    return g_runModule;
}

// ---------------------------------------------------------------------------
// Helper — build a 2-stage manifest with one module per stage.
// Boot stage has bootMod; Run stage has runMod.  Not autoStage, so the test
// drives the Boot → Run transition explicitly via TransitionTo.
// ---------------------------------------------------------------------------

static ApplicationManifestV2 BuildTwoStageManifest()
{
    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;  boot.name = StringCRC("Boot");
    StageDeclaration run;   run.name  = StringCRC("Run");
    manifest.stages.Add(boot);
    manifest.stages.Add(run);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration bootMod;
    bootMod.instanceId = StringCRC("bootMod");
    bootMod.typeId     = StringCRC("E2E_BootMod");
    bootMod.stages.Add(StringCRC("Boot"));
    pu.modules.Add(bootMod);

    ModuleDeclaration runMod;
    runMod.instanceId = StringCRC("runMod");
    runMod.typeId     = StringCRC("E2E_RunMod");
    runMod.stages.Add(StringCRC("Run"));
    pu.modules.Add(runMod);

    manifest.processingUnits.Add(pu);
    return manifest;
}

// Pump app.Update(dt) until predicate returns true or safety limit reached.
template<typename Pred>
static bool PumpUntil(Application& app, Pred pred, int safetyLimit = 100)
{
    for (int i = 0; i < safetyLimit; ++i)
    {
        if (pred()) return true;
        app.Update(1.0f / 60.0f);
    }
    return pred();
}

// ---------------------------------------------------------------------------
// The E2E test
// ---------------------------------------------------------------------------

TEST(ApplicationE2E, BootRunTransitionShutdown)
{
    g_bootModule = nullptr;
    g_runModule  = nullptr;

    TypeRegistry reg;
    reg.Register(StringCRC("E2E_BootMod"), &CreateBootModule);
    reg.Register(StringCRC("E2E_RunMod"),  &CreateRunModule);

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);

    // --- Start: manifest validated, PUs built, initial stage entered.
    ASSERT_TRUE(app.Start());
    EXPECT_EQ(app.GetCurrentStage(), StringCRC("Boot"));

    // --- Pump until bootMod is active.
    ASSERT_NE(g_bootModule, nullptr) << "Factory should have created bootMod during Start";
    ASSERT_NE(g_runModule,  nullptr) << "Factory should have created runMod during Start";

    ASSERT_TRUE(PumpUntil(app, [] { return g_bootModule->startCalls > 0; }, 5))
        << "bootMod should have received DoStart within a few frames";
    EXPECT_EQ(g_bootModule->GetState(), ModuleState::kActive);
    EXPECT_EQ(g_runModule->GetState(),  ModuleState::kInactive);

    // --- Run N frames in Boot stage; bootMod updates, runMod does not.
    const int framesBeforeTransition = 3;
    const int updatesBeforeTransition = g_bootModule->updateCalls;
    for (int i = 0; i < framesBeforeTransition; ++i)
        app.Update(1.0f / 60.0f);

    EXPECT_GE(g_bootModule->updateCalls, updatesBeforeTransition + framesBeforeTransition);
    EXPECT_EQ(g_runModule->updateCalls, 0) << "runMod should not update while in Boot stage";

    // --- Queue transition to Run.  Stops bootMod (it's not in Run), starts runMod.
    app.TransitionTo(StringCRC("Run"));
    ASSERT_TRUE(PumpUntil(app,
        [] { return g_runModule->GetState() == ModuleState::kActive; }, 10))
        << "runMod should reach Active after TransitionTo(Run)";

    EXPECT_EQ(app.GetCurrentStage(), StringCRC("Run"));
    EXPECT_EQ(g_bootModule->GetState(), ModuleState::kInactive)
        << "bootMod should be stopped after leaving Boot stage";
    EXPECT_GE(g_bootModule->stopCalls, 1);

    // --- Run a few frames in Run stage; runMod updates.
    const int runUpdatesBeforeShutdown = g_runModule->updateCalls;
    for (int i = 0; i < framesBeforeTransition; ++i)
        app.Update(1.0f / 60.0f);
    EXPECT_GE(g_runModule->updateCalls, runUpdatesBeforeShutdown + framesBeforeTransition);

    // --- RequestShutdown: stops all active modules, then Update returns false.
    app.RequestShutdown();
    EXPECT_TRUE(app.IsShuttingDown());

    bool shutdownDrained = false;
    for (int i = 0; i < 100; ++i)
    {
        if (!app.Update(1.0f / 60.0f))
        {
            shutdownDrained = true;
            break;
        }
    }
    EXPECT_TRUE(shutdownDrained) << "Update should return false once shutdown drains";

    // --- Final assertions: runMod was stopped; both modules saw at least
    // one Start and one Stop across the entire run.
    EXPECT_GE(g_bootModule->startCalls, 1);
    EXPECT_GE(g_bootModule->stopCalls,  1);
    EXPECT_GE(g_runModule->startCalls,  1);
    EXPECT_GE(g_runModule->stopCalls,   1);
}
