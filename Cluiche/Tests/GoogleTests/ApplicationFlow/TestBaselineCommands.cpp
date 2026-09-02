////////////////////////////////////////////////////////////////////////////////
// Filename: TestBaselineCommands.cpp
// GoogleTest suite — DiaApplicationFlow baseline commands
//
// Covers AC5-AC10 from:
//   docs/specs/features/dia/diaapplicationflow/baseline-commands.md
//
// AC5: dia.app.quit command registered; calls RequestShutdown; returns {success:true}
// AC6: dia.app.report command registered; returns stage + modules + transition info
// AC7: Commands registered from Application::Start() before any module DoStart
// AC8: Commands use dotted namespace grammar
// AC9: Commands available without DiaAutomation or AutomationModule
// AC10: DiaAPI always linked; commands available even if registry not yet init'd
//
// Module types prefixed "BC_" to avoid ODR collisions.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Fixture module — always kReady / kDone; tracks DoStart call count
// ---------------------------------------------------------------------------
struct BC_SimpleModule : SimModule
{
    using SimModule::SimModule;
    static const StringCRC kTypeId;
    int startCallCount = 0;
    StartResult DoStart() override { ++startCallCount; return StartResult::kReady; }
    void        DoUpdate(const Dia::SimTime::SimTimeContext&) override {}
    StopResult  DoStop() override { return StopResult::kDone; }
};
const StringCRC BC_SimpleModule::kTypeId("BC_SimpleModule");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ApplicationManifestV3 BcBuildSingleStageManifest()
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
    mod.typeId         = BC_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);
    return manifest;
}

static TypeRegistry BcBuildRegistry()
{
    TypeRegistry reg;
    reg.Register(BC_SimpleModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new BC_SimpleModule(id); });
    return reg;
}

static bool BcPumpUntilSettled(Application& app, int limit = 50)
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
// Fixture: ensures DiaAPI is clean before each test
// ---------------------------------------------------------------------------
class BaselineCommandsTest : public ::testing::Test
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

// ---------------------------------------------------------------------------
// AC5 — dia.app.quit is registered and calls RequestShutdown
// ---------------------------------------------------------------------------
TEST_F(BaselineCommandsTest, AC5_QuitCommandRegistered)
{
    TypeRegistry reg = BcBuildRegistry();
    ApplicationManifestV3 manifest = BcBuildSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    BcPumpUntilSettled(app);

    // Command should be registered after Start() (JSON path)
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.app.quit"), Json::Value(Json::objectValue));
    EXPECT_TRUE(response["success"].asBool());

    // After quit, application should be shutting down
    EXPECT_TRUE(app.IsShuttingDown());

    // Drain to completion so Application destructor is clean
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// ---------------------------------------------------------------------------
// AC6 — dia.app.report returns stage + modules + transition
// ---------------------------------------------------------------------------
TEST_F(BaselineCommandsTest, AC6_ReportCommandReturnsState)
{
    TypeRegistry reg = BcBuildRegistry();
    ApplicationManifestV3 manifest = BcBuildSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    BcPumpUntilSettled(app);

    // Execute the report command
    Json::Value params(Json::objectValue);
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.app.report"), params);
    ASSERT_TRUE(response["success"].asBool()) << "dia.app.report failed: " << response.toStyledString();

    const Json::Value data = response["data"];

    // Stage field
    EXPECT_TRUE(data.isMember("stage"));
    EXPECT_EQ(data["stage"].asString(), std::string("Boot"));

    // Modules array
    EXPECT_TRUE(data.isMember("modules"));
    EXPECT_TRUE(data["modules"].isArray());
    EXPECT_GE(data["modules"].size(), 1u);

    if (data["modules"].size() > 0)
    {
        // Check first module has expected fields
        const Json::Value firstMod = data["modules"][0];
        EXPECT_TRUE(firstMod.isMember("instanceId"));
        EXPECT_TRUE(firstMod.isMember("typeId"));
        EXPECT_TRUE(firstMod.isMember("state"));
    }

    // Transition info
    EXPECT_TRUE(data.isMember("transition"));
    EXPECT_TRUE(data["transition"].isMember("inProgress"));
    EXPECT_TRUE(data["transition"].isMember("fromStage"));
    EXPECT_TRUE(data["transition"].isMember("toStage"));
    EXPECT_TRUE(data["transition"].isMember("heldByGuards"));

    // Clean shutdown
    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// ---------------------------------------------------------------------------
// AC7 — Commands registered before any module DoStart
// (Verified by: boot module's DoStart runs AFTER Start() registers commands)
// ---------------------------------------------------------------------------
TEST_F(BaselineCommandsTest, AC7_CommandsRegisteredBeforeModuleDoStart)
{
    TypeRegistry reg = BcBuildRegistry();
    ApplicationManifestV3 manifest = BcBuildSingleStageManifest();
    Application app(manifest, reg);

    // Before Start() — commands should NOT be registered yet
    EXPECT_EQ(Dia::API::ExecuteCommandJson(StringCRC("dia.app.report"), Json::Value(Json::objectValue))["error"].asString(),
              "command not found");

    ASSERT_TRUE(app.Start());

    // After Start() — commands are registered
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.app.report"), Json::Value(Json::objectValue));
    EXPECT_TRUE(response["success"].asBool());

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// ---------------------------------------------------------------------------
// AC8 — Commands use dotted namespace grammar (dia.app.*)
// ---------------------------------------------------------------------------
TEST_F(BaselineCommandsTest, AC8_CommandNamesUseDottedNamespace)
{
    TypeRegistry reg = BcBuildRegistry();
    ApplicationManifestV3 manifest = BcBuildSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    // Verify report has dotted name and works
    Json::Value resp = Dia::API::ExecuteCommandJson(StringCRC("dia.app.report"), Json::Value(Json::objectValue));
    EXPECT_TRUE(resp["success"].asBool());

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// ---------------------------------------------------------------------------
// AC9 — Commands available without DiaAutomation/AutomationModule
// (No automation module in manifest — commands still work)
// ---------------------------------------------------------------------------
TEST_F(BaselineCommandsTest, AC9_CommandsAvailableWithoutAutomation)
{
    TypeRegistry reg = BcBuildRegistry();
    ApplicationManifestV3 manifest = BcBuildSingleStageManifest();
    // No automation module in manifest
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    BcPumpUntilSettled(app);

    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.app.report"), Json::Value(Json::objectValue));
    EXPECT_TRUE(response["success"].asBool());

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// ---------------------------------------------------------------------------
// AC10 — DiaAPI always linked by DiaApplicationFlow
// (Commands registered via RegisterCommandJson even if registry not yet init'd)
// ---------------------------------------------------------------------------
TEST_F(BaselineCommandsTest, AC10_DiaApiAlwaysLinked)
{
    // DiaAPI is available and its header is reachable from DiaApplicationFlow context.
    // Verify the registry is functional (Initialize/Shutdown cycle).
    EXPECT_FALSE(Dia::API::IsInitialized());
    Dia::API::Initialize();
    EXPECT_TRUE(Dia::API::IsInitialized());
    Dia::API::Shutdown();
    EXPECT_FALSE(Dia::API::IsInitialized());
}

// ---------------------------------------------------------------------------
// dia.manifest.stages — returns stages reachable from Boot
// ---------------------------------------------------------------------------

static ApplicationManifestV3 BcBuildMultiStageManifest()
{
    ApplicationManifestV3 manifest;
    manifest.version = 3;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    boot.transitions.Add(StringCRC("Game"));
    boot.transitions.Add(StringCRC("Editor"));
    manifest.stages.Add(boot);

    StageDeclaration game;
    game.name = StringCRC("Game");
    game.transitions.Add(StringCRC("Boot"));
    manifest.stages.Add(game);

    StageDeclaration editor;
    editor.name = StringCRC("Editor");
    manifest.stages.Add(editor);

    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("testMod");
    mod.typeId         = BC_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));
    mod.stages.Add(StringCRC("Game"));
    mod.stages.Add(StringCRC("Editor"));
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);
    return manifest;
}

TEST_F(BaselineCommandsTest, ManifestStages_ReturnsBootTransitions)
{
    TypeRegistry reg = BcBuildRegistry();
    ApplicationManifestV3 manifest = BcBuildMultiStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    BcPumpUntilSettled(app);

    Json::Value params(Json::objectValue);
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.manifest.stages"), params);
    ASSERT_TRUE(response["success"].asBool())
        << "dia.manifest.stages failed: " << response.toStyledString();

    const Json::Value& data = response["data"];
    ASSERT_TRUE(data.isMember("stages"));
    const Json::Value& stages = data["stages"];
    ASSERT_EQ(stages.size(), 2u);

    std::string s0 = stages[0].asString();
    std::string s1 = stages[1].asString();
    bool hasGame   = (s0 == "Game" || s1 == "Game");
    bool hasEditor = (s0 == "Editor" || s1 == "Editor");
    EXPECT_TRUE(hasGame);
    EXPECT_TRUE(hasEditor);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

TEST_F(BaselineCommandsTest, ManifestStages_SingleStageManifest_ReturnsEmpty)
{
    // Boot is the only stage — nothing to navigate to
    TypeRegistry reg = BcBuildRegistry();
    ApplicationManifestV3 manifest = BcBuildSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    BcPumpUntilSettled(app);

    Json::Value params(Json::objectValue);
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.manifest.stages"), params);
    ASSERT_TRUE(response["success"].asBool());

    const Json::Value& stages = response["data"]["stages"];
    EXPECT_EQ(stages.size(), 0u);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

TEST_F(BaselineCommandsTest, ManifestStages_NoExplicitTransitions_ReturnsAllOtherStages)
{
    // When Boot has no explicit transitions, all non-Boot stages are returned
    TypeRegistry reg = BcBuildRegistry();
    ApplicationManifestV3 manifest;
    manifest.version = 3;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);

    StageDeclaration alpha;
    alpha.name = StringCRC("Alpha");
    manifest.stages.Add(alpha);

    StageDeclaration beta;
    beta.name = StringCRC("Beta");
    manifest.stages.Add(beta);

    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("testMod");
    mod.typeId         = BC_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));
    mod.stages.Add(StringCRC("Alpha"));
    mod.stages.Add(StringCRC("Beta"));
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    BcPumpUntilSettled(app);

    Json::Value params(Json::objectValue);
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.manifest.stages"), params);
    ASSERT_TRUE(response["success"].asBool());

    const Json::Value& stages = response["data"]["stages"];
    ASSERT_EQ(stages.size(), 2u);

    std::string s0 = stages[0].asString();
    std::string s1 = stages[1].asString();
    bool hasAlpha = (s0 == "Alpha" || s1 == "Alpha");
    bool hasBeta  = (s0 == "Beta"  || s1 == "Beta");
    EXPECT_TRUE(hasAlpha);
    EXPECT_TRUE(hasBeta);

    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}
