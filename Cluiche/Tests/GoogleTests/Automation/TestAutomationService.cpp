////////////////////////////////////////////////////////////////////////////////
// Filename: TestAutomationService.cpp
// GoogleTest suite — DiaAutomation AutomationService
//
// Covers AC1-AC8 from each of:
//   docs/specs/features/dia/diaautomation/checkpoint-registry.md
//   docs/specs/features/dia/diaautomation/pause-resume.md
//   docs/specs/features/dia/diaautomation/navigation-hold.md
//   docs/specs/features/dia/diaautomation/ci-safety.md
//
// Module types prefixed "AS_" to avoid ODR collisions.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Fixture module — always kReady / kDone
// ---------------------------------------------------------------------------
struct AS_SimpleModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;
    StartResult DoStart() override { return StartResult::kReady; }
    void        DoUpdate(float) override {}
    StopResult  DoStop() override { return StopResult::kDone; }
};
const StringCRC AS_SimpleModule::kTypeId("AS_SimpleModule");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ApplicationManifestV3 AsAddSingleStageManifest()
{
    ApplicationManifestV3 manifest;
    manifest.version = 3;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("bootMod");
    mod.typeId         = AS_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);
    return manifest;
}

static ApplicationManifestV3 AsAddTwoStageManifest()
{
    ApplicationManifestV3 manifest;
    manifest.version = 3;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);

    StageDeclaration game;
    game.name = StringCRC("Game");
    manifest.stages.Add(game);

    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration bootMod;
    bootMod.instanceId     = StringCRC("bootMod");
    bootMod.typeId         = AS_SimpleModule::kTypeId;
    bootMod.startTimeoutMs = 10000.0f;
    bootMod.stopTimeoutMs  = 5000.0f;
    bootMod.stages.Add(StringCRC("Boot"));
    pu.modules.Add(bootMod);

    ModuleDeclaration gameMod;
    gameMod.instanceId     = StringCRC("gameMod");
    gameMod.typeId         = AS_SimpleModule::kTypeId;
    gameMod.startTimeoutMs = 10000.0f;
    gameMod.stopTimeoutMs  = 5000.0f;
    gameMod.stages.Add(StringCRC("Game"));
    pu.modules.Add(gameMod);

    manifest.processingUnits.Add(pu);
    return manifest;
}

static TypeRegistry AsBuildRegistry()
{
    TypeRegistry reg;
    reg.Register(AS_SimpleModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new AS_SimpleModule(id); });
    return reg;
}

static bool AsPumpUntilSettled(Application& app, int limit = 50)
{
    for (int i = 0; i < limit; ++i)
    {
        DynamicArrayC<ModuleStateInfo, 64> infos;
        app.GetActiveModules(StringCRC("MainPU"), infos);
        bool transitioning = false;
        for (unsigned int m = 0; m < infos.Size(); ++m)
        {
            if (infos[m].state == ModuleState::kStarting || infos[m].state == ModuleState::kStopping)
            {
                transitioning = true;
                break;
            }
        }
        if (!transitioning)
            return true;
        app.Update(1.0f / 60.0f);
    }
    return false;
}

// ---------------------------------------------------------------------------
// Fixture: ensures DiaAPI is clean before and after each test
// ---------------------------------------------------------------------------
class AutomationServiceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Dia::API::Shutdown();
    }
    void TearDown() override
    {
        Dia::API::Shutdown();
    }
};

// ===========================================================================
// Checkpoint Registry
// ===========================================================================

// AC1 — RegisterCheckpoint stores the entry; HasCheckpoint returns true
TEST_F(AutomationServiceTest, CheckpointRegistry_RegisterAndHas)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);

    service.RegisterCheckpoint(nullptr, StringCRC("myCheck"),
        []() -> Dia::Automation::CheckpointResult { return {true, "ok", 1.0f}; });

    EXPECT_TRUE(service.HasCheckpoint(StringCRC("myCheck")));

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC2 — RegisterCheckpoint rejects duplicate names; size stays at 1
TEST_F(AutomationServiceTest, CheckpointRegistry_DuplicateRejected)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);

    service.RegisterCheckpoint(nullptr, StringCRC("myCheck"),
        []() -> Dia::Automation::CheckpointResult { return {true, "first", 1.0f}; });

    // Second registration of same name — must not crash and must not replace
    service.RegisterCheckpoint(nullptr, StringCRC("myCheck"),
        []() -> Dia::Automation::CheckpointResult { return {true, "second", 2.0f}; });

    // Still present (original not removed)
    EXPECT_TRUE(service.HasCheckpoint(StringCRC("myCheck")));

    // Original callback still returns "first"
    Dia::Automation::CheckpointResult result = service.RunCheckpoint(StringCRC("myCheck"));
    EXPECT_STREQ(result.message, "first");

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC3/AC8 — UnregisterCheckpoints removes entries by owner; HasCheckpoint returns false
TEST_F(AutomationServiceTest, CheckpointRegistry_Unregister)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);

    // Register with nullptr as owner
    service.RegisterCheckpoint(nullptr, StringCRC("myCheck"),
        []() -> Dia::Automation::CheckpointResult { return {true, "ok", 1.0f}; });
    ASSERT_TRUE(service.HasCheckpoint(StringCRC("myCheck")));

    service.UnregisterCheckpoints(nullptr);
    EXPECT_FALSE(service.HasCheckpoint(StringCRC("myCheck")));

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC4 — RunCheckpoint invokes the fn and returns its CheckpointResult
TEST_F(AutomationServiceTest, CheckpointRegistry_RunCheckpoint)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);

    service.RegisterCheckpoint(nullptr, StringCRC("myCheck"),
        []() -> Dia::Automation::CheckpointResult { return {true, "ok", 1.0f}; });

    Dia::Automation::CheckpointResult result = service.RunCheckpoint(StringCRC("myCheck"));
    EXPECT_TRUE(result.passed);
    EXPECT_STREQ(result.message, "ok");
    EXPECT_FLOAT_EQ(result.durationMs, 1.0f);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC5 — RunCheckpoint for unknown name returns {false, "checkpoint not found", 0}
TEST_F(AutomationServiceTest, CheckpointRegistry_RunUnknown)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);

    Dia::Automation::CheckpointResult result = service.RunCheckpoint(StringCRC("doesNotExist"));
    EXPECT_FALSE(result.passed);
    EXPECT_STREQ(result.message, "checkpoint not found");
    EXPECT_FLOAT_EQ(result.durationMs, 0.0f);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC — dia.automation.list_checkpoints returns all registered checkpoint names
TEST_F(AutomationServiceTest, CheckpointRegistry_ListCheckpointsCommand)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    service.RegisterCheckpoint(nullptr, StringCRC("alpha"),
        []() -> Dia::Automation::CheckpointResult { return {true, "a", 0.0f}; });
    service.RegisterCheckpoint(nullptr, StringCRC("beta"),
        []() -> Dia::Automation::CheckpointResult { return {false, "b", 0.0f}; });

    Json::Value params(Json::objectValue);
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.list_checkpoints"), params);

    ASSERT_TRUE(response["success"].asBool())
        << "dia.automation.list_checkpoints failed: " << response.toStyledString();

    const Json::Value& data = response["data"];
    ASSERT_TRUE(data.isMember("checkpoints"));
    const Json::Value& arr = data["checkpoints"];
    ASSERT_EQ(arr.size(), 2u);

    // Verify both names are present (order not guaranteed)
    std::string name0 = arr[0].asString();
    std::string name1 = arr[1].asString();
    bool hasAlpha = (name0 == "alpha" || name1 == "alpha");
    bool hasBeta  = (name0 == "beta"  || name1 == "beta");
    EXPECT_TRUE(hasAlpha);
    EXPECT_TRUE(hasBeta);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC — dia.automation.list_checkpoints with no checkpoints returns empty array
TEST_F(AutomationServiceTest, CheckpointRegistry_ListCheckpointsEmpty)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    Json::Value params(Json::objectValue);
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.list_checkpoints"), params);

    ASSERT_TRUE(response["success"].asBool());

    const Json::Value& data = response["data"];
    ASSERT_TRUE(data.isMember("checkpoints"));
    EXPECT_EQ(data["checkpoints"].size(), 0u);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC — dia.automation.list_checkpoints shrinks after UnregisterCheckpoints
TEST_F(AutomationServiceTest, CheckpointRegistry_ListCheckpointsAfterUnregister)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    service.RegisterCheckpoint(nullptr, StringCRC("check_a"),
        []() -> Dia::Automation::CheckpointResult { return {true, "a", 0.0f}; });
    service.RegisterCheckpoint(nullptr, StringCRC("check_b"),
        []() -> Dia::Automation::CheckpointResult { return {true, "b", 0.0f}; });

    // Verify both present
    Json::Value params(Json::objectValue);
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.list_checkpoints"), params);
    ASSERT_TRUE(response["success"].asBool());
    ASSERT_EQ(response["data"]["checkpoints"].size(), 2u);

    // Unregister all (nullptr owner)
    service.UnregisterCheckpoints(nullptr);

    response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.list_checkpoints"), params);
    ASSERT_TRUE(response["success"].asBool());
    EXPECT_EQ(response["data"]["checkpoints"].size(), 0u);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC — dia.automation.list_checkpoints only shows checkpoints for current owner set
TEST_F(AutomationServiceTest, CheckpointRegistry_ListCheckpointsPartialUnregister)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    // Register two checkpoints with different owners (use Module* cast as fake ptrs)
    auto* fakeOwnerA = reinterpret_cast<Dia::ApplicationFlow::Module*>(0x1);
    auto* fakeOwnerB = reinterpret_cast<Dia::ApplicationFlow::Module*>(0x2);

    service.RegisterCheckpoint(fakeOwnerA, StringCRC("owned_by_a"),
        []() -> Dia::Automation::CheckpointResult { return {true, "a", 0.0f}; });
    service.RegisterCheckpoint(fakeOwnerB, StringCRC("owned_by_b"),
        []() -> Dia::Automation::CheckpointResult { return {true, "b", 0.0f}; });

    Json::Value params(Json::objectValue);
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.list_checkpoints"), params);
    ASSERT_EQ(response["data"]["checkpoints"].size(), 2u);

    // Unregister only owner A
    service.UnregisterCheckpoints(fakeOwnerA);

    response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.list_checkpoints"), params);
    ASSERT_TRUE(response["success"].asBool());
    ASSERT_EQ(response["data"]["checkpoints"].size(), 1u);
    EXPECT_EQ(response["data"]["checkpoints"][0].asString(), std::string("owned_by_b"));

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC6 — dia.automation.validate command; returns {success:true, data:{passed,message}}
TEST_F(AutomationServiceTest, CheckpointRegistry_ValidateCommand)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    service.RegisterCheckpoint(nullptr, StringCRC("myCheck"),
        []() -> Dia::Automation::CheckpointResult { return {true, "pass", 0.5f}; });

    Json::Value params(Json::objectValue);
    params["checkpoint"] = "myCheck";
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.validate"), params);

    ASSERT_TRUE(response["success"].asBool())
        << "dia.automation.validate failed: " << response.toStyledString();

    const Json::Value& data = response["data"];
    EXPECT_TRUE(data["passed"].asBool());
    EXPECT_EQ(data["message"].asString(), std::string("pass"));

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC7 — dia.automation.validate for unknown checkpoint returns {success:false, error:...}
TEST_F(AutomationServiceTest, CheckpointRegistry_ValidateCommandUnknown)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    Json::Value params(Json::objectValue);
    params["checkpoint"] = "noSuchCheck";
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.validate"), params);

    EXPECT_FALSE(response["success"].asBool());
    EXPECT_FALSE(response["error"].asString().empty());
    // Error message should reference "checkpoint not found"
    EXPECT_NE(response["error"].asString().find("checkpoint not found"), std::string::npos);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// ===========================================================================
// Pause / Resume
// ===========================================================================

// AC3/AC7 — Pause() invokes pause callback; IsPaused() returns true
TEST_F(AutomationServiceTest, PauseResume_Pause)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);

    bool pauseFlag = false;
    service.RegisterPauseCallback(nullptr,
        [&pauseFlag]() { pauseFlag = true; },
        []() {});

    service.Pause();

    EXPECT_TRUE(service.IsPaused());
    EXPECT_TRUE(pauseFlag);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC4/AC8 — Pause then Resume; IsPaused() returns false; resume callback called
TEST_F(AutomationServiceTest, PauseResume_Resume)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);

    bool resumeFlag = false;
    service.RegisterPauseCallback(nullptr,
        []() {},
        [&resumeFlag]() { resumeFlag = true; });

    service.Pause();
    ASSERT_TRUE(service.IsPaused());

    service.Resume();
    EXPECT_FALSE(service.IsPaused());
    EXPECT_TRUE(resumeFlag);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC5 — Pause() while already paused is a no-op; callback called only once
TEST_F(AutomationServiceTest, PauseResume_DoublePauseNoop)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);

    int pauseCount = 0;
    service.RegisterPauseCallback(nullptr,
        [&pauseCount]() { ++pauseCount; },
        []() {});

    service.Pause();
    service.Pause();  // second call should be no-op

    EXPECT_TRUE(service.IsPaused());
    EXPECT_EQ(pauseCount, 1);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC6 — Resume() while not paused is a no-op; callback not called
TEST_F(AutomationServiceTest, PauseResume_DoubleResumeNoop)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);

    int resumeCount = 0;
    service.RegisterPauseCallback(nullptr,
        []() {},
        [&resumeCount]() { ++resumeCount; });

    // Resume without pausing first — should be no-op
    service.Resume();

    EXPECT_FALSE(service.IsPaused());
    EXPECT_EQ(resumeCount, 0);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// ===========================================================================
// Navigation Hold
// ===========================================================================

// AC1/AC7 — EnableNavigationHold registers a guard that blocks transitions
TEST_F(AutomationServiceTest, NavigationHold_EnableHoldsTransition)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.EnableNavigationHold();
    EXPECT_TRUE(service.IsHolding());

    // Request transition to "Game" — hold should block it
    app.TransitionTo(StringCRC("Game"));

    // Pump a few frames — transition must NOT commit
    for (int i = 0; i < 5; ++i)
        app.Update(1.0f / 60.0f);

    EXPECT_EQ(app.GetCurrentStage(), StringCRC("Boot"));

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC3 — ReleaseNavigationHold with unknown stage returns success=false; guard stays held
TEST_F(AutomationServiceTest, NavigationHold_ReleaseInvalidStage)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.EnableNavigationHold();

    bool success = true;
    const char* errMsg = nullptr;
    service.ReleaseNavigationHold(StringCRC("BOGUS"), &success, &errMsg);

    EXPECT_FALSE(success);
    EXPECT_NE(errMsg, nullptr);

    // Guard should still be holding
    EXPECT_TRUE(service.IsHolding());

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC4 — ReleaseNavigationHold for valid stage: transition proceeds to that stage
TEST_F(AutomationServiceTest, NavigationHold_ReleaseValidStage)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.EnableNavigationHold();

    // Queue transition — hold blocks it
    app.TransitionTo(StringCRC("Game"));

    // Pump a few frames to confirm hold is active
    for (int i = 0; i < 5; ++i)
        app.Update(1.0f / 60.0f);
    ASSERT_EQ(app.GetCurrentStage(), StringCRC("Boot"));

    // Release hold for "Game" — guard switches to Allow, TransitionTo was already queued
    bool success = false;
    const char* errMsg = nullptr;
    service.ReleaseNavigationHold(StringCRC("Game"), &success, &errMsg);
    EXPECT_TRUE(success);

    // Pump until settled
    for (int i = 0; i < 50; ++i)
    {
        app.Update(1.0f / 60.0f);
        if (app.GetCurrentStage() == StringCRC("Game") && !app.IsTransitioning())
            break;
    }

    EXPECT_EQ(app.GetCurrentStage(), StringCRC("Game"));

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC6 — dia.automation.navigate_to command releases hold and transitions
TEST_F(AutomationServiceTest, NavigationHold_NavigateToCommand)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();
    service.EnableNavigationHold();

    // Execute the navigate_to command
    Json::Value params(Json::objectValue);
    params["target"] = "Game";
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.navigate_to"), params);
    ASSERT_TRUE(response["success"].asBool())
        << "dia.automation.navigate_to failed: " << response.toStyledString();

    // Pump until settled
    for (int i = 0; i < 50; ++i)
    {
        app.Update(1.0f / 60.0f);
        if (app.GetCurrentStage() == StringCRC("Game") && !app.IsTransitioning())
            break;
    }

    EXPECT_EQ(app.GetCurrentStage(), StringCRC("Game"));

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// ===========================================================================
// CI Safety
// ===========================================================================

// AC1 — OnDisconnect() calls app.RequestShutdown()
TEST_F(AutomationServiceTest, CISafety_OnDisconnectShutdown)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);

    service.OnDisconnect();

    EXPECT_TRUE(app.IsShuttingDown());

    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC6 — TickHeartbeat accumulates; exceeding timeout triggers OnDisconnect
TEST_F(AutomationServiceTest, CISafety_HeartbeatTimeout)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.EnableHeartbeatMonitor(1.0f);

    // Tick below threshold — must NOT trigger shutdown
    service.TickHeartbeat(0.9f);
    EXPECT_FALSE(app.IsShuttingDown());

    // Tick enough to push over threshold — must trigger OnDisconnect → shutdown
    service.TickHeartbeat(0.2f);
    EXPECT_TRUE(app.IsShuttingDown());

    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// AC5/AC8 — ResetHeartbeat prevents timeout accumulation
TEST_F(AutomationServiceTest, CISafety_ResetHeartbeat)
{
    TypeRegistry reg = AsBuildRegistry();
    ApplicationManifestV3 manifest = AsAddSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    AsPumpUntilSettled(app);

    Dia::Automation::AutomationService service(app);
    service.EnableHeartbeatMonitor(1.0f);

    // Tick to 0.8s — below threshold
    service.TickHeartbeat(0.8f);
    ASSERT_FALSE(app.IsShuttingDown());

    // Reset elapsed back to 0
    service.ResetHeartbeat();

    // Tick another 0.8s — still below threshold because we reset
    service.TickHeartbeat(0.8f);
    EXPECT_FALSE(app.IsShuttingDown());

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}
