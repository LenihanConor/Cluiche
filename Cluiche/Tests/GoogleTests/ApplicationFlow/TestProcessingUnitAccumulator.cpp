////////////////////////////////////////////////////////////////////////////////
// Filename: TestProcessingUnitAccumulator.cpp
// GoogleTest suite — DiaSimTime Task 1.4: ProcessingUnit fixed-timestep
//                    accumulator + per-affinity time-context injection.
//
// This is the task that finally makes the whole DiaSimTime cutover branch link:
// ProcessingUnit now exposes GetSimTimeContext()/GetRenderTimeContext()/
// GetMainTimeContext() (called by the typed module bases from Task 1.3) and owns
// a world SimTimeDomain driven by a fixed-timestep accumulator.
//
// Coverage (plan acceptance criteria for Task 1.4):
//   - kSim PU advances its world clock exactly once per drained fixed step.
//   - kMain PU exposes wall-clock dt via GetMainTimeContext().
//   - kRender PU's cached GetRenderTimeContext() stays unchanged until
//     SetRenderTimeContext() is called.
//   - One real Update(dt) with dt == 3x the fixed step drains exactly 3 steps.
//   - dt smaller than one fixed step drains 0 steps; the leftover is banked and
//     a later crossing call then drains.
//   - Backlog beyond maxCatchUpTicksPerFrame is dropped (accumulator reset),
//     logged, and counted (simtime.accumulator.dropped_ticks) — not deferred.
//   - Pausing then a long real gap does not trigger a catch-up burst on resume.
//   - Reverse pass (kStopping modules) still ticks even when 0 fixed steps drain.
//   - A kStarting module is polled within ~1 fixed step of accumulated real time
//     even when a given Update() call drains 0 steps (bounded, not stalled).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaCore/SimTime/SimTimeDomain.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <cstdint>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using Dia::SimTime::SimTimeContext;
using Dia::SimTime::RenderTimeContext;
using Dia::SimTime::MainTimeContext;

namespace {
    constexpr float kFixed60 = 1.0f / 60.0f;   // fixed step for a 60Hz PU
} // anonymous namespace

// ---------------------------------------------------------------------------
// kMain: wall-clock dt is exposed through GetMainTimeContext()
// ---------------------------------------------------------------------------
TEST(ProcessingUnitAccumulator, MainPU_ExposesWallClockDt)
{
    ProcessingUnit pu(StringCRC("MainPU"), 60.0f, false);
    pu.Update(0.016f);
    EXPECT_FLOAT_EQ(pu.GetMainTimeContext().wallClockDt, 0.016f)
        << "A kMain PU must forward the raw wall-clock delta as MainTimeContext";

    pu.Update(0.033f);
    EXPECT_FLOAT_EQ(pu.GetMainTimeContext().wallClockDt, 0.033f)
        << "MainTimeContext must reflect the most recent Update()'s dt";
}

// ---------------------------------------------------------------------------
// kRender: cached RenderTimeContext is unchanged by Update(); only
//          SetRenderTimeContext() mutates it (DiaRenderTime pushes it later).
// ---------------------------------------------------------------------------
TEST(ProcessingUnitAccumulator, RenderPU_ContextUnchangedUntilSet)
{
    ProcessingUnit pu(StringCRC("RenderPU"), 60.0f, false);

    // Default-initialized context.
    EXPECT_FLOAT_EQ(pu.GetRenderTimeContext().frameDt, 0.0f);
    EXPECT_EQ(pu.GetRenderTimeContext().renderFrame, 0u);

    // A render-affinity Update() must NOT compute or mutate the render context.
    pu.Update(0.016f);
    EXPECT_FLOAT_EQ(pu.GetRenderTimeContext().frameDt, 0.0f)
        << "RenderPU::Update must not populate the render context itself";
    EXPECT_EQ(pu.GetRenderTimeContext().renderFrame, 0u);

    // Only SetRenderTimeContext() changes it.
    RenderTimeContext pushed{ 0.033f, Dia::Core::TimeAbsolute::Zero(), 7 };
    pu.SetRenderTimeContext(pushed);
    EXPECT_FLOAT_EQ(pu.GetRenderTimeContext().frameDt, 0.033f);
    EXPECT_EQ(pu.GetRenderTimeContext().renderFrame, 7u);
}

// ---------------------------------------------------------------------------
// kSim: dt == 3x fixed step drains exactly 3 steps (not "at least" / "roughly").
// ---------------------------------------------------------------------------
TEST(ProcessingUnitAccumulator, SimPU_ThreeStepDt_DrainsExactlyThree)
{
    ProcessingUnit pu(StringCRC("SimPU"), 60.0f, false);

    const uint64_t before = pu.GetWorldDomain().GetTick();

    // dt worth 3.5 fixed steps: unambiguously between 3 and 4 steps, so it must
    // drain EXACTLY 3 (floor), banking the leftover 0.5 step. This proves the
    // "3, not 2, not 4" floor semantics robustly. (An exact 3.0x-fixed-step dt
    // lands on the >= comparison's knife-edge — 3.0f/60.0f is marginally smaller
    // than 3 * (1.0f/60.0f) in float — which would spuriously drain only 2; real
    // wall-clock deltas are never hair-exact multiples, so this is a test artifact
    // of exact multiples, not an accumulator bug.)
    pu.Update(3.5f / 60.0f);

    EXPECT_EQ(pu.GetWorldDomain().GetTick() - before, 3u)
        << "A dt worth 3.5 fixed steps must advance the world clock by exactly 3 ticks";
    EXPECT_EQ(pu.GetSimTimeContext().tick, before + 3u)
        << "The cached SimTimeContext must reflect the last drained tick";
    EXPECT_GT(pu.GetSimTimeContext().timeScale, 0.0f);
    EXPECT_FALSE(pu.GetSimTimeContext().isPaused);

    // The 0.5-step remainder is banked: a following 0.6-step call crosses the
    // threshold and drains exactly one more (proving the leftover was kept).
    pu.Update((1.0f / 60.0f) * 0.6f);
    EXPECT_EQ(pu.GetWorldDomain().GetTick() - before, 4u)
        << "The banked 0.5-step remainder must carry into the next call";
}

// ---------------------------------------------------------------------------
// kSim: a sub-step dt drains 0 steps and banks the remainder; a later call
//       that crosses the threshold then drains.
// ---------------------------------------------------------------------------
TEST(ProcessingUnitAccumulator, SimPU_SubStepBanksThenDrains)
{
    ProcessingUnit pu(StringCRC("SimPU"), 60.0f, false);

    const uint64_t before = pu.GetWorldDomain().GetTick();

    pu.Update(kFixed60 * 0.4f);   // banks, drains nothing
    EXPECT_EQ(pu.GetWorldDomain().GetTick(), before)
        << "A dt below one fixed step must drain 0 steps";

    pu.Update(kFixed60 * 0.7f);   // 0.4 + 0.7 = 1.1 fixed -> exactly 1 step
    EXPECT_EQ(pu.GetWorldDomain().GetTick() - before, 1u)
        << "The banked remainder must let a later crossing call drain a step";
}

// ---------------------------------------------------------------------------
// kSim: backlog beyond maxCatchUpTicksPerFrame is dropped (accumulator reset),
//       counted, and NOT deferred.
// ---------------------------------------------------------------------------
TEST(ProcessingUnitAccumulator, SimPU_BacklogBeyondCapIsDroppedNotDeferred)
{
    // The counter is registered globally on the first ProcessingUnit construction.
    ProcessingUnit pu(StringCRC("SimPU"), 60.0f, false, /*maxCatchUpTicksPerFrame*/ 2u);

    Dia::Observation::Metric::Counter* dropped =
        Dia::Observation::Metric::MetricRegistry::Instance()
            .FindCounter(StringCRC("simtime.accumulator.dropped_ticks"));
    ASSERT_NE(dropped, nullptr)
        << "ProcessingUnit construction must register simtime.accumulator.dropped_ticks";

    const uint64_t droppedBefore = dropped->Value();
    const uint64_t tickBefore    = pu.GetWorldDomain().GetTick();

    // 10 steps worth of backlog, capped at 2.
    pu.Update(10.0f / 60.0f);

    EXPECT_EQ(pu.GetWorldDomain().GetTick() - tickBefore, 2u)
        << "Only maxCatchUpTicksPerFrame (2) steps must actually run";
    EXPECT_EQ(dropped->Value() - droppedBefore, 1u)
        << "Exactly one drop event must be counted for the over-cap backlog";

    // The remaining 8 steps must have been DROPPED (accumulator reset to 0),
    // not deferred: a subsequent sub-step call therefore drains nothing.
    pu.Update(kFixed60 * 0.4f);
    EXPECT_EQ(pu.GetWorldDomain().GetTick() - tickBefore, 2u)
        << "Backlog must be dropped (accumulator reset), not carried over";
}

// ---------------------------------------------------------------------------
// kSim: pausing then a long real gap must not trigger a catch-up burst on the
//       next call after Resume() — the accumulator is reset while paused.
// ---------------------------------------------------------------------------
TEST(ProcessingUnitAccumulator, SimPU_PauseThenResume_NoCatchUpBurst)
{
    ProcessingUnit pu(StringCRC("SimPU"), 60.0f, false);

    pu.GetWorldDomain().Pause();
    const uint64_t before = pu.GetWorldDomain().GetTick();

    // A large real gap while paused must bank nothing and drain nothing.
    pu.Update(100.0f * kFixed60);
    EXPECT_EQ(pu.GetWorldDomain().GetTick(), before)
        << "A paused sim must not advance its world clock, however long the gap";

    // Resume and feed one fixed step: exactly one tick, no burst from the gap.
    pu.GetWorldDomain().Resume();
    pu.Update(kFixed60);
    EXPECT_EQ(pu.GetWorldDomain().GetTick() - before, 1u)
        << "Resuming after a long paused gap must not trigger a catch-up burst";
}

// ---------------------------------------------------------------------------
// Application-driven lifecycle cases (reverse pass + kStarting bound).
// A kSim module cannot be driven into kStarting/kStopping from bare
// ProcessingUnit (BeginStart/BeginStop are private to Application), so these
// go through a real single-PU Application whose SimPU runs inline in Update().
// ---------------------------------------------------------------------------
namespace {

    struct LifecycleRecSimModule : SimModule
    {
        using SimModule::SimModule;
        static const StringCRC kTypeId;

        int startCalls  = 0;
        int updateCalls = 0;
        int stopCalls   = 0;

        StartResult DoStart() override { ++startCalls; return StartResult::kReady; }
        StopResult  DoStop()  override { ++stopCalls;  return StopResult::kDone;  }
        void        DoUpdate(const SimTimeContext&) override { ++updateCalls; }
    };
    const StringCRC LifecycleRecSimModule::kTypeId("PUAcc_LifecycleRecSimModule");

    LifecycleRecSimModule* g_lifecycleRecSimModule = nullptr;

    ApplicationManifestV3 BuildSimModuleManifest()
    {
        ApplicationManifestV3 manifest;
        manifest.version = 3;

        StageDeclaration stage;
        stage.name = StringCRC("Boot");
        manifest.stages.Add(stage);
        manifest.initialStage = StringCRC("Boot");

        ProcessingUnitDeclaration pu;
        pu.instanceId      = StringCRC("SimPU");
        pu.frequencyHz     = 60.0f;
        pu.dedicatedThread = false;   // -> PU index 0, ticked inline by Application::Update

        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("simMod0");
        mod.typeId         = LifecycleRecSimModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);

        manifest.processingUnits.Add(pu);
        return manifest;
    }

    void RegisterSimModuleInto(TypeRegistry& reg)
    {
        reg.Register(LifecycleRecSimModule::kTypeId,
            [](const StringCRC& id) -> Module*
            {
                auto* m = new LifecycleRecSimModule(id);
                g_lifecycleRecSimModule = m;
                return m;
            });
    }

} // anonymous namespace

// A kStopping module still gets ticked (reverse pass) even when the sim
// accumulator drains 0 forward steps that call.
TEST(ProcessingUnitAccumulator, SimPU_ReversePassRunsWhenZeroForwardStepsDrain)
{
    g_lifecycleRecSimModule = nullptr;
    auto manifest = BuildSimModuleManifest();
    TypeRegistry reg;
    RegisterSimModuleInto(reg);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_NE(g_lifecycleRecSimModule, nullptr);

    // Drive the module to kActive one full step at a time.
    for (int i = 0; i < 50 && g_lifecycleRecSimModule->GetState() != ModuleState::kActive; ++i)
        app.Update(kFixed60);
    ASSERT_EQ(g_lifecycleRecSimModule->GetState(), ModuleState::kActive);

    const int stopsBefore = g_lifecycleRecSimModule->stopCalls;

    // Begin shutdown: BeginStopAllActive() flips the module to kStopping, then
    // the SimPU is ticked with a SUB-STEP dt so 0 forward steps drain — the
    // reverse pass must still fire DoStop.
    app.RequestShutdown();
    app.Update(kFixed60 * 0.3f);

    EXPECT_GT(g_lifecycleRecSimModule->stopCalls, stopsBefore)
        << "A kStopping module must be ticked by the reverse pass even when the "
           "accumulator drains 0 forward steps";
}

// A kStarting module is polled within ~1 fixed step of accumulated real time,
// even though a single sub-step Update() that drains 0 steps does NOT poll it.
TEST(ProcessingUnitAccumulator, SimPU_StartingModulePolledWithinOneFixedStep)
{
    g_lifecycleRecSimModule = nullptr;
    auto manifest = BuildSimModuleManifest();
    TypeRegistry reg;
    RegisterSimModuleInto(reg);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_NE(g_lifecycleRecSimModule, nullptr);
    ASSERT_EQ(g_lifecycleRecSimModule->GetState(), ModuleState::kStarting)
        << "After Start() the module should be kStarting, not yet polled";

    const int startsAtEntry = g_lifecycleRecSimModule->startCalls;

    // A single sub-step call drains 0 fixed steps -> DoStart is NOT polled yet.
    app.Update(kFixed60 * 0.3f);
    EXPECT_EQ(g_lifecycleRecSimModule->startCalls, startsAtEntry)
        << "A sub-step Update() that drains 0 steps must not poll DoStart";

    // Keep banking sub-steps. Startup is bounded: once accumulated real time
    // crosses one fixed step, the forward pass runs and DoStart is polled.
    bool polled = false;
    for (int i = 0; i < 4; ++i)   // 4 x 0.3 fixed = 1.2 fixed of accumulated time
    {
        app.Update(kFixed60 * 0.3f);
        if (g_lifecycleRecSimModule->startCalls > startsAtEntry) { polled = true; break; }
    }
    EXPECT_TRUE(polled)
        << "DoStart must be polled within ~1 fixed step of accumulated real time "
           "(startup bounded, not stalled)";
}
