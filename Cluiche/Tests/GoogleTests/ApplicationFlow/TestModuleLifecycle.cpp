////////////////////////////////////////////////////////////////////////////////
// Filename: TestModuleLifecycle.cpp
// GoogleTest suite — DiaApplicationFlow v2 Module lifecycle
//
// Tests module state transitions driven through Application::Start() /
// Application::Update().  All tests use a local TypeRegistry to avoid
// polluting the global singleton.
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
// Fixtures
// ---------------------------------------------------------------------------

struct CountingModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    int startCalls  = 0;
    int updateCalls = 0;
    int stopCalls   = 0;

    StartResult startResult = StartResult::kReady;
    StopResult  stopResult  = StopResult::kDone;

    StartResult DoStart() override { ++startCalls; return startResult; }
    void        DoUpdate(float) override { ++updateCalls; }
    StopResult  DoStop() override { ++stopCalls; return stopResult; }
};
const StringCRC CountingModule::kTypeId("CountingModule");

// Module that returns kLoading a configurable number of times then kReady.
struct AsyncStartModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    int loadingFrames = 3;    // how many kLoading results before kReady
    int startCallCount = 0;

    StartResult DoStart() override
    {
        ++startCallCount;
        return (startCallCount >= loadingFrames) ? StartResult::kReady : StartResult::kLoading;
    }
    void       DoUpdate(float) override {}
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC AsyncStartModule::kTypeId("AsyncStartModule");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a minimal single-PU, single-module manifest.
static ApplicationManifestV2 BuildSingleModuleManifest(
    const char* puId,
    const char* typeId,
    const char* instanceId,
    const char* stageName,
    float startTimeoutMs = 10000.0f,
    float stopTimeoutMs  = 5000.0f)
{
    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration stage;
    stage.name = StringCRC(stageName);
    manifest.stages.Add(stage);
    manifest.initialStage = StringCRC(stageName);

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC(puId);
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration mod;
    mod.instanceId      = StringCRC(instanceId);
    mod.typeId          = StringCRC(typeId);
    mod.startTimeoutMs  = startTimeoutMs;
    mod.stopTimeoutMs   = stopTimeoutMs;
    mod.stages.Add(StringCRC(stageName));

    pu.modules.Add(mod);
    manifest.processingUnits.Add(pu);
    return manifest;
}

// Pump Update() until all modules in puId are kActive, or until safetyLimit
// iterations. Returns true if all are active within the limit.
static bool PumpUntilActive(Application& app,
                             const StringCRC& puId,
                             int safetyLimit = 100)
{
    for (int i = 0; i < safetyLimit; ++i)
    {
        DynamicArrayC<ModuleStateInfo, 64> infos;
        app.GetActiveModules(puId, infos);
        bool allActive = (infos.Size() > 0);
        for (unsigned int m = 0; m < infos.Size(); ++m)
        {
            if (infos[m].state != ModuleState::kActive)
            {
                allActive = false;
                break;
            }
        }
        if (allActive)
            return true;
        app.Update(1.0f / 60.0f);
    }
    return false;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// After Start(), a module in the initial stage is kStarting (DoStart not yet
// called on that first Update) or kActive after one Update returns kReady.
TEST(ModuleLifecycle, StartReadyBecomesActive)
{
    TypeRegistry reg;
    reg.Register(CountingModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new CountingModule(id); });

    auto manifest = BuildSingleModuleManifest("MainPU", "CountingModule", "mod0", "Boot");
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    bool active = PumpUntilActive(app, StringCRC("MainPU"), 50);
    EXPECT_TRUE(active);

    DynamicArrayC<ModuleStateInfo, 64> infos;
    app.GetActiveModules(StringCRC("MainPU"), infos);
    ASSERT_EQ(infos.Size(), 1u);
    EXPECT_EQ(infos[0].state, ModuleState::kActive);
}

// File-scoped pointer used by UpdateCallsDoUpdate factory to capture the
// created module instance without a capturing lambda.
static CountingModule* g_updateTestModule = nullptr;

// Each Update() after a module is active calls DoUpdate() once.
TEST(ModuleLifecycle, UpdateCallsDoUpdate)
{
    g_updateTestModule = nullptr;

    TypeRegistry reg;
    reg.Register(CountingModule::kTypeId,
        [](const StringCRC& id) -> Module*
        {
            auto* m = new CountingModule(id);
            g_updateTestModule = m;
            return m;
        });

    auto manifest = BuildSingleModuleManifest("MainPU", "CountingModule", "mod0", "Boot");
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    ASSERT_TRUE(PumpUntilActive(app, StringCRC("MainPU"), 50));
    ASSERT_NE(g_updateTestModule, nullptr);

    const int updatesBefore = g_updateTestModule->updateCalls;
    app.Update(1.0f / 60.0f);
    EXPECT_EQ(g_updateTestModule->updateCalls, updatesBefore + 1)
        << "DoUpdate should be called exactly once per Update";

    app.Update(1.0f / 60.0f);
    EXPECT_EQ(g_updateTestModule->updateCalls, updatesBefore + 2);

    app.Update(1.0f / 60.0f);
    EXPECT_EQ(g_updateTestModule->updateCalls, updatesBefore + 3);
}

// RequestShutdown() causes all modules to stop; Update() returns false.
TEST(ModuleLifecycle, ShutdownStopsModule)
{
    TypeRegistry reg;
    reg.Register(CountingModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new CountingModule(id); });

    auto manifest = BuildSingleModuleManifest("MainPU", "CountingModule", "mod0", "Boot");
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    ASSERT_TRUE(PumpUntilActive(app, StringCRC("MainPU"), 50));

    app.RequestShutdown();

    int safetyLimit = 100;
    bool stopped = false;
    for (int i = 0; i < safetyLimit; ++i)
    {
        if (!app.Update(1.0f / 60.0f))
        {
            stopped = true;
            break;
        }
    }

    EXPECT_TRUE(stopped) << "Application did not shut down within safety limit";
}

// A module that returns kLoading several times before kReady eventually
// becomes kActive after enough Update() calls.
// loadingFrames=3 means DoStart returns kLoading on calls 1, 2 and kReady on
// call 3.
TEST(ModuleLifecycle, AsyncStartLoopsUntilReady)
{
    TypeRegistry reg;
    // Non-capturing lambda is convertible to FactoryFn (raw function pointer).
    // loadingFrames defaults to 3 in AsyncStartModule.
    reg.Register(AsyncStartModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new AsyncStartModule(id); });

    auto manifest = BuildSingleModuleManifest("MainPU", "AsyncStartModule", "asyncMod", "Boot");
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    // After Start() the module is kStarting, not yet kActive.
    {
        DynamicArrayC<ModuleStateInfo, 64> infos;
        app.GetActiveModules(StringCRC("MainPU"), infos);
        if (infos.Size() > 0)
        {
            EXPECT_NE(infos[0].state, ModuleState::kActive)
                << "Module should not be kActive before enough Updates";
        }
    }

    // After enough Updates the module should become kActive.
    bool active = PumpUntilActive(app, StringCRC("MainPU"), 20);
    EXPECT_TRUE(active) << "Async-start module did not become kActive within 20 frames";
}

// A module that never returns kReady from DoStart exceeds its startTimeoutMs
// and is forced into kFailed.  The framework sets kFailed in Module::FrameTick
// when the elapsed time exceeds the configured timeout.
TEST(ModuleLifecycle, StartTimeoutCausesKFailed)
{
    TypeRegistry reg;
    reg.Register(AsyncStartModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new AsyncStartModule(id); });

    // Configure a very short timeout (50 ms) and a large loadingFrames value
    // so the module never returns kReady on its own.
    // loadingFrames defaults to 3 — change to a large value via a custom factory.
    static AsyncStartModule* g_timeoutMod = nullptr;
    struct Factory {
        static Module* Create(const StringCRC& id)
        {
            auto* m = new AsyncStartModule(id);
            m->loadingFrames = 9999;  // will never self-complete
            g_timeoutMod = m;
            return m;
        }
    };

    TypeRegistry reg2;
    reg2.Register(AsyncStartModule::kTypeId, Factory::Create);

    // Use a tiny startTimeoutMs (10 ms).  Each Update passes ~16 ms at 60 Hz.
    auto manifest = BuildSingleModuleManifest("MainPU", "AsyncStartModule", "timeoutMod", "Boot",
                                              /*startTimeoutMs=*/10.0f,
                                              /*stopTimeoutMs=*/5000.0f);
    Application app(manifest, reg2);
    ASSERT_TRUE(app.Start());

    // One Update at 60 Hz (dt ~16 ms) exceeds the 10 ms timeout.
    app.Update(1.0f / 60.0f);

    ASSERT_NE(g_timeoutMod, nullptr);
    EXPECT_EQ(g_timeoutMod->GetState(), ModuleState::kFailed)
        << "Module should be kFailed after startTimeoutMs exceeded";
}

// IsShuttingDown() returns true after RequestShutdown().
TEST(ModuleLifecycle, IsShuttingDownAfterRequest)
{
    TypeRegistry reg;
    reg.Register(CountingModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new CountingModule(id); });

    auto manifest = BuildSingleModuleManifest("MainPU", "CountingModule", "mod0", "Boot");
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    EXPECT_FALSE(app.IsShuttingDown());
    app.RequestShutdown();
    EXPECT_TRUE(app.IsShuttingDown());
}
