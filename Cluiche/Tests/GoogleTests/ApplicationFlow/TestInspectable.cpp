////////////////////////////////////////////////////////////////////////////////
// Filename: TestInspectable.cpp
// GoogleTest suite — DiaApplicationFlow v2 IApplicationInspectable
//
// Tests the read-only introspection interface exposed by Application.
// All tests use a local TypeRegistry.
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
// Fixture module
// ---------------------------------------------------------------------------

struct InspectModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;
    StartResult DoStart() override { return StartResult::kReady; }
    void        DoUpdate(float) override {}
    StopResult  DoStop() override { return StopResult::kDone; }
};
const StringCRC InspectModule::kTypeId("InspectModule");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ApplicationManifestV2 BuildTwoStageManifest()
{
    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot, game;
    boot.name = StringCRC("Boot");
    game.name = StringCRC("Game");
    manifest.stages.Add(boot);
    manifest.stages.Add(game);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration modA;
    modA.instanceId     = StringCRC("modA");
    modA.typeId         = InspectModule::kTypeId;
    modA.startTimeoutMs = 10000.0f;
    modA.stopTimeoutMs  = 5000.0f;
    modA.stages.Add(StringCRC("Boot"));
    pu.modules.Add(modA);

    ModuleDeclaration modB;
    modB.instanceId     = StringCRC("modB");
    modB.typeId         = InspectModule::kTypeId;
    modB.startTimeoutMs = 10000.0f;
    modB.stopTimeoutMs  = 5000.0f;
    modB.stages.Add(StringCRC("Game"));
    pu.modules.Add(modB);

    manifest.processingUnits.Add(pu);
    return manifest;
}

static void PumpUntilSettled(Application& app, const StringCRC& puId, int limit = 100)
{
    for (int i = 0; i < limit; ++i)
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

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(Inspectable, GetCurrentStageMatchesInitialStage)
{
    TypeRegistry reg;
    reg.Register(InspectModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new InspectModule(id); });

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    EXPECT_EQ(app.GetCurrentStage(), StringCRC("Boot"));
}

TEST(Inspectable, GetAllStagesReturnsAllDeclaredStages)
{
    TypeRegistry reg;
    reg.Register(InspectModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new InspectModule(id); });

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    DynamicArrayC<StringCRC, 16> stages;
    app.GetAllStages(stages);

    ASSERT_EQ(stages.Size(), 2u);
    bool hasBoot = false, hasGame = false;
    for (unsigned int i = 0; i < stages.Size(); ++i)
    {
        if (stages[i] == StringCRC("Boot")) hasBoot = true;
        if (stages[i] == StringCRC("Game")) hasGame = true;
    }
    EXPECT_TRUE(hasBoot);
    EXPECT_TRUE(hasGame);
}

TEST(Inspectable, GetProcessingUnitsReturnsAllPUs)
{
    TypeRegistry reg;
    reg.Register(InspectModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new InspectModule(id); });

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    DynamicArrayC<StringCRC, 4> pus;
    app.GetProcessingUnits(pus);

    ASSERT_EQ(pus.Size(), 1u);
    EXPECT_EQ(pus[0], StringCRC("MainPU"));
}

TEST(Inspectable, GetActiveModulesReturnsModuleInfoWhenActive)
{
    TypeRegistry reg;
    reg.Register(InspectModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new InspectModule(id); });

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    PumpUntilSettled(app, StringCRC("MainPU"), 50);
    app.Update(1.0f / 60.0f);
    PumpUntilSettled(app, StringCRC("MainPU"), 50);

    DynamicArrayC<ModuleStateInfo, 64> infos;
    app.GetActiveModules(StringCRC("MainPU"), infos);

    // Both modA and modB are reported (regardless of state).
    ASSERT_EQ(infos.Size(), 2u);

    bool foundA = false, foundB = false;
    for (unsigned int i = 0; i < infos.Size(); ++i)
    {
        if (infos[i].instanceId == StringCRC("modA"))
        {
            foundA = true;
            EXPECT_EQ(infos[i].typeId, InspectModule::kTypeId);
            EXPECT_EQ(infos[i].state, ModuleState::kActive);
        }
        if (infos[i].instanceId == StringCRC("modB"))
        {
            foundB = true;
            // modB is in "Game" stage — should be kInactive in "Boot"
            EXPECT_EQ(infos[i].state, ModuleState::kInactive);
        }
    }
    EXPECT_TRUE(foundA);
    EXPECT_TRUE(foundB);
}

TEST(Inspectable, IsShuttingDownFalseBeforeRequest)
{
    TypeRegistry reg;
    reg.Register(InspectModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new InspectModule(id); });

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    EXPECT_FALSE(app.IsShuttingDown());
}

TEST(Inspectable, IsShuttingDownTrueAfterRequestShutdown)
{
    TypeRegistry reg;
    reg.Register(InspectModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new InspectModule(id); });

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    app.RequestShutdown();
    EXPECT_TRUE(app.IsShuttingDown());
}

TEST(Inspectable, IsTransitioningFalseWhenIdle)
{
    TypeRegistry reg;
    reg.Register(InspectModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new InspectModule(id); });

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    // After Start, no pending transition queued.
    EXPECT_FALSE(app.IsTransitioning());
}

TEST(Inspectable, IsTransitioningTrueAfterTransitionTo)
{
    TypeRegistry reg;
    reg.Register(InspectModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new InspectModule(id); });

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    PumpUntilSettled(app, StringCRC("MainPU"), 50);

    // Queue a transition without consuming it yet.
    app.TransitionTo(StringCRC("Game"));
    EXPECT_TRUE(app.IsTransitioning());

    // Pump until the transition fully completes (stop drain + start new modules).
    // IsTransitioning() stays true while any module is kStarting or kStopping.
    for (int i = 0; i < 50 && app.IsTransitioning(); ++i)
        app.Update(1.0f / 60.0f);

    EXPECT_FALSE(app.IsTransitioning());
}

TEST(Inspectable, GetInspectableReturnsSelf)
{
    TypeRegistry reg;
    reg.Register(InspectModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new InspectModule(id); });

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);

    IApplicationInspectable* inspectable = app.GetInspectable();
    EXPECT_EQ(inspectable, static_cast<IApplicationInspectable*>(&app));
}

TEST(Inspectable, GetStreamInfoEmptyWhenNoStreams)
{
    TypeRegistry reg;
    reg.Register(InspectModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new InspectModule(id); });

    auto manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    DynamicArrayC<StreamInfo, 16> streams;
    app.GetStreamInfo(streams);

    EXPECT_EQ(streams.Size(), 0u);
}
