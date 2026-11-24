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
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/PerfSummary.h>
#include <DiaObservation/Profile/TripChunk.h>
#include <DiaObservation/Profile/TripReporter.h>
#include <DiaObservation/Profile/UnitTimingTable.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
        static inline int startSleepMs = 0;   // slept inside DoStart (runs while the module is kStarting)
        StartResult DoStart() override
        {
            if (startSleepMs > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(startSleepMs));
            return StartResult::kReady;
        }
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

// Audit (task 11): a catch-up burst hitting maxCatchUpTicksPerFrame (default 5) adds exactly the steps run
// (5), counts one catchup_updates, and the excess backlog is dropped, not deferred.
TEST(ProcessingUnitTiming, SimCatchupCapHitAddsStepsRunAndCountsOnce)
{
    g_timed = nullptr;
    auto manifest = BuildManifest("capMod");
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    DriveToActive(app, g_timed);

    ProcessingUnit* pu = g_timed->GetProcessingUnit();
    ASSERT_NE(pu, nullptr);
    auto* catchup = MetricRegistry::Instance().FindCounter(StringCRC("pu.SimPU.catchup_updates"));
    ASSERT_NE(catchup, nullptr);

    const uint32_t entries = pu->GetUnitTimingTable().GetEntryCount();
    const uint64_t catches = catchup->Value();
    app.Update(20.0f * kFixed60);
    EXPECT_EQ(pu->GetLastUpdateStepCount(), 5u);
    EXPECT_EQ(pu->GetUnitTimingTable().GetEntryCount() - entries, 5u);
    EXPECT_EQ(catchup->Value() - catches, 1u);

    // Backlog was dropped: an immediate zero-dt update runs nothing.
    app.Update(0.0f);
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

// ---------------------------------------------------------------------------
// Perf Inspector A3c (AA7, AA10, AA11): trip detector wired into the ProcessingUnit.
// ---------------------------------------------------------------------------
namespace {

    class CaptureSink : public Dia::Observation::Log::ISink
    {
    public:
        CaptureSink() { SetLevelThreshold(Dia::Observation::Log::LogLevel::kDebug); }
        void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mMessages.emplace_back(e.message);
        }
        const char* GetName() const override { return "PUTripCaptureSink"; }
        int Count(const char* needle) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            int n = 0;
            for (const auto& m : mMessages)
                if (m.find(needle) != std::string::npos) ++n;
            return n;
        }
    private:
        mutable std::mutex       mMutex;
        std::vector<std::string> mMessages;
    };

    struct PUTripFixture : ::testing::Test
    {
        void SetUp() override
        {
            Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
            Dia::Observation::Log::Logger::Instance().RegisterSink(&sink);
            g_timed = nullptr;
            TimedSimModule::startSleepMs = 0;
        }
        void TearDown() override
        {
            TimedSimModule::startSleepMs = 0;
            Dia::Observation::Log::Logger::Instance().UnregisterSink(&sink);
            Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
        }
        void Flush() { Dia::Observation::Log::Logger::Instance().FlushSync(); }

        CaptureSink sink;
        Dia::Observation::Profile::TripReporter reporter;   // not started: counts-only, still logs pu.trip
    };
}

// Many over-budget entries: one rate-limited trip, counter counts every over-budget entry, and the
// per-frame pu.over_budget log line is gone.
TEST_F(PUTripFixture, SustainedOverBudgetEmitsOneTripNoPerFrameLog)
{
    auto manifest = BuildManifest("tripSleep");
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ProcessingUnit* pu = g_timed->GetProcessingUnit();
    pu->SetTripReporter(&reporter);
    DriveToActive(app, g_timed);

    auto* over = MetricRegistry::Instance().FindCounter(StringCRC("pu.SimPU.over_budget_frames"));
    ASSERT_NE(over, nullptr);
    const uint64_t overBefore = over->Value();
    const uint64_t submittedBefore = reporter.GetSubmitted();

    g_timed->sleepMs = 40;   // > 2x the 16.7 ms budget: every step is a candidate trip
    constexpr int kUpdates = 15;
    for (int i = 0; i < kUpdates; ++i)
        app.Update(kFixed60 * 1.1f);
    Flush();

    EXPECT_GE(over->Value() - overBefore, static_cast<uint64_t>(kUpdates));
    EXPECT_EQ(reporter.GetSubmitted() - submittedBefore, 1u);   // minIntervalMs (5 s) >> test duration
    EXPECT_EQ(sink.Count("pu.trip"), 1);
    EXPECT_EQ(sink.Count("pu.over_budget"), 0);
}

// Detaching the reporter (null) keeps the counter but emits nothing.
TEST_F(PUTripFixture, NullReporterCountsOnly)
{
    auto manifest = BuildManifest("tripNull");
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ProcessingUnit* pu = g_timed->GetProcessingUnit();
    pu->SetTripReporter(nullptr);
    DriveToActive(app, g_timed);

    auto* over = MetricRegistry::Instance().FindCounter(StringCRC("pu.SimPU.over_budget_frames"));
    ASSERT_NE(over, nullptr);
    const uint64_t overBefore = over->Value();

    g_timed->sleepMs = 40;
    for (int i = 0; i < 3; ++i)
        app.Update(kFixed60 * 1.1f);
    Flush();

    EXPECT_GE(over->Value() - overBefore, 3u);
    EXPECT_GE(pu->GetTripDetector().GetTripCount(), 1u);
    EXPECT_EQ(reporter.GetSubmitted(), 0u);
    EXPECT_EQ(sink.Count("pu.trip"), 0);
}

// AA10: a Sim Update() running several fixed steps, each within the per-step budget, never trips
// (the budget is per step, not per real call).
TEST_F(PUTripFixture, SimMultiStepWithinPerStepBudgetDoesNotTrip)
{
    auto manifest = BuildManifest("tripSteps");
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ProcessingUnit* pu = g_timed->GetProcessingUnit();
    pu->SetTripReporter(&reporter);
    DriveToActive(app, g_timed);

    const uint32_t overBefore = pu->GetTripDetector().GetOverBudgetFrames();
    g_timed->sleepMs = 3;   // 3 steps x ~3 ms = ~9 ms real call, but each step is far under 16.7 ms
    app.Update(3.0f * kFixed60);
    EXPECT_GE(pu->GetLastUpdateStepCount(), 3u);
    EXPECT_EQ(pu->GetTripDetector().GetOverBudgetFrames(), overBefore);
    EXPECT_EQ(pu->GetTripDetector().GetTripCount(), 0u);
    EXPECT_EQ(reporter.GetSubmitted(), 0u);
}

// A3: entries while a module is kStarting (here a 60 ms DoStart) and the unit's first entry are skipped.
TEST_F(PUTripFixture, StartingModuleEntryIsSkipped)
{
    auto manifest = BuildManifest("tripStart");
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ProcessingUnit* pu = g_timed->GetProcessingUnit();
    pu->SetTripReporter(&reporter);

    TimedSimModule::startSleepMs = 60;   // >> 2x budget, but only inside the kStarting pass
    DriveToActive(app, g_timed);

    EXPECT_EQ(pu->GetTripDetector().GetOverBudgetFrames(), 0u);
    EXPECT_EQ(pu->GetTripDetector().GetTripCount(), 0u);
    EXPECT_EQ(reporter.GetSubmitted(), 0u);
}


// ---------------------------------------------------------------------------
// Perf Inspector A4b (AA9): per-unit perf summary rows outlive stopped modules.
// ---------------------------------------------------------------------------
namespace {
    struct PlainMainModule : Module
    {
        using Module::Module;
        StartResult DoStart() override { return StartResult::kReady; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void DoUpdate(float) override {}
    };
    const StringCRC kPlainTypeId("PUTiming_PlainMainModule");

    ApplicationManifestV3 BuildTwoPuTwoStageManifest()
    {
        ApplicationManifestV3 manifest;
        manifest.version = 3;
        StageDeclaration boot; boot.name = StringCRC("Boot");
        StageDeclaration run;  run.name  = StringCRC("Run");
        manifest.stages.Add(boot);
        manifest.stages.Add(run);
        manifest.initialStage = StringCRC("Boot");

        auto addMod = [](ProcessingUnitDeclaration& pu, const char* id, const StringCRC& type, const char* stage1, const char* stage2)
        {
            ModuleDeclaration m;
            m.instanceId = StringCRC(id);
            m.typeId = type;
            m.startTimeoutMs = 10000.0f;
            m.stopTimeoutMs = 5000.0f;
            if (stage1) m.stages.Add(StringCRC(stage1));
            if (stage2) m.stages.Add(StringCRC(stage2));
            pu.modules.Add(m);
        };

        ProcessingUnitDeclaration sim;
        sim.instanceId = StringCRC("SimPU"); sim.frequencyHz = 60.0f; sim.dedicatedThread = false;
        addMod(sim, "perfSimBoot", kTimedTypeId, "Boot", nullptr);
        addMod(sim, "perfSimRun",  kTimedTypeId, "Run", nullptr);
        manifest.processingUnits.Add(sim);

        ProcessingUnitDeclaration main;
        main.instanceId = StringCRC("PerfMainPU"); main.frequencyHz = 60.0f; main.dedicatedThread = false;
        addMod(main, "perfMainBoot", kPlainTypeId, "Boot", nullptr);
        addMod(main, "perfMainBoth", kPlainTypeId, "Boot", "Run");
        manifest.processingUnits.Add(main);
        return manifest;
    }

    const Dia::Observation::Profile::PerfModuleRow* FindRow(const Dia::Observation::Profile::PerfUnitSummary& u, const char* name)
    {
        for (uint32_t i = 0; i < u.modules.GetCount(); ++i)
            if (std::strcmp(u.modules.GetRow(i).name, name) == 0)
                return &u.modules.GetRow(i);
        return nullptr;
    }
}

TEST(PerfSummaryProcessingUnit, StoppedModuleKeepsRowAfterLaterModuleStarts_EveryModuleOfEveryUnitPresent)
{
    using namespace Dia::Observation::Profile;
    g_timed = nullptr;
    TypeRegistry reg;
    RegisterTimed(reg);
    reg.Register(kPlainTypeId, [](const StringCRC& id) -> Module* { return new PlainMainModule(id); });

    auto manifest = BuildTwoPuTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    for (int i = 0; i < 30; ++i) app.Update(kFixed60 * 1.1f);
    ProcessingUnit* simPu = g_timed->GetProcessingUnit();
    ASSERT_NE(simPu, nullptr);
    Module* simBoot = simPu->FindModule(StringCRC("perfSimBoot"));
    ASSERT_NE(simBoot, nullptr);
    ASSERT_EQ(simBoot->GetState(), ModuleState::kActive);

    app.TransitionTo(StringCRC("Run"));
    Module* simRun = simPu->FindModule(StringCRC("perfSimRun"));
    ASSERT_NE(simRun, nullptr);
    for (int i = 0; i < 60 && simRun->GetState() != ModuleState::kActive; ++i)
        app.Update(kFixed60 * 1.1f);
    ASSERT_EQ(simRun->GetState(), ModuleState::kActive);
    ASSERT_EQ(simBoot->GetState(), ModuleState::kInactive);

    // The stopped module's row is already banked in the unit's accumulator (not just the live table).
    bool banked = false;
    for (uint32_t i = 0; i < simPu->GetPerfSummaryAccumulator().GetCount(); ++i)
        if (std::strcmp(simPu->GetPerfSummaryAccumulator().GetRow(i).name, "perfSimBoot") == 0)
            banked = simPu->GetPerfSummaryAccumulator().GetRow(i).entryCount > 0;
    EXPECT_TRUE(banked);

    // Collect through the Application control interface (as ObservationModule does).
    PerfSummary summary;
    static_cast<IApplicationControl&>(app).CollectPerfSummary(summary);
    ASSERT_EQ(summary.units.size(), 2u);

    const PerfUnitSummary* sim = nullptr;
    const PerfUnitSummary* mainU = nullptr;
    for (const PerfUnitSummary& u : summary.units)
    {
        if (std::strcmp(u.unit, "SimPU") == 0) sim = &u;
        if (std::strcmp(u.unit, "PerfMainPU") == 0) mainU = &u;
    }
    ASSERT_NE(sim, nullptr);
    ASSERT_NE(mainU, nullptr);

    const PerfModuleRow* boot = FindRow(*sim, "perfSimBoot");
    const PerfModuleRow* run  = FindRow(*sim, "perfSimRun");
    ASSERT_NE(boot, nullptr);
    ASSERT_NE(run, nullptr);
    EXPECT_GT(boot->entryCount, 0u);
    EXPECT_EQ(sim->modules.GetCount(), 2u);
    EXPECT_GT(sim->entryCount, 0u);
    EXPECT_FLOAT_EQ(sim->budgetMs, 1000.0f / 60.0f);

    EXPECT_NE(FindRow(*mainU, "perfMainBoot"), nullptr);
    EXPECT_NE(FindRow(*mainU, "perfMainBoth"), nullptr);
    EXPECT_EQ(mainU->modules.GetCount(), 2u);
    EXPECT_EQ(sim->tripCount, simPu->GetTripDetector().GetTripCount());
    EXPECT_EQ(sim->overBudgetFrames, simPu->GetTripDetector().GetOverBudgetFrames());
    EXPECT_EQ(sim->suppressedCount, simPu->GetTripDetector().GetSuppressedCount());
}

// Perf Inspector task 3b: a trip produces exactly one TripChunk on "$perftrips" (independent of any
// TripReporter), and the unit's cumulative tripCount shows up in a later coarse chunk.
namespace { struct SLOW_PUTripChunkFixture : PUTripFixture {}; }
TEST_F(SLOW_PUTripChunkFixture, TripSendsOneTripChunkAndCoarseTripCount)
{
    using namespace Dia::Observation::Profile;
    auto manifest = BuildManifest("tripChunk");
    TypeRegistry reg;
    RegisterTimed(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_NE(app.GetPerfTripsStore(), nullptr);
    const int tripReader   = app.GetPerfTripsStore()->RegisterReader();
    const int coarseReader = app.GetPerfCoarseStore()->RegisterReader();
    ASSERT_GE(tripReader, 0);
    ASSERT_GE(coarseReader, 0);
    DriveToActive(app, g_timed);

    g_timed->sleepMs = 40;   // > 2x the 16.7 ms budget
    for (int i = 0; i < 15; ++i)
        app.Update(kFixed60 * 1.1f);

    const uint32_t simCrc = StringCRC("SimPU").Value();
    auto trips = std::make_unique<Dia::Core::Containers::DynamicArrayC<Event<TripChunk>, 4>>();
    int tripEvents = 0;
    for (int pass = 0; pass < 32; ++pass)
    {
        trips->RemoveAll();
        app.GetPerfTripsStore()->Consume(tripReader, *trips);
        if (trips->Size() == 0) break;
        for (unsigned int i = 0; i < trips->Size(); ++i)
        {
            const TripChunk& c = (*trips)[i].payload;
            if (c.unitCrc != simCrc) continue;
            ++tripEvents;
            EXPECT_EQ(1u, c.seq);
            EXPECT_STREQ("SimPU", c.record.unit);
            EXPECT_GE(c.record.peakMs, 33.0f);
            EXPECT_LE(c.record.firstFrame, c.record.lastFrame);
        }
    }
    EXPECT_EQ(1, tripEvents);

    auto coarse = std::make_unique<Dia::Core::Containers::DynamicArrayC<Event<CoarseChunk>, 4>>();
    uint32_t lastTripCount = 0;
    for (int pass = 0; pass < 32; ++pass)
    {
        coarse->RemoveAll();
        app.GetPerfCoarseStore()->Consume(coarseReader, *coarse);
        if (coarse->Size() == 0) break;
        for (unsigned int i = 0; i < coarse->Size(); ++i)
            if ((*coarse)[i].payload.unitCrc == simCrc)
                lastTripCount = (*coarse)[i].payload.tripCount;
    }
    EXPECT_EQ(1u, lastTripCount);
}
