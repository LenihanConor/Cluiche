////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfPause.cpp
// GoogleTest suite - perf-live-control task 3 (AC1 unit, AC3, AC11): module pause in
// ApplicationFlow. Debug-only (_DEBUG); in Release the feature is compiled out and this
// file is empty. Non-dedicated PUs tick on the test thread via Application::Update.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef _DEBUG

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/PerfPause.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Perf/PerfMarkers.h>

#include <cstdio>
#include <cstring>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using Dia::Observation::Perf::PerfMarker;
using Dia::Observation::Perf::PerfMarkers;
using Dia::SimTime::SimTimeContext;

namespace {

    constexpr float kStep = 1.0f / 60.0f;

    struct PerfPauseFixture : ::testing::Test
    {
        void SetUp() override    { PerfMarkers::RegisterThread(); }
        void TearDown() override { PerfMarkers::UnregisterThread(); }

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

    int g_nextStartTicks = 0;   // applied to counters created by the factory

    struct CounterModule : SimModule
    {
        using SimModule::SimModule;
        int updates    = 0;
        int startCalls = 0;
        int stopCalls  = 0;
        int startTicks = 0;   // DoStart returns kLoading this many times first
        StartResult DoStart() override { ++startCalls; return (startCalls <= startTicks) ? StartResult::kLoading : StartResult::kReady; }
        StopResult  DoStop()  override { ++stopCalls; return StopResult::kDone; }
        void DoUpdate(const SimTimeContext&) override { ++updates; }
    };
    const StringCRC kCounterType("PerfPause_Counter");
    // Type id of the real denylisted app module (the Dia layer cannot include app headers).
    CounterModule* g_mod[4] = {};

    void Register(TypeRegistry& reg)
    {
        reg.Register(kCounterType, [](const StringCRC& id) -> Module*
        {
            auto* m = new CounterModule(id);
            m->startTicks = g_nextStartTicks;
            for (auto& p : g_mod) if (!p) { p = m; break; }
            return m;
        });
    }

    ApplicationManifestV3 MakeManifest(const char* stageA, const char* stageB, int counters)
    {
        ApplicationManifestV3 manifest;
        manifest.version = 3;
        StageDeclaration a; a.name = StringCRC(stageA);
        manifest.stages.Add(a);
        if (stageB) { StageDeclaration b; b.name = StringCRC(stageB); manifest.stages.Add(b); }
        manifest.initialStage = StringCRC(stageA);

        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("PerfPausePU");
        pu.frequencyHz = 60.0f;
        pu.dedicatedThread = false;
        auto add = [&](const char* id, const StringCRC& type)
        {
            ModuleDeclaration m;
            m.instanceId = StringCRC(id); m.typeId = type;
            m.startTimeoutMs = 10000.0f; m.stopTimeoutMs = 5000.0f;
            m.stages.Add(StringCRC(stageA));
            if (stageB) m.stages.Add(StringCRC(stageB));
            pu.modules.Add(m);
        };
        for (int i = 0; i < counters; ++i)
        {
            char buf[32]; std::snprintf(buf, sizeof(buf), "ppMod%d", i);
            add(buf, kCounterType);
        }
        manifest.processingUnits.Add(pu);
        return manifest;
    }

    void Pump(Application& app, int n)
    {
        for (int i = 0; i < n; ++i) app.Update(kStep * 1.1f);
    }

    struct RigBase
    {
        TypeRegistry reg;
        RigBase(int startTicks)
        {
            std::memset(g_mod, 0, sizeof(g_mod));
            g_nextStartTicks = startTicks;
            Register(reg);
        }
    };

    struct Rig : RigBase
    {
        ApplicationManifestV3 manifest;
        Application app;
        Rig(ApplicationManifestV3 m, int startTicks = 0)
            : RigBase(startTicks), manifest(std::move(m)), app(manifest, reg) {}
    };
}

TEST_F(PerfPauseFixture, PauseStopsDoUpdateAndResumeContinues)
{
    Rig r(MakeManifest("PPA", nullptr, 2));
    ASSERT_TRUE(r.app.Start());
    Pump(r.app, 5);
    ASSERT_EQ(g_mod[0]->GetState(), ModuleState::kActive);
    ASSERT_GT(g_mod[0]->updates, 0);

    PerfPauseController* ctl = r.app.GetPerfPauseController();
    ASSERT_NE(ctl, nullptr);
    EXPECT_TRUE(ctl->Pause(0, PerfPauseKind::kModule, StringCRC("ppMod0")).ok);

    Pump(r.app, 1);                      // snapshot picked up within one Update
    const int frozen = g_mod[0]->updates;
    const int other  = g_mod[1]->updates;
    Pump(r.app, 10);
    EXPECT_EQ(g_mod[0]->updates, frozen);
    EXPECT_GT(g_mod[1]->updates, other);                    // other module keeps running
    EXPECT_EQ(g_mod[0]->GetState(), ModuleState::kActive);  // state untouched

    EXPECT_TRUE(ctl->Resume(0, PerfPauseKind::kModule, StringCRC("ppMod0")).ok);
    Pump(r.app, 3);
    EXPECT_GT(g_mod[0]->updates, frozen);
}

TEST_F(PerfPauseFixture, PausedModuleStopsCleanlyOnShutdown)
{
    Rig r(MakeManifest("PPB", nullptr, 1));
    ASSERT_TRUE(r.app.Start());
    Pump(r.app, 5);
    r.app.GetPerfPauseController()->Pause(0, PerfPauseKind::kModule, StringCRC("ppMod0"));
    Pump(r.app, 2);
    r.app.RequestShutdown();
    for (int i = 0; i < 100 && r.app.Update(kStep * 1.1f); ++i) {}
    EXPECT_EQ(g_mod[0]->GetState(), ModuleState::kInactive);
    EXPECT_EQ(g_mod[0]->stopCalls, 1);
}

TEST_F(PerfPauseFixture, PauseDuringStartStillCompletesStart)
{
    Rig r(MakeManifest("PPC", nullptr, 1), /*startTicks=*/5);
    ASSERT_TRUE(r.app.Start());
    ASSERT_NE(g_mod[0], nullptr);
    PerfPauseController* ctl = r.app.GetPerfPauseController();
    EXPECT_TRUE(ctl->Pause(0, PerfPauseKind::kModule, StringCRC("ppMod0")).ok);
    Pump(r.app, 30);
    EXPECT_EQ(g_mod[0]->GetState(), ModuleState::kActive);
    EXPECT_GE(g_mod[0]->startCalls, 6);
    EXPECT_EQ(g_mod[0]->updates, 0);                  // never updated while paused
    EXPECT_TRUE(ctl->Resume(0, PerfPauseKind::kModule, StringCRC("ppMod0")).ok);
    Pump(r.app, 3);
    EXPECT_GT(g_mod[0]->updates, 0);
}

// The real denylisted type ids are registered globally by other test binaries' code with a Main-only
// affinity, so this uses a standalone controller with a fake resolver instead of a live Application.
namespace {
    bool ResolveAsType(void* ctx, const StringCRC&, StringCRC& out)
    {
        out = *static_cast<const StringCRC*>(ctx);
        return true;
    }
}

TEST_F(PerfPauseFixture, DenylistedModuleRefusedWithReason)
{
    const char* names[] = { "KernelModule", "DebugServerHostModule", "SimQueryHostModule" };
    for (const char* name : names)
    {
        FrameStreamStore<PerfPauseSet> store{StringCRC("$perfpauseTest")};
        PerfPauseController ctl(&store);
        StringCRC type(name);
        ctl.SetModuleResolver(&ResolveAsType, &type);
        const PerfPauseResult res = ctl.Pause(0, PerfPauseKind::kModule, StringCRC("anyInstance"));
        EXPECT_FALSE(res.ok) << name;
        ASSERT_NE(res.reason, nullptr);
        EXPECT_NE(std::strstr(res.reason, "denylist"), nullptr);
        EXPECT_FALSE(ctl.IsPaused(PerfPauseKind::kModule, StringCRC("anyInstance")));
    }
    // A non-denylisted type is accepted by the same path.
    FrameStreamStore<PerfPauseSet> store{StringCRC("$perfpauseTest2")};
    PerfPauseController ctl(&store);
    StringCRC other("SomeGameplayModule");
    ctl.SetModuleResolver(&ResolveAsType, &other);
    EXPECT_TRUE(ctl.Pause(0, PerfPauseKind::kModule, StringCRC("anyInstance")).ok);
}

TEST_F(PerfPauseFixture, UnknownModuleFailsWithReason)
{
    Rig r(MakeManifest("PPE", nullptr, 1));
    ASSERT_TRUE(r.app.Start());
    const PerfPauseResult res = r.app.GetPerfPauseController()->Pause(0, PerfPauseKind::kModule, StringCRC("noSuchModule"));
    EXPECT_FALSE(res.ok);
    ASSERT_NE(res.reason, nullptr);
    EXPECT_NE(std::strstr(res.reason, "unknown"), nullptr);
}

TEST_F(PerfPauseFixture, ReleaseOwnerResumesOnlyThatOwnersPauses)
{
    Rig r(MakeManifest("PPF", nullptr, 2));
    ASSERT_TRUE(r.app.Start());
    Pump(r.app, 5);
    PerfPauseController* ctl = r.app.GetPerfPauseController();
    ctl->Pause(7, PerfPauseKind::kModule, StringCRC("ppMod0"));
    ctl->Pause(9, PerfPauseKind::kModule, StringCRC("ppMod1"));
    Pump(r.app, 2);
    const int a = g_mod[0]->updates, b = g_mod[1]->updates;
    Pump(r.app, 5);
    EXPECT_EQ(g_mod[0]->updates, a);
    EXPECT_EQ(g_mod[1]->updates, b);

    ctl->ReleaseOwner(7);
    Pump(r.app, 5);
    EXPECT_GT(g_mod[0]->updates, a);
    EXPECT_EQ(g_mod[1]->updates, b);
}

TEST_F(PerfPauseFixture, TwoOwnersSameModuleResumesOnlyAfterBothRelease)
{
    Rig r(MakeManifest("PPG", nullptr, 1));
    ASSERT_TRUE(r.app.Start());
    Pump(r.app, 5);
    PerfPauseController* ctl = r.app.GetPerfPauseController();
    ctl->Pause(1, PerfPauseKind::kModule, StringCRC("ppMod0"));
    ctl->Pause(2, PerfPauseKind::kModule, StringCRC("ppMod0"));
    Pump(r.app, 2);
    const int u = g_mod[0]->updates;
    ctl->ReleaseOwner(1);
    Pump(r.app, 5);
    EXPECT_EQ(g_mod[0]->updates, u);                 // owner 2 still holds it
    ctl->ResumeAll(2);
    Pump(r.app, 5);
    EXPECT_GT(g_mod[0]->updates, u);
}

TEST_F(PerfPauseFixture, LocalOwnerReleasedOnStageExit)
{
    Rig r(MakeManifest("PPH", "PPH2", 1));
    ASSERT_TRUE(r.app.Start());
    Pump(r.app, 5);
    PerfPauseController* ctl = r.app.GetPerfPauseController();
    ctl->Pause(PerfPauseController::kLocalOwner, PerfPauseKind::kModule, StringCRC("ppMod0"));
    ctl->Pause(5, PerfPauseKind::kModule, StringCRC("ppMod0"));
    EXPECT_TRUE(ctl->IsPaused(PerfPauseKind::kModule, StringCRC("ppMod0")));
    r.app.TransitionTo(StringCRC("PPH2"));
    Pump(r.app, 20);
    // Remote owner 5 survives; local owner 0 is gone.
    PerfPauseEntry e[8];
    const unsigned n = ctl->ActivePauses(e, 8);
    ASSERT_EQ(n, 1u);
    EXPECT_EQ(e[0].owner, 5u);
}

TEST_F(PerfPauseFixture, LocalOwnerReleasedOnShutdown)
{
    Rig r(MakeManifest("PPI", nullptr, 1));
    ASSERT_TRUE(r.app.Start());
    Pump(r.app, 5);
    PerfPauseController* ctl = r.app.GetPerfPauseController();
    ctl->Pause(PerfPauseController::kLocalOwner, PerfPauseKind::kModule, StringCRC("ppMod0"));
    r.app.RequestShutdown();
    for (int i = 0; i < 100 && r.app.Update(kStep * 1.1f); ++i) {}
    PerfPauseEntry e[8];
    EXPECT_EQ(ctl->ActivePauses(e, 8), 0u);
}

TEST_F(PerfPauseFixture, PauseAndResumeEmitMarkers)
{
    Rig r(MakeManifest("PPJ", nullptr, 1));
    ASSERT_TRUE(r.app.Start());
    Pump(r.app, 3);
    PerfPauseController* ctl = r.app.GetPerfPauseController();
    const uint32_t crc = StringCRC("ppMod0").Value();
    const unsigned p0 = Count("perf.pause", crc), r0 = Count("perf.resume", crc);
    ctl->Pause(0, PerfPauseKind::kModule, StringCRC("ppMod0"));
    EXPECT_EQ(Count("perf.pause", crc), p0 + 1);
    ctl->Resume(0, PerfPauseKind::kModule, StringCRC("ppMod0"));
    EXPECT_EQ(Count("perf.resume", crc), r0 + 1);
}

TEST_F(PerfPauseFixture, DomainPausePublishedInSnapshot)
{
    Rig r(MakeManifest("PPK", nullptr, 1));
    ASSERT_TRUE(r.app.Start());
    PerfPauseController* ctl = r.app.GetPerfPauseController();
    const uint32_t gen0 = ctl->GetGeneration();
    EXPECT_TRUE(ctl->Pause(3, PerfPauseKind::kDomain, StringCRC("someDomain")).ok);
    EXPECT_GT(ctl->GetGeneration(), gen0);
    EXPECT_TRUE(ctl->IsPaused(PerfPauseKind::kDomain, StringCRC("someDomain")));
    const PerfPauseSet* s = ctl->GetStore()->FetchLatest();
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->domainCount, 1u);
    EXPECT_EQ(s->domainCrcs[0], StringCRC("someDomain").Value());
}

TEST_F(PerfPauseFixture, AppReportListsActivePauses)
{
    Rig r(MakeManifest("PPL", nullptr, 1));
    // Application registers dia.app.report with a raw this-capture and never unregisters it, so a command left by an
    // earlier (destroyed) Application would be called. Drop it first so this Application re-registers its own.
    Dia::API::UnregisterCommandJson(StringCRC("dia.app.report"));
    ASSERT_TRUE(r.app.Start());
    Pump(r.app, 3);
    PerfPauseController* ctl = r.app.GetPerfPauseController();
    ASSERT_TRUE(ctl->Pause(5, PerfPauseKind::kModule, StringCRC("ppMod0")).ok);
    ASSERT_TRUE(ctl->Pause(0, PerfPauseKind::kDomain, StringCRC("someDomain")).ok);

    const Json::Value rep = Dia::API::ExecuteCommandJson(StringCRC("dia.app.report"), Json::Value(Json::objectValue));
    ASSERT_TRUE(rep["success"].asBool());
    const Json::Value& pauses = rep["data"]["activePauses"];
    ASSERT_TRUE(pauses.isArray());
    ASSERT_EQ(pauses.size(), 2u);
    bool sawModule = false, sawDomain = false;
    for (const Json::Value& e : pauses)
    {
        if (e["kind"].asString() == "module")
        {
            sawModule = true;
            EXPECT_EQ(e["owner"].asUInt(), 5u);
            EXPECT_EQ(e["id"].asString(), "ppMod0");
            EXPECT_EQ(e["crc"].asUInt(), StringCRC("ppMod0").Value());
        }
        else if (e["kind"].asString() == "domain")
        {
            sawDomain = true;
            EXPECT_EQ(e["crc"].asUInt(), StringCRC("someDomain").Value());
        }
    }
    EXPECT_TRUE(sawModule);
    EXPECT_TRUE(sawDomain);
    Dia::API::UnregisterCommandJson(StringCRC("dia.app.report"));   // do not leave a dangling this-capture
}

#endif // _DEBUG
