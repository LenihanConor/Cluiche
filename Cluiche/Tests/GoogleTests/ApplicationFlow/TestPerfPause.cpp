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

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
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

// ---- perf-live-control task 10: gap audit ----
namespace {
    bool ResolveAlwaysPlain(void*, const StringCRC&, StringCRC& out) { out = StringCRC("PlainModule"); return true; }

    struct StandaloneCtl
    {
        FrameStreamStore<PerfPauseSet> store{StringCRC("$perfpauseAudit")};
        PerfPauseController ctl{&store};
        StandaloneCtl() { ctl.SetModuleResolver(&ResolveAlwaysPlain, nullptr); }
        static StringCRC Mod(unsigned i) { char b[24]; std::snprintf(b, sizeof(b), "auditMod%u", i); return StringCRC(b); }
    };
}

TEST_F(PerfPauseFixture, PauseTwiceSameOwner_OneEntryOneMarkerNoGenerationBump)
{
    StandaloneCtl c;
    const uint32_t crc = StringCRC("twice").Value();
    const unsigned p0 = Count("perf.pause", crc);
    EXPECT_TRUE(c.ctl.Pause(4, PerfPauseKind::kModule, StringCRC("twice")).ok);
    const uint32_t gen = c.ctl.GetGeneration();
    EXPECT_TRUE(c.ctl.Pause(4, PerfPauseKind::kModule, StringCRC("twice")).ok);
    EXPECT_EQ(c.ctl.GetGeneration(), gen);
    EXPECT_EQ(Count("perf.pause", crc), p0 + 1);
    PerfPauseEntry e[4];
    EXPECT_EQ(c.ctl.ActivePauses(e, 4), 1u);
    EXPECT_TRUE(c.ctl.Resume(4, PerfPauseKind::kModule, StringCRC("twice")).ok);   // one resume is enough
    EXPECT_FALSE(c.ctl.IsPaused(PerfPauseKind::kModule, StringCRC("twice")));
}

TEST_F(PerfPauseFixture, ResumeWithoutPauseFailsAndPublishesNothing)
{
    StandaloneCtl c;
    const uint32_t gen = c.ctl.GetGeneration();
    const PerfPauseResult r = c.ctl.Resume(1, PerfPauseKind::kModule, StringCRC("never"));
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.reason, nullptr);
    EXPECT_EQ(c.ctl.GetGeneration(), gen);
}

TEST_F(PerfPauseFixture, ReleaseOwnerAfterAlreadyResumedIsNoOp)
{
    StandaloneCtl c;
    c.ctl.Pause(8, PerfPauseKind::kModule, StringCRC("a"));
    c.ctl.Resume(8, PerfPauseKind::kModule, StringCRC("a"));
    const uint32_t gen = c.ctl.GetGeneration();
    c.ctl.ReleaseOwner(8);          // disconnect after the client resumed everything
    c.ctl.ResumeAll(77);            // owner that never paused
    EXPECT_EQ(c.ctl.GetGeneration(), gen);
    PerfPauseEntry e[2];
    EXPECT_EQ(c.ctl.ActivePauses(e, 2), 0u);
}

TEST_F(PerfPauseFixture, ModuleSetFull_RefusesNewIdWithReason_ExistingStillWork)
{
    StandaloneCtl c;
    for (unsigned i = 0; i < PerfPauseSet::kMaxModules; ++i)
        ASSERT_TRUE(c.ctl.Pause(1, PerfPauseKind::kModule, StandaloneCtl::Mod(i)).ok) << i;
    const PerfPauseResult r = c.ctl.Pause(1, PerfPauseKind::kModule, StandaloneCtl::Mod(PerfPauseSet::kMaxModules));
    EXPECT_FALSE(r.ok);
    ASSERT_NE(r.reason, nullptr);
    EXPECT_NE(std::strstr(r.reason, "full"), nullptr);
    EXPECT_FALSE(c.ctl.IsPaused(PerfPauseKind::kModule, StandaloneCtl::Mod(PerfPauseSet::kMaxModules)));
    // Published set holds exactly the 32, nothing truncated silently.
    EXPECT_EQ(c.ctl.GetStore()->FetchLatest()->moduleCount, PerfPauseSet::kMaxModules);
    // A second owner may still join an id already in the set.
    EXPECT_TRUE(c.ctl.Pause(2, PerfPauseKind::kModule, StandaloneCtl::Mod(0)).ok);
    // After a resume there is room again.
    EXPECT_TRUE(c.ctl.Resume(1, PerfPauseKind::kModule, StandaloneCtl::Mod(5)).ok);
    EXPECT_TRUE(c.ctl.Pause(1, PerfPauseKind::kModule, StandaloneCtl::Mod(PerfPauseSet::kMaxModules)).ok);
}

TEST_F(PerfPauseFixture, DomainSetFull_RefusesSeventeenthWithReason)
{
    StandaloneCtl c;
    char b[24];
    for (unsigned i = 0; i < PerfPauseSet::kMaxDomains; ++i)
    {
        std::snprintf(b, sizeof(b), "dom%u", i);
        ASSERT_TRUE(c.ctl.Pause(1, PerfPauseKind::kDomain, StringCRC(b)).ok) << i;
    }
    const PerfPauseResult r = c.ctl.Pause(1, PerfPauseKind::kDomain, StringCRC("domOverflow"));
    EXPECT_FALSE(r.ok);
    ASSERT_NE(r.reason, nullptr);
    EXPECT_FALSE(c.ctl.IsPaused(PerfPauseKind::kDomain, StringCRC("domOverflow")));
    EXPECT_EQ(c.ctl.GetStore()->FetchLatest()->domainCount, PerfPauseSet::kMaxDomains);
}

TEST_F(PerfPauseFixture, EntryTableFull_RefusesEvenForKnownId_ThenRecovers)
{
    StandaloneCtl c;
    for (unsigned i = 0; i < PerfPauseSet::kMaxModules; ++i)
    {
        ASSERT_TRUE(c.ctl.Pause(1, PerfPauseKind::kModule, StandaloneCtl::Mod(i)).ok);
        ASSERT_TRUE(c.ctl.Pause(2, PerfPauseKind::kModule, StandaloneCtl::Mod(i)).ok);
    }
    PerfPauseEntry all[PerfPauseController::kMaxEntries + 1];
    ASSERT_EQ(c.ctl.ActivePauses(all, PerfPauseController::kMaxEntries + 1), PerfPauseController::kMaxEntries);
    const PerfPauseResult r = c.ctl.Pause(3, PerfPauseKind::kModule, StandaloneCtl::Mod(0));
    EXPECT_FALSE(r.ok);
    ASSERT_NE(r.reason, nullptr);
    EXPECT_NE(std::strstr(r.reason, "table full"), nullptr);
    EXPECT_EQ(c.ctl.ActivePauses(all, PerfPauseController::kMaxEntries + 1), PerfPauseController::kMaxEntries);
    c.ctl.ReleaseOwner(2);
    EXPECT_TRUE(c.ctl.Pause(3, PerfPauseKind::kModule, StandaloneCtl::Mod(0)).ok);
}

// AC11 on another PU: a module on a dedicated thread stops updating after the pause and resumes afterwards.
// Counts are atomic because the module runs on the PU thread.
namespace {
    struct AtomicCounterModule : SimModule
    {
        using SimModule::SimModule;
        std::atomic<int> updates{0};
        StartResult DoStart() override { return StartResult::kReady; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void DoUpdate(const SimTimeContext&) override { updates.fetch_add(1); }
    };
    const StringCRC kAtomicType("PerfPause_AtomicCounter");
    AtomicCounterModule* g_atomicMod = nullptr;

    void PumpFor(Application& app, int ms)
    {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < end)
        {
            app.Update(kStep);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    bool WaitUntil(Application& app, bool (*pred)(), int timeoutMs)
    {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < end)
        {
            app.Update(kStep);
            if (pred()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return pred();
    }
}

TEST_F(PerfPauseFixture, DedicatedThreadPU_PausedModuleStopsUpdatingAndResumes)
{
    TypeRegistry reg;
    g_atomicMod = nullptr;
    reg.Register(kAtomicType, [](const StringCRC& id) -> Module* { auto* m = new AtomicCounterModule(id); g_atomicMod = m; return m; });

    ApplicationManifestV3 manifest;
    manifest.version = 3;
    StageDeclaration st; st.name = StringCRC("PPD");
    manifest.stages.Add(st);
    manifest.initialStage = StringCRC("PPD");
    ProcessingUnitDeclaration pu;
    pu.instanceId = StringCRC("PerfPauseDedicatedPU");
    pu.frequencyHz = 200.0f;
    pu.dedicatedThread = true;
    ModuleDeclaration m;
    m.instanceId = StringCRC("ppDed0"); m.typeId = kAtomicType;
    m.startTimeoutMs = 10000.0f; m.stopTimeoutMs = 5000.0f;
    m.stages.Add(StringCRC("PPD"));
    pu.modules.Add(m);
    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(WaitUntil(app, [] { return g_atomicMod && g_atomicMod->updates.load() > 5; }, 5000));

    PerfPauseController* ctl = app.GetPerfPauseController();
    ASSERT_TRUE(ctl->Pause(0, PerfPauseKind::kModule, StringCRC("ppDed0")).ok);
    PumpFor(app, 100);   // PU picks up the snapshot on its next Update
    const int frozen = g_atomicMod->updates.load();
    PumpFor(app, 300);
    EXPECT_EQ(g_atomicMod->updates.load(), frozen);

    ASSERT_TRUE(ctl->Resume(0, PerfPauseKind::kModule, StringCRC("ppDed0")).ok);
    PumpFor(app, 300);
    EXPECT_GT(g_atomicMod->updates.load(), frozen);

    app.RequestShutdown();
    for (int i = 0; i < 2000 && app.Update(kStep); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

// Overhead of the per-pass pause check (reported, never gating): Application::Update with no pauses, with a
// domain pause set (table rebuilt, nothing skipped) and with one module paused (one update skipped).
TEST_F(PerfPauseFixture, BenchmarkPassCostWithAndWithoutPausesReported)
{
    Rig r(MakeManifest("PPM", nullptr, 16));
    ASSERT_TRUE(r.app.Start());
    Pump(r.app, 10);
    PerfPauseController* ctl = r.app.GetPerfPauseController();
    const int kIters = 20000;
    auto run = [&]() {
        Pump(r.app, 50);
        const auto t0 = std::chrono::steady_clock::now();
        Pump(r.app, kIters);
        return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / kIters;
    };
    const double none = run();
    ctl->Pause(0, PerfPauseKind::kDomain, StringCRC("benchDomain"));
    const double domain = run();
    ctl->Pause(0, PerfPauseKind::kModule, StringCRC("ppMod0"));
    const double module = run();
    std::printf("[ PERF ] Application::Update, 16 modules: no pauses %.0f ns, domain pause set %.0f ns, 1 module paused %.0f ns (reported, not gating)\n",
                none, domain, module);
    SUCCEED();
}

#endif // _DEBUG
