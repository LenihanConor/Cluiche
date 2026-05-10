////////////////////////////////////////////////////////////////////////////////
// Filename: TestStageSystem.cpp
// GoogleTest suite — DiaApplicationFlow v2 Stage system
//
// Tests stage transitions using an Application with 2 stages and modules
// assigned to each.  All tests use a local TypeRegistry.
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
// Fixture module — tracks calls, always returns kReady / kDone
// ---------------------------------------------------------------------------
struct StageTestModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;
    StartResult DoStart() override { return StartResult::kReady; }
    void        DoUpdate(float) override {}
    StopResult  DoStop() override { return StopResult::kDone; }
};
const StringCRC StageTestModule::kTypeId("StageTestModule");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Add a module declaration to a PU declaration.
static void AddModuleToDecl(ProcessingUnitDeclaration& pu,
                             const char* instanceId,
                             const char* typeId,
                             const char* stageName)
{
    ModuleDeclaration mod;
    mod.instanceId     = StringCRC(instanceId);
    mod.typeId         = StringCRC(typeId);
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC(stageName));
    pu.modules.Add(mod);
}

// Add an "all"-stage module to a PU declaration.
static void AddAllStageModuleToDecl(ProcessingUnitDeclaration& pu,
                                     const char* instanceId,
                                     const char* typeId)
{
    ModuleDeclaration mod;
    mod.instanceId     = StringCRC(instanceId);
    mod.typeId         = StringCRC(typeId);
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("all"));
    pu.modules.Add(mod);
}

// Pump Update() until all modules in puId have a state other than kStarting,
// up to safetyLimit iterations.
static void PumpUntilSettled(Application& app,
                              const StringCRC& puId,
                              int safetyLimit = 100)
{
    for (int i = 0; i < safetyLimit; ++i)
    {
        DynamicArrayC<ModuleStateInfo, 64> infos;
        app.GetActiveModules(puId, infos);
        bool settled = true;
        for (unsigned int m = 0; m < infos.Size(); ++m)
        {
            if (infos[m].state == ModuleState::kStarting ||
                infos[m].state == ModuleState::kStopping)
            {
                settled = false;
                break;
            }
        }
        if (settled)
            return;
        app.Update(1.0f / 60.0f);
    }
}

// Find a module's state in the inspectable output.
static ModuleState FindModuleState(Application& app,
                                    const StringCRC& puId,
                                    const StringCRC& moduleId)
{
    DynamicArrayC<ModuleStateInfo, 64> infos;
    app.GetActiveModules(puId, infos);
    for (unsigned int i = 0; i < infos.Size(); ++i)
    {
        if (infos[i].instanceId == moduleId)
            return infos[i].state;
    }
    // Module not found in the list — treat as kInactive.
    return ModuleState::kInactive;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// After Start(), only modules assigned to the initial stage are kActive.
// Modules assigned to the other stage remain kInactive.
TEST(StageSystem, InitialStageSetsCorrectModulesActive)
{
    TypeRegistry reg;
    reg.Register(StageTestModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new StageTestModule(id); });

    // Build manifest: stages "Boot", "Game"
    // modA — Boot, modB — Game, modC — all
    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration bootStage, gameStage;
    bootStage.name = StringCRC("Boot");
    gameStage.name = StringCRC("Game");
    manifest.stages.Add(bootStage);
    manifest.stages.Add(gameStage);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    AddModuleToDecl(pu, "modA", "StageTestModule", "Boot");
    AddModuleToDecl(pu, "modB", "StageTestModule", "Game");
    AddAllStageModuleToDecl(pu, "modC", "StageTestModule");

    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    // Pump until settled
    PumpUntilSettled(app, StringCRC("MainPU"), 50);
    // One more Update to make sure Start ticks complete
    app.Update(1.0f / 60.0f);
    PumpUntilSettled(app, StringCRC("MainPU"), 50);

    EXPECT_EQ(FindModuleState(app, StringCRC("MainPU"), StringCRC("modA")),
              ModuleState::kActive) << "modA should be kActive in Boot stage";

    EXPECT_EQ(FindModuleState(app, StringCRC("MainPU"), StringCRC("modB")),
              ModuleState::kInactive) << "modB should be kInactive (not in Boot)";

    EXPECT_EQ(FindModuleState(app, StringCRC("MainPU"), StringCRC("modC")),
              ModuleState::kActive) << "modC (all) should be kActive in any stage";
}

// After TransitionTo("Game"), modA becomes kInactive, modB becomes kActive,
// and modC (all) stays kActive.
TEST(StageSystem, TransitionStopsOldStartsNew)
{
    TypeRegistry reg;
    reg.Register(StageTestModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new StageTestModule(id); });

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration bootStage, gameStage;
    bootStage.name = StringCRC("Boot");
    gameStage.name = StringCRC("Game");
    manifest.stages.Add(bootStage);
    manifest.stages.Add(gameStage);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    AddModuleToDecl(pu, "modA", "StageTestModule", "Boot");
    AddModuleToDecl(pu, "modB", "StageTestModule", "Game");
    AddAllStageModuleToDecl(pu, "modC", "StageTestModule");

    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    // Wait for Boot stage to stabilise.
    PumpUntilSettled(app, StringCRC("MainPU"), 50);
    app.Update(1.0f / 60.0f);
    PumpUntilSettled(app, StringCRC("MainPU"), 50);

    // Verify modA is active before transition.
    ASSERT_EQ(FindModuleState(app, StringCRC("MainPU"), StringCRC("modA")),
              ModuleState::kActive);

    // Trigger transition to Game.
    app.TransitionTo(StringCRC("Game"));

    // Pump until the full transition completes:
    //   frame 1: stop phase begins (modA goes kStopping)
    //   frame 2: modA reaches kInactive, drain commits, modB begins kStarting
    //   frame 3: modB reaches kActive, IsTransitioning() clears
    for (int i = 0; i < 100; ++i)
    {
        if (!app.IsTransitioning())
        {
            // Also check no module is mid-state
            DynamicArrayC<ModuleStateInfo, 64> infos;
            app.GetActiveModules(StringCRC("MainPU"), infos);
            bool settled = true;
            for (unsigned int m = 0; m < infos.Size(); ++m)
            {
                if (infos[m].state == ModuleState::kStarting ||
                    infos[m].state == ModuleState::kStopping)
                {
                    settled = false;
                    break;
                }
            }
            if (settled)
                break;
        }
        app.Update(1.0f / 60.0f);
    }

    EXPECT_EQ(FindModuleState(app, StringCRC("MainPU"), StringCRC("modA")),
              ModuleState::kInactive) << "modA should be kInactive after leaving Boot";

    EXPECT_EQ(FindModuleState(app, StringCRC("MainPU"), StringCRC("modB")),
              ModuleState::kActive) << "modB should be kActive in Game stage";

    EXPECT_EQ(FindModuleState(app, StringCRC("MainPU"), StringCRC("modC")),
              ModuleState::kActive) << "modC (all) should remain kActive";
}

// TransitionTo the same stage that is already current — no modules change state.
TEST(StageSystem, SameStageTransitionIsNoop)
{
    TypeRegistry reg;
    reg.Register(StageTestModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new StageTestModule(id); });

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration bootStage, gameStage;
    bootStage.name = StringCRC("Boot");
    gameStage.name = StringCRC("Game");
    manifest.stages.Add(bootStage);
    manifest.stages.Add(gameStage);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    AddModuleToDecl(pu, "modA", "StageTestModule", "Boot");
    AddModuleToDecl(pu, "modB", "StageTestModule", "Game");

    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    PumpUntilSettled(app, StringCRC("MainPU"), 50);
    app.Update(1.0f / 60.0f);
    PumpUntilSettled(app, StringCRC("MainPU"), 50);

    ASSERT_EQ(FindModuleState(app, StringCRC("MainPU"), StringCRC("modA")),
              ModuleState::kActive);

    // Transition to Boot — same stage we're already in.
    app.TransitionTo(StringCRC("Boot"));
    app.Update(1.0f / 60.0f);
    PumpUntilSettled(app, StringCRC("MainPU"), 50);

    // modA should still be kActive; modB should still be kInactive.
    EXPECT_EQ(FindModuleState(app, StringCRC("MainPU"), StringCRC("modA")),
              ModuleState::kActive) << "Same-stage transition should not stop modA";
    EXPECT_EQ(FindModuleState(app, StringCRC("MainPU"), StringCRC("modB")),
              ModuleState::kInactive) << "Same-stage transition should not start modB";
}

// GetCurrentStage() updates after a completed transition.
TEST(StageSystem, GetCurrentStageUpdatesAfterTransition)
{
    TypeRegistry reg;
    reg.Register(StageTestModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new StageTestModule(id); });

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration bootStage, gameStage;
    bootStage.name = StringCRC("Boot");
    gameStage.name = StringCRC("Game");
    manifest.stages.Add(bootStage);
    manifest.stages.Add(gameStage);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    AddModuleToDecl(pu, "modA", "StageTestModule", "Boot");
    AddModuleToDecl(pu, "modB", "StageTestModule", "Game");

    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    PumpUntilSettled(app, StringCRC("MainPU"), 50);

    EXPECT_EQ(app.GetCurrentStage(), StringCRC("Boot"));

    app.TransitionTo(StringCRC("Game"));

    // Pump until the transition drain completes: stop phase runs frame 1,
    // stage commits frame 2 once outgoing modules are kInactive.
    for (int i = 0; i < 50 && app.IsTransitioning(); ++i)
        app.Update(1.0f / 60.0f);

    EXPECT_EQ(app.GetCurrentStage(), StringCRC("Game"));
}
