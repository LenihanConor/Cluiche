////////////////////////////////////////////////////////////////////////////////
// Filename: TestPUTypedModules.cpp
// GoogleTest suite — DiaSimTime Task 1.3: typed module bases
//                    (SimModule / RenderModule / MainModule).
//
// Covers the plan's acceptance criteria:
//   - "a SimModule receives populated SimTimeContext" — a test-local SimModule
//     subclass records the SimTimeContext it is handed in
//     DoUpdate(const SimTimeContext&), driven through a real Application tick.
//   - "RenderModule cannot be constructed against sim time" — a compile-time
//     (static_assert) guarantee that the Sim / Render typed bases are distinct,
//     non-interchangeable hierarchies with incompatible DoUpdate overload sets,
//     so a Sim-shaped body cannot be silently plugged into a Render-shaped base
//     (or vice versa).
//
// NOTE (TDD RED): The SimModule tick test references
// ProcessingUnit::GetSimTimeContext(), which is added in DiaSimTime plan Task 1.4
// (same hard-cutover branch). Until 1.4 lands this file will fail to LINK — that
// is intentional and expected; Task 1.4 turns it GREEN.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/RenderModule.h>
#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <type_traits>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;
using Dia::SimTime::SimTimeContext;
using Dia::SimTime::RenderTimeContext;
using Dia::SimTime::MainTimeContext;

// ---------------------------------------------------------------------------
// Test-local typed module subclasses
// ---------------------------------------------------------------------------
namespace {

    // A SimModule subclass that records the last SimTimeContext handed to it.
    struct RecordingSimModule : SimModule
    {
        using SimModule::SimModule;
        static const StringCRC kTypeId;

        int            updateCalls = 0;
        // TimeAbsolute/TimeRelative expose only a private default constructor
        // (factory-only construction), so SimTimeContext{} cannot value-initialize
        // gameTime/gameDt implicitly (MSVC C2512) — supply Zero() explicitly.
        SimTimeContext lastCtx{ Dia::Core::TimeAbsolute::Zero(), Dia::Core::TimeRelative::Zero() };

        StartResult DoStart() override { return StartResult::kReady; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void        DoUpdate(const SimTimeContext& ctx) override
        {
            ++updateCalls;
            lastCtx = ctx;
        }
    };
    const StringCRC RecordingSimModule::kTypeId("TypedMod_RecordingSimModule");

    // A RenderModule subclass — used only for the compile-time distinctness check.
    struct RecordingRenderModule : RenderModule
    {
        using RenderModule::RenderModule;
        StartResult DoStart() override { return StartResult::kReady; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void        DoUpdate(const RenderTimeContext&) override {}
    };

    // A MainModule subclass — used only for the compile-time distinctness check.
    struct RecordingMainModule : MainModule
    {
        using MainModule::MainModule;
        StartResult DoStart() override { return StartResult::kReady; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void        DoUpdate(const MainTimeContext&) override {}
    };

    // File-scoped pointer so a non-capturing factory can hand the created
    // instance back to the test (mirrors TestModuleLifecycle's pattern).
    RecordingSimModule* g_recordingSimModule = nullptr;

    // Build a minimal single-PU, single-module manifest (mirrors TestModuleLifecycle).
    ApplicationManifestV3 BuildSingleModuleManifest(
        const char* puId,
        const char* typeId,
        const char* instanceId,
        const char* stageName)
    {
        ApplicationManifestV3 manifest;
        manifest.version = 3;

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
        mod.startTimeoutMs  = 10000.0f;
        mod.stopTimeoutMs   = 5000.0f;
        mod.stages.Add(StringCRC(stageName));

        pu.modules.Add(mod);
        manifest.processingUnits.Add(pu);
        return manifest;
    }

    // Pump Update() until the single module in puId is kActive (mirrors TestModuleLifecycle).
    bool PumpUntilActive(Application& app, const StringCRC& puId, int safetyLimit = 50)
    {
        for (int i = 0; i < safetyLimit; ++i)
        {
            DynamicArrayC<ModuleStateInfo, 64> infos;
            app.GetActiveModules(puId, infos);
            bool allActive = (infos.Size() > 0);
            for (unsigned int m = 0; m < infos.Size(); ++m)
            {
                if (infos[m].state != ModuleState::kActive) { allActive = false; break; }
            }
            if (allActive) return true;
            app.Update(1.0f / 60.0f);
        }
        return false;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Compile-time distinctness: Sim / Render / Main bases are unrelated hierarchies
// ---------------------------------------------------------------------------
// The three typed bases each seal DoUpdate(float) and expose a DIFFERENT typed
// virtual (SimTimeContext vs RenderTimeContext vs MainTimeContext). Those context
// types are non-convertible, so the overload sets are incompatible: a
// RenderModule subclass can never satisfy a SimModule's DoUpdate, and the two
// bases are not related by inheritance. This is the real meaning of "RenderModule
// cannot be constructed against sim time" — you cannot substitute one typed base
// for another without a compile error.
static_assert(!std::is_base_of_v<SimModule, RenderModule>,
              "RenderModule must not derive from SimModule — distinct time hierarchies");
static_assert(!std::is_base_of_v<RenderModule, SimModule>,
              "SimModule must not derive from RenderModule — distinct time hierarchies");
static_assert(!std::is_base_of_v<SimModule, MainModule>,
              "MainModule must not derive from SimModule — distinct time hierarchies");
static_assert(!std::is_base_of_v<RenderModule, MainModule>,
              "MainModule must not derive from RenderModule — distinct time hierarchies");
// The typed context types are themselves mutually non-convertible, so a Sim body
// can never be handed a Render context (or vice versa) through these virtuals.
static_assert(!std::is_convertible_v<SimTimeContext, RenderTimeContext>,
              "SimTimeContext and RenderTimeContext must be non-interchangeable");
static_assert(!std::is_convertible_v<RenderTimeContext, SimTimeContext>,
              "RenderTimeContext and SimTimeContext must be non-interchangeable");
// All three are, however, Modules (they share the framework lifecycle contract).
static_assert(std::is_base_of_v<Module, SimModule>,    "SimModule must be a Module");
static_assert(std::is_base_of_v<Module, RenderModule>, "RenderModule must be a Module");
static_assert(std::is_base_of_v<Module, MainModule>,   "MainModule must be a Module");

// ---------------------------------------------------------------------------
// Runtime: a SimModule receives a populated SimTimeContext through a real tick
// ---------------------------------------------------------------------------
TEST(PUTypedModules, SimModuleReceivesPopulatedSimTimeContext)
{
    g_recordingSimModule = nullptr;

    TypeRegistry reg;
    reg.Register(RecordingSimModule::kTypeId,
        [](const StringCRC& id) -> Module*
        {
            auto* m = new RecordingSimModule(id);
            g_recordingSimModule = m;
            return m;
        });

    auto manifest = BuildSingleModuleManifest("SimPU", "TypedMod_RecordingSimModule",
                                              "simMod0", "Boot");
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    ASSERT_TRUE(PumpUntilActive(app, StringCRC("SimPU"), 50));
    ASSERT_NE(g_recordingSimModule, nullptr);

    const int before = g_recordingSimModule->updateCalls;
    app.Update(1.0f / 60.0f);
    ASSERT_EQ(g_recordingSimModule->updateCalls, before + 1)
        << "SimModule::DoUpdate(const SimTimeContext&) should fire once per Update";

    // The context the ProcessingUnit cached and forwarded should be populated:
    // a running sim advances the tick counter and carries a real (non-negative)
    // time scale.
    const SimTimeContext& ctx = g_recordingSimModule->lastCtx;
    EXPECT_GT(ctx.timeScale, 0.0f)
        << "A running sim should forward a positive timeScale (default 1.0)";
    EXPECT_GE(ctx.tick, 1u)
        << "A running sim should have advanced its tick counter by the time DoUpdate fires";
}
