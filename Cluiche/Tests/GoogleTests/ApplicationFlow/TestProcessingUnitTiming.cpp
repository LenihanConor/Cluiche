////////////////////////////////////////////////////////////////////////////////
// Filename: TestProcessingUnitTiming.cpp
// GoogleTest suite — Perf Inspector task 4 (A2b): ProcessingUnit owns a
// UnitTimingTable fed by RunForwardPass with a real steady_clock (AA3), always
// compiled in and not gated on profile categories (AA4); Sim records one entry
// per fixed step (AA10); cost per module per pass is benchmarked (AA8).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Profile/UnitTimingTable.h>

#include <chrono>
#include <cstdio>
#include <thread>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using Dia::SimTime::SimTimeContext;
using Dia::Observation::Metric::MetricRegistry;

namespace {

    constexpr float kFixed60 = 1.0f / 60.0f;

    struct TimedSimModule : SimModule
    {
        using SimModule::SimModule;
        int sleepMs = 0;
        StartResult DoStart() override { return StartResult::kReady; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void DoUpdate(const SimTimeContext&) override
        {
            if (sleepMs > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
    };

    TimedSimModule* g_timed = nullptr;
    const StringCRC kTimedTypeId("PUTiming_TimedSimModule");

    ApplicationManifestV3 BuildManifest(const char* modId, int moduleCount = 1)
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
        pu.dedicatedThread = false;
        for (int i = 0; i < moduleCount; ++i)
        {
            ModuleDeclaration mod;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s%d", modId, i);
            mod.instanceId     = StringCRC(buf);
            mod.typeId         = kTimedTypeId;
            mod.startTimeoutMs = 10000.0f;
            mod.stopTimeoutMs  = 5000.0f;
            mod.stages.Add(StringCRC("Boot"));
            pu.modules.Add(mod);
        }
        manifest.processingUnits.Add(pu);
        return manifest;
    }

    void RegisterTimed(TypeRegistry& reg)
    {
        reg.Register(kTimedTypeId, [](const StringCRC& id) -> Module*
        {
            auto* m = new TimedSimModule(id);
            if (g_timed == nullptr) g_timed = m;   // first created module
            return m;
        });
    }

    void DriveToActive(Application& app, Module* m)
    {
        for (int i = 0; i < 50 && m->GetState() != ModuleState::kActive; ++i)
            app.Update(kFixed60 * 1.1f);
        ASSERT_EQ(m->GetState(), ModuleState::kActive);
    }
} // namespace

// AA3 real clock: a 5 ms sleeper shows ~5 ms in the table (tolerant: sleep_for oversleeps).
TEST(ProcessingUnitTiming, SleepingModuleShowsRealMilliseconds)
{
    g_timed = nullptr;
    auto manifest = BuildManifest("timeSleep");
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_NE(g_timed, nullptr);
    DriveToActive(app, g_timed);

    g_timed->sleepMs = 5;
    for (int i = 0; i < 5; ++i)
        app.Update(kFixed60 * 1.1f);

    const ProcessingUnit* pu = g_timed->GetProcessingUnit();
    ASSERT_NE(pu, nullptr);
    const auto& table = pu->GetUnitTimingTable();
    EXPECT_GE(table.GetLast(0), 4.0f);
    EXPECT_LT(table.GetLast(0), 50.0f);
    EXPECT_GE(table.GetUnitLast(), table.GetLast(0) - 0.01f);
    EXPECT_EQ(table.GetModuleCrc(0), StringCRC("timeSleep0").Value());
}

// AA4: gauges update with profiling categories off (nothing in this test enables them).
TEST(ProcessingUnitTiming, ModuleEwmaGaugeUpdatesWithoutProfiling)
{
    g_timed = nullptr;
    auto manifest = BuildManifest("gaugeMod");
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    DriveToActive(app, g_timed);

    auto* gauge = MetricRegistry::Instance().FindGauge(StringCRC("pu.SimPU.module.gaugeMod0.ms"));
    ASSERT_NE(gauge, nullptr);

    g_timed->sleepMs = 5;
    for (int i = 0; i < 5; ++i)
        app.Update(kFixed60 * 1.1f);
    EXPECT_GE(gauge->Value(), 2.0);

    auto* self = MetricRegistry::Instance().FindGauge(StringCRC("perf.self_ms"));
    ASSERT_NE(self, nullptr);
    EXPECT_GE(self->Value(), 0.0);
    EXPECT_LT(self->Value(), 1.0);

    auto* tick = MetricRegistry::Instance().FindGauge(StringCRC("pu.SimPU.last_tick_ms"));
    ASSERT_NE(tick, nullptr);
    EXPECT_GE(tick->Value(), 4.0);
}

// On module stop the gauge is zeroed (and stays registered).
TEST(ProcessingUnitTiming, ModuleGaugeZeroedOnStop)
{
    g_timed = nullptr;
    auto manifest = BuildManifest("stopMod");
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    DriveToActive(app, g_timed);
    g_timed->sleepMs = 3;
    app.Update(kFixed60 * 1.1f);

    auto* gauge = MetricRegistry::Instance().FindGauge(StringCRC("pu.SimPU.module.stopMod0.ms"));
    ASSERT_NE(gauge, nullptr);
    EXPECT_GT(gauge->Value(), 0.0);

    app.RequestShutdown();
    for (int i = 0; i < 10; ++i)
        app.Update(kFixed60 * 1.1f);
    EXPECT_EQ(g_timed->GetState(), ModuleState::kInactive);
    EXPECT_EQ(gauge->Value(), 0.0);
}

// AA10: Sim adds one entry per fixed step; 0 steps adds none; catchup counter per multi-step Update().
TEST(ProcessingUnitTiming, SimEntryPerFixedStepAndCatchupCounter)
{
    g_timed = nullptr;
    auto manifest = BuildManifest("stepMod");
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    DriveToActive(app, g_timed);

    ProcessingUnit* pu = g_timed->GetProcessingUnit();
    ASSERT_NE(pu, nullptr);
    auto* catchup = MetricRegistry::Instance().FindCounter(StringCRC("pu.SimPU.catchup_updates"));
    ASSERT_NE(catchup, nullptr);

    // The banked remainder is < 1 step, so dt = 3 steps drains 3 (or 4 if the remainder crosses).
    uint32_t entries = pu->GetUnitTimingTable().GetEntryCount();
    uint64_t catches = catchup->Value();

    app.Update(3.0f * kFixed60);
    const uint32_t steps = pu->GetLastUpdateStepCount();
    EXPECT_GE(steps, 3u);
    EXPECT_LE(steps, 4u);
    EXPECT_EQ(pu->GetUnitTimingTable().GetEntryCount() - entries, steps);
    EXPECT_EQ(catchup->Value() - catches, 1u);

    // 0 steps: after the loop the remainder is < 1 step, so dt = 0 can never drain one.
    entries = pu->GetUnitTimingTable().GetEntryCount();
    catches = catchup->Value();
    app.Update(0.0f);
    EXPECT_EQ(pu->GetUnitTimingTable().GetEntryCount(), entries);
    EXPECT_EQ(catchup->Value(), catches);
    EXPECT_EQ(pu->GetLastUpdateStepCount(), 0u);
}

// AA8: cost per module per pass (reported, not gating). Build-config dependent: meaningful in Release.
TEST(ProcessingUnitTiming, BenchmarkCostPerModulePerPass)
{
    g_timed = nullptr;
    auto manifest = BuildManifest("benchMod", 16);
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    DriveToActive(app, g_timed);

    constexpr int kPasses = 2000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kPasses; ++i)
        app.Update(kFixed60 * 1.01f);
    const auto t1 = std::chrono::steady_clock::now();
    const double totalNs = std::chrono::duration<double, std::nano>(t1 - t0).count();
    std::printf("[ BENCH    ] ProcessingUnit pass: %.1f ns per module per pass (16 empty modules, includes module tick)\n",
                totalNs / (kPasses * 16.0));
    SUCCEED();
}
