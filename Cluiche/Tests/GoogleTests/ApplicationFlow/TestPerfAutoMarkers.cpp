////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfAutoMarkers.cpp
// GoogleTest suite — perf-live-control task 2 (AC7): automatic PerfMarkers for stage
// transitions, module start/stop/failed and PU trips, emitted with no call-site changes.
// PerfMarkers state is process-wide, so every assertion filters by marker name AND a value
// (stage / module / unit CRC) unique to the test. Non-dedicated PUs tick on the test thread,
// which each fixture registers with PerfMarkers.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Perf/PerfMarkers.h>

#include <chrono>
#include <thread>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using Dia::Observation::Perf::PerfMarker;
using Dia::Observation::Perf::PerfMarkers;
using Dia::SimTime::SimTimeContext;

namespace {

    constexpr float kStep = 1.0f / 60.0f;

    struct PerfAutoFixture : ::testing::Test
    {
        void SetUp() override    { PerfMarkers::RegisterThread(); }
        void TearDown() override { PerfMarkers::UnregisterThread(); }

        // Number of markers named `name` whose value equals `value`.
        static unsigned int Count(const char* name, uint32_t value)
        {
            std::vector<PerfMarker> buf(PerfMarkers::kRingCapacity * PerfMarkers::kMaxThreads);
            const unsigned int n = PerfMarkers::Snapshot(buf.data(), static_cast<unsigned int>(buf.size()));
            const uint32_t crc = StringCRC(name).Value();
            unsigned int hits = 0;
            for (unsigned int i = 0; i < n; ++i)
                if (buf[i].nameCrc == crc && buf[i].hasValue && buf[i].value == value)
                    ++hits;
            return hits;
        }
    };

    struct PlainModule : Module
    {
        using Module::Module;
        StartResult DoStart() override { return StartResult::kReady; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void DoUpdate(float) override {}
    };
    const StringCRC kPlainType("PerfAuto_PlainModule");

    // DoStart never completes: used to hit the start timeout.
    struct NeverStartsModule : Module
    {
        using Module::Module;
        StartResult DoStart() override { return StartResult::kLoading; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void DoUpdate(float) override {}
    };
    const StringCRC kNeverType("PerfAuto_NeverStartsModule");

    struct SlowSimModule : SimModule
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
    const StringCRC kSlowType("PerfAuto_SlowSimModule");
    SlowSimModule* g_slow = nullptr;

    ModuleDeclaration MakeModule(const char* id, const StringCRC& type, float startTimeoutMs, const char* stageA, const char* stageB = nullptr)
    {
        ModuleDeclaration m;
        m.instanceId     = StringCRC(id);
        m.typeId         = type;
        m.startTimeoutMs = startTimeoutMs;
        m.stopTimeoutMs  = 5000.0f;
        m.stages.Add(StringCRC(stageA));
        if (stageB) m.stages.Add(StringCRC(stageB));
        return m;
    }

    ApplicationManifestV3 MakeManifest(const char* puId, const char* stageA, const char* stageB)
    {
        ApplicationManifestV3 manifest;
        manifest.version = 3;
        StageDeclaration a; a.name = StringCRC(stageA);
        manifest.stages.Add(a);
        if (stageB)
        {
            StageDeclaration b; b.name = StringCRC(stageB);
            manifest.stages.Add(b);
        }
        manifest.initialStage = StringCRC(stageA);

        ProcessingUnitDeclaration pu;
        pu.instanceId      = StringCRC(puId);
        pu.frequencyHz     = 60.0f;
        pu.dedicatedThread = false;
        manifest.processingUnits.Add(pu);
        return manifest;
    }

    void Register(TypeRegistry& reg)
    {
        reg.Register(kPlainType, [](const StringCRC& id) -> Module* { return new PlainModule(id); });
        reg.Register(kNeverType, [](const StringCRC& id) -> Module* { return new NeverStartsModule(id); });
        reg.Register(kSlowType,  [](const StringCRC& id) -> Module* { auto* m = new SlowSimModule(id); g_slow = m; return m; });
    }
} // namespace

// Thread observed registered in these tests (documented for the report): the test thread, because
// non-dedicated PUs and EmitLifecycleEvent callers run on it.
TEST_F(PerfAutoFixture, TestThreadIsRegistered)
{
    EXPECT_TRUE(PerfMarkers::IsThreadRegistered());
}

TEST_F(PerfAutoFixture, StageTransitionEmitsRequestedStartedCommitted)
{
    auto manifest = MakeManifest("PerfStagePU", "PerfAutoStageA", "PerfAutoStageB");
    manifest.processingUnits[0].modules.Add(MakeModule("perfStageMod", kPlainType, 10000.0f, "PerfAutoStageA", "PerfAutoStageB"));
    TypeRegistry reg;
    Register(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    const uint32_t stageB = StringCRC("PerfAutoStageB").Value();
    EXPECT_EQ(Count("stage.requested", stageB), 0u);
    for (int i = 0; i < 10; ++i) app.Update(kStep * 1.1f);
    app.TransitionTo(StringCRC("PerfAutoStageB"));
    for (int i = 0; i < 20; ++i) app.Update(kStep * 1.1f);

    EXPECT_GE(Count("stage.requested", stageB), 1u);
    EXPECT_GE(Count("stage.started", stageB), 1u);
    EXPECT_GE(Count("stage.committed", stageB), 1u);
}

TEST_F(PerfAutoFixture, ModuleStartAndStopEmitMarkers)
{
    auto manifest = MakeManifest("PerfModPU", "PerfAutoModStage", nullptr);
    manifest.processingUnits[0].modules.Add(MakeModule("perfStartStopMod", kPlainType, 10000.0f, "PerfAutoModStage"));
    TypeRegistry reg;
    Register(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    const uint32_t mod = StringCRC("perfStartStopMod").Value();
    EXPECT_EQ(Count("module.started", mod), 0u);
    for (int i = 0; i < 10; ++i) app.Update(kStep * 1.1f);
    EXPECT_EQ(Count("module.started", mod), 1u);
    EXPECT_EQ(Count("module.stopped", mod), 0u);

    app.RequestShutdown();
    for (int i = 0; i < 20; ++i) app.Update(kStep * 1.1f);
    EXPECT_EQ(Count("module.stopped", mod), 1u);
}

TEST_F(PerfAutoFixture, StartTimeoutEmitsModuleFailed)
{
    auto manifest = MakeManifest("PerfFailPU", "PerfAutoFailStage", nullptr);
    manifest.processingUnits[0].modules.Add(MakeModule("perfNeverStarts", kNeverType, 50.0f, "PerfAutoFailStage"));
    TypeRegistry reg;
    Register(reg);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    const uint32_t mod = StringCRC("perfNeverStarts").Value();
    EXPECT_EQ(Count("module.failed", mod), 0u);
    for (int i = 0; i < 20; ++i) app.Update(0.02f);   // 400 ms of module time >> 50 ms timeout

    EXPECT_EQ(Count("module.failed", mod), 1u);       // exactly one: the timeout path does not also emit state-changed
    EXPECT_EQ(Count("module.started", mod), 0u);
}

TEST_F(PerfAutoFixture, PuTripEmitsTripMarkerWithUnitCrc)
{
    auto manifest = MakeManifest("PerfTripPU", "PerfAutoTripStage", nullptr);
    manifest.processingUnits[0].modules.Add(MakeModule("perfTripMod", kSlowType, 10000.0f, "PerfAutoTripStage"));
    TypeRegistry reg;
    Register(reg);
    g_slow = nullptr;
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_NE(g_slow, nullptr);
    for (int i = 0; i < 50 && g_slow->GetState() != ModuleState::kActive; ++i)
        app.Update(kStep * 1.1f);
    ASSERT_EQ(g_slow->GetState(), ModuleState::kActive);

    const uint32_t unit = StringCRC("PerfTripPU").Value();
    EXPECT_EQ(Count("trip", unit), 0u);

    g_slow->sleepMs = 40;   // > 2x the 16.7 ms budget
    for (int i = 0; i < 10; ++i) app.Update(kStep * 1.1f);

    EXPECT_GE(Count("trip", unit), 1u);
}
