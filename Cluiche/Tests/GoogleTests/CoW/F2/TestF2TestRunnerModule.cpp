// Tests for CoW::F2TestRunnerModule: phase order, bus events, and phases 1-3 (Session, Save,
// ReferenceFuture) with check C4, driven by a fixed-step harness (Application::Update(1/30)) over a
// real SimPU holding MessageBusHost + DiaSimTime + SaveLoad + GameSpeedController + RandomStream.
#include <gtest/gtest.h>
#include <Modules/RandomStreamModule.h>   // before Application headers: RandomStream.h needs std::min/max unmacroed
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaSimTime/DiaSimTimeModule.h>
#include <Modules/F2TestRunnerModule.h>
#include <Modules/GameSpeedControllerModule.h>
#include <Modules/GameSpeedMessages.h>
#include <Modules/MessageBusHostModule.h>
#include <Modules/SaveLoadModule.h>
#include <DiaCore/Json/external/json/json.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace CoW;
using Dia::SimTime::DiaSimTimeModule;

namespace {

Cluiche::AppFlow::MessageBusHostModule* g_bus = nullptr;
F2TestRunnerModule* g_runner = nullptr;
RandomStreamModule* g_random = nullptr;
GameSpeedControllerModule* g_game = nullptr;
SaveLoadModule* g_save = nullptr;

Module* CreateBus(const StringCRC& id) { g_bus = new Cluiche::AppFlow::MessageBusHostModule(id); return g_bus; }
Module* CreateRunner(const StringCRC& id) { g_runner = new F2TestRunnerModule(id); return g_runner; }
Module* CreateRandom(const StringCRC& id) { g_random = new RandomStreamModule(id); return g_random; }
Module* CreateGame(const StringCRC& id) { g_game = new GameSpeedControllerModule(id); return g_game; }
Module* CreateSave(const StringCRC& id) { g_save = new SaveLoadModule(id); return g_save; }
Module* CreateSimTime(const StringCRC& id) { return new DiaSimTimeModule(id); }

constexpr float kDt = 1.0f / 30.0f;
constexpr int kMaxFrames = 3000;   // phases 0-3 need ~1,010 sim ticks

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    F2TestRunnerModule* runner = nullptr;
    Dia::MessageBus::BusSubscriptionHandle phaseSub, completedSub, checkSub;
    std::vector<F2CheckCompleted> checks;
    std::vector<F2PhaseId> phases;
    std::vector<F2ScenarioCompleted> completions;
    std::string dir;
    std::string saveConfig;

    explicit Fixture(const char* testName)
    {
        dir = std::string("out/test_saves/") + testName + "/";
        std::filesystem::remove_all(dir);

        reg.Register(StringCRC("MessageBusHostModule"), &CreateBus);
        reg.Register(DiaSimTimeModule::kTypeId, &CreateSimTime);
        reg.Register(StringCRC("SaveLoadModule"), &CreateSave);
        reg.Register(StringCRC("GameSpeedControllerModule"), &CreateGame);
        reg.Register(StringCRC("RandomStreamModule"), &CreateRandom);
        reg.Register(StringCRC("F2TestRunnerModule"), &CreateRunner);

        manifest.version = 3;
        StageDeclaration run; run.name = StringCRC("Run");
        manifest.stages.Add(run);
        manifest.initialStage = StringCRC("Run");

        StreamDeclaration simTime;
        simTime.id = StringCRC("SimTime");
        simTime.kind = StringCRC("FrameStream");
        manifest.streams.Add(simTime);
        StreamDeclaration fire;
        fire.id = StringCRC("SimTimeSchedulerFire");
        fire.kind = StringCRC("EventStream");
        manifest.streams.Add(fire);

        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("SimPU");
        pu.frequencyHz = 30.0f;
        pu.dedicatedThread = false;

        ModuleDeclaration busDecl;
        busDecl.instanceId = StringCRC("MessageBusHostModule");
        busDecl.typeId = StringCRC("MessageBusHostModule");
        busDecl.stages.Add(StringCRC("all"));
        pu.modules.Add(busDecl);

        ModuleDeclaration stDecl;
        stDecl.instanceId = DiaSimTimeModule::kTypeId;
        stDecl.typeId = DiaSimTimeModule::kTypeId;
        stDecl.stages.Add(StringCRC("all"));
        pu.modules.Add(stDecl);

        ModuleDeclaration saveDecl;
        saveDecl.instanceId = StringCRC("SaveLoadModule");
        saveDecl.typeId = StringCRC("SaveLoadModule");
        saveDecl.stages.Add(StringCRC("all"));
        saveConfig = std::string("{\"base_directory\":\"") + dir + "\"}";
        saveDecl.configJson = saveConfig.c_str();
        pu.modules.Add(saveDecl);

        ModuleDeclaration gameDecl;
        gameDecl.instanceId = StringCRC("GameSpeedControllerModule");
        gameDecl.typeId = StringCRC("GameSpeedControllerModule");
        gameDecl.stages.Add(StringCRC("all"));
        gameDecl.dependencies.Add(StringCRC("MessageBusHostModule"));
        gameDecl.dependencies.Add(StringCRC("SaveLoadModule"));
        pu.modules.Add(gameDecl);

        ModuleDeclaration randDecl;
        randDecl.instanceId = StringCRC("RandomStreamModule");
        randDecl.typeId = StringCRC("RandomStreamModule");
        randDecl.stages.Add(StringCRC("all"));
        randDecl.dependencies.Add(StringCRC("SaveLoadModule"));
        pu.modules.Add(randDecl);

        ModuleDeclaration decl;
        decl.instanceId = StringCRC("F2TestRunnerModule");
        decl.typeId = StringCRC("F2TestRunnerModule");
        decl.stages.Add(StringCRC("all"));
        decl.dependencies.Add(StringCRC("MessageBusHostModule"));
        decl.dependencies.Add(StringCRC("RandomStreamModule"));
        decl.dependencies.Add(StringCRC("GameSpeedControllerModule"));
        decl.dependencies.Add(StringCRC("SaveLoadModule"));
        pu.modules.Add(decl);

        manifest.processingUnits.Add(pu);
    }

    ~Fixture()
    {
        phaseSub = Dia::MessageBus::BusSubscriptionHandle();
        completedSub = Dia::MessageBus::BusSubscriptionHandle();
        checkSub = Dia::MessageBus::BusSubscriptionHandle();
        if (app)
        {
            app->RequestShutdown();
            for (int i = 0; i < 50 && app->Update(kDt); ++i) {}
        }
        app.reset();
        std::filesystem::remove_all(dir);
    }

    bool Boot()
    {
        g_bus = nullptr;
        g_runner = nullptr;
        g_random = nullptr;
        g_game = nullptr;
        g_save = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start() || !g_bus || !g_runner || !g_random || !g_game || !g_save)
            return false;
        runner = g_runner;
        for (int i = 0; i < 30; ++i)
        {
            app->Update(kDt);
            if (runner->GetState() == ModuleState::kActive && g_game->GetState() == ModuleState::kActive
                && g_save->GetState() == ModuleState::kActive && g_random->GetState() == ModuleState::kActive)
            {
                app->Update(kDt);   // lets the game/rng modules register their save participants
                app->Update(kDt);
                phaseSub = g_bus->GetBus().Subscribe<F2PhaseEntered>(StringCRC("TestPhaseSink"),
                    [this](const F2PhaseEntered& m) { phases.push_back(m.phase); });
                completedSub = g_bus->GetBus().Subscribe<F2ScenarioCompleted>(StringCRC("TestDoneSink"),
                    [this](const F2ScenarioCompleted& m) { completions.push_back(m); });
                checkSub = g_bus->GetBus().Subscribe<F2CheckCompleted>(StringCRC("TestCheckSink"),
                    [this](const F2CheckCompleted& m) { checks.push_back(m); });
                return checkSub.IsValid() && phaseSub.IsValid() && completedSub.IsValid();
            }
        }
        return false;
    }

    void Pump(int frames) { for (int i = 0; i < frames; ++i) app->Update(kDt); }

    template <class Pred> bool PumpUntil(Pred done, int maxFrames = kMaxFrames)
    {
        for (int i = 0; i < maxFrames && !done(); ++i)
            app->Update(kDt);
        return done();
    }

    bool PumpToPhase(F2PhaseId phase) { return PumpUntil([&] { return runner->GetCurrentPhase() == phase; }); }
    std::string SlotPath(const char* slot) const { return dir + "slot_" + slot + ".sav"; }
};

// Everything phases 1-3 produce, for run-to-run comparison.
struct Phases1to3Result
{
    bool reached = false;
    F2CheckStatus c4 = F2CheckStatus::NotRun;
    std::string c4Summary;
    F2Fingerprint f500, f1000;
    bool hasF500 = false, hasF1000 = false, hasDraws = false, saveOk = false;
    std::array<int, kF2ReferenceDrawCount> draws = {};
};

Phases1to3Result RunPhases1to3(const char* testName)
{
    Phases1to3Result r;
    Fixture f(testName);
    if (!f.Boot() || !f.runner->RunScenario())
        return r;
    r.reached = f.PumpToPhase(F2PhaseId::Reload);
    r.c4 = f.runner->GetCheckStatus(F2CheckId::ClockProof);
    r.c4Summary = f.runner->GetLastResult(F2CheckId::ClockProof);
    r.f500 = f.runner->GetFingerprint500();
    r.f1000 = f.runner->GetFingerprint1000();
    r.hasF500 = f.runner->HasFingerprint500();
    r.hasF1000 = f.runner->HasFingerprint1000();
    r.hasDraws = f.runner->HasReferenceDraws();
    r.saveOk = f.runner->SaveSucceeded();
    r.draws = f.runner->GetReferenceDraws();
    return r;
}

} // namespace

TEST(F2TestRunnerModule, ChecksStartNotRun)
{
    Fixture f("f2r_notrun");
    ASSERT_TRUE(f.Boot());
    for (int i = 0; i < kF2CheckCount; ++i)
    {
        EXPECT_EQ(f.runner->GetCheckStatus(static_cast<F2CheckId>(i)), F2CheckStatus::NotRun) << i;
        EXPECT_STREQ(f.runner->GetLastResult(static_cast<F2CheckId>(i)), "") << i;
    }
}

TEST(F2TestRunnerModule, PhaseStartsIdle)
{
    Fixture f("f2r_idle");
    ASSERT_TRUE(f.Boot());
    f.Pump(5);
    EXPECT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::Idle);
    EXPECT_TRUE(f.phases.empty());
}

TEST(F2TestRunnerModule, PhasesVisitedInOrder)
{
    Fixture f("f2r_order");
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    EXPECT_FALSE(f.runner->RunScenario());   // already in progress

    f.Pump(1);
    EXPECT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::Isolation);
    for (int expected = 1; expected < kF2PhaseCount; ++expected)
    {
        ASSERT_TRUE(f.PumpToPhase(static_cast<F2PhaseId>(expected))) << expected;
    }
}

TEST(F2TestRunnerModule, SessionAndReferenceFutureSpanManyTicks)
{
    Fixture f("f2r_span");
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    f.Pump(2);
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::Session));
    f.Pump(100);
    EXPECT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::Session) << "phase 1 must still be running after 100 updates";
    EXPECT_GE(f.runner->GetScenarioTick(), 90u);
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::ReferenceFuture));
    EXPECT_GE(f.runner->GetScenarioTick(), kF2SessionTicks);   // phase 3 may already have run tick 500
    EXPECT_LE(f.runner->GetScenarioTick(), kF2SessionTicks + 1);
    f.Pump(100);
    EXPECT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::ReferenceFuture);
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::Reload));
    EXPECT_EQ(f.runner->GetScenarioTick(), kF2ScenarioTicks);
}

TEST(F2TestRunnerModule, EventsFireInOrderAndScenarioCompletesOnce)
{
    Fixture f("f2r_events");
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    ASSERT_TRUE(f.PumpUntil([&] { return !f.completions.empty(); }));
    f.Pump(5);

    ASSERT_EQ(f.phases.size(), 8u);
    for (int i = 0; i < kF2PhaseCount; ++i)
        EXPECT_EQ(static_cast<int>(f.phases[i]), i);

    ASSERT_EQ(f.completions.size(), 1u);
    EXPECT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::Idle);
    // C1, C3, C4 are resolved so far; C5 stays Running (part a done, part b pending); C2, C6 NotRun.
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::StreamIndependence), F2CheckStatus::Pass);
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::ClockProof), F2CheckStatus::Pass);
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::SaveLoadRng), F2CheckStatus::Pass);
    EXPECT_EQ(f.completions[0].passCount, 3u);
    EXPECT_EQ(f.completions[0].failCount, 0u);
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::Determinism), F2CheckStatus::NotRun);
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::SaveLoadProof), F2CheckStatus::Running);
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::BandResolver), F2CheckStatus::NotRun);
}

TEST(F2TestRunnerModule, IsolationResolvesC1StreamIndependence)
{
    Fixture f("f2r_c1");
    ASSERT_TRUE(f.Boot());
    ASSERT_NE(g_random, nullptr);

    // Snapshot live stream state; the check must not disturb it.
    uint64_t counts[RandomStreamModule::kStreamCount];
    for (int i = 0; i < RandomStreamModule::kStreamCount; ++i)
        counts[i] = g_random->GetStreamByName(StringCRC(RandomStreamModule::kStreamNames[i]))->GetDrawCount();

    ASSERT_TRUE(f.runner->RunScenario());
    f.Pump(1);   // Isolation runs on the first update (the session, which draws, starts on the second)

    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::StreamIndependence), F2CheckStatus::Pass);
    EXPECT_STREQ(f.runner->GetLastResult(F2CheckId::StreamIndependence), "8 / 8 streams independent over 64 draws");
    for (int i = 0; i < RandomStreamModule::kStreamCount; ++i)
        EXPECT_EQ(g_random->GetStreamByName(StringCRC(RandomStreamModule::kStreamNames[i]))->GetDrawCount(), counts[i]) << i;

    f.Pump(1);   // bus delivery of the check event
    ASSERT_GE(f.checks.size(), 1u);
    EXPECT_EQ(f.checks[0].check, F2CheckId::StreamIndependence);
    EXPECT_TRUE(f.checks[0].passed);
    EXPECT_STREQ(f.checks[0].summary, "8 / 8 streams independent over 64 draws");
}

TEST(F2TestRunnerModule, SessionResolvesC4ClockProof)
{
    Fixture f("f2r_c4");
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::Save));
    f.Pump(2);

    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::ClockProof), F2CheckStatus::Pass) << f.runner->GetLastResult(F2CheckId::ClockProof);
    const std::string expected = std::to_string(F2SessionScript().size()) + " / " + std::to_string(F2SessionScript().size()) + " transitions match";
    EXPECT_NE(std::string(f.runner->GetLastResult(F2CheckId::ClockProof)).find(expected), std::string::npos)
        << f.runner->GetLastResult(F2CheckId::ClockProof);

    bool published = false;
    for (const F2CheckCompleted& c : f.checks)
        if (c.check == F2CheckId::ClockProof) { published = true; EXPECT_TRUE(c.passed); }
    EXPECT_TRUE(published);

    // The session leaves the clock stopped and modal-free for the save.
    EXPECT_TRUE(g_game->GetController().IsPaused());
    EXPECT_EQ(g_game->GetController().GetState().modalDepth, 0);
}

TEST(F2TestRunnerModule, SeededClockFaultFailsC4AndNamesTheMismatch)
{
    Fixture f("f2r_c4fault");
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    // Tick 130 pauses; between 140 and 150 an unscripted ModalOpened sneaks onto the bus, so the
    // controller's modalDepth no longer matches the reference model at the tick-150 manual step.
    ASSERT_TRUE(f.PumpUntil([&] { return f.runner->GetScenarioTick() >= 140; }));
    ASSERT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::Session);
    ASSERT_TRUE(g_bus->GetBus().Broadcast(ModalOpened{}));
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::Save));
    f.Pump(2);

    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::ClockProof), F2CheckStatus::Fail);
    const std::string summary = f.runner->GetLastResult(F2CheckId::ClockProof);
    EXPECT_NE(summary.find("modalDepth"), std::string::npos) << summary;
    EXPECT_NE(summary.find("ManualStep"), std::string::npos) << summary;
}

TEST(F2TestRunnerModule, SavePhaseWritesQuickSaveAndCapturesF500)
{
    Fixture f("f2r_save");
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::Save));
    EXPECT_FALSE(std::filesystem::exists(f.SlotPath("quick")));
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::ReferenceFuture));

    EXPECT_TRUE(f.runner->SaveSucceeded());
    EXPECT_TRUE(std::filesystem::exists(f.SlotPath("quick")));
    ASSERT_TRUE(f.runner->HasFingerprint500());
    const F2Fingerprint& fp = f.runner->GetFingerprint500();
    EXPECT_GT(fp.streams[0].drawCount + fp.streams[1].drawCount + fp.streams[2].drawCount + fp.streams[3].drawCount, 0u);
    EXPECT_EQ(fp.speedTier, SpeedTier::Paused);
    EXPECT_EQ(fp.modalDepth, 0);
    EXPECT_GT(fp.dayCounter, 0u) << "the session runs the clock, so days have passed by tick 500";
    EXPECT_EQ(fp.dayCounter, g_game->GetController().GetState().dayCounter) << "clock is stopped, nothing moves during the save";
}

TEST(F2TestRunnerModule, ReferenceFutureCapturesF1000AndEightHundredDraws)
{
    Fixture f("f2r_future");
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::Reload));

    ASSERT_TRUE(f.runner->HasFingerprint500());
    ASSERT_TRUE(f.runner->HasFingerprint1000());
    ASSERT_TRUE(f.runner->HasReferenceDraws());
    const F2Fingerprint& f500 = f.runner->GetFingerprint500();
    const F2Fingerprint& f1000 = f.runner->GetFingerprint1000();
    EXPECT_FALSE(f500 == f1000);
    EXPECT_GT(f1000.dayCounter, f500.dayCounter) << "the remainder runs the clock";
    for (int i = 0; i < kF2FingerprintStreamCount; ++i)
        EXPECT_GT(f1000.streams[i].drawCount, f500.streams[i].drawCount) << i;

    // F1000 was captured before the reference draws; the live streams then advanced by exactly 100 each.
    for (int i = 0; i < RandomStreamModule::kStreamCount; ++i)
    {
        Dia::Maths::RandomStream* s = g_random->GetStreamByName(StringCRC(RandomStreamModule::kStreamNames[i]));
        EXPECT_EQ(s->GetDrawCount(), f1000.streams[i].drawCount + kF2ReferenceDrawsPerStream) << i;
    }

    // The 800 values really are the next 100 draws of each stream from the F1000 point.
    const auto& draws = f.runner->GetReferenceDraws();
    for (int i = 0; i < RandomStreamModule::kStreamCount; ++i)
    {
        Dia::Maths::RandomStream probe(f1000.streams[i].seed);
        probe.Restore(f1000.streams[i].seed, f1000.streams[i].drawCount);
        for (int n = 0; n < kF2ReferenceDrawsPerStream; ++n)
            ASSERT_EQ(draws[i * kF2ReferenceDrawsPerStream + n], probe.RandomInt()) << "stream " << i << " draw " << n;
    }
}

TEST(F2TestRunnerModule, TwoRunsFromTheSameSeedProduceIdenticalPhases1To3)
{
    const Phases1to3Result a = RunPhases1to3("f2r_det_a");
    const Phases1to3Result b = RunPhases1to3("f2r_det_b");
    ASSERT_TRUE(a.reached);
    ASSERT_TRUE(b.reached);
    EXPECT_EQ(a.c4, F2CheckStatus::Pass) << a.c4Summary;
    EXPECT_EQ(b.c4, F2CheckStatus::Pass) << b.c4Summary;
    ASSERT_TRUE(a.hasF500 && b.hasF500 && a.hasF1000 && b.hasF1000 && a.hasDraws && b.hasDraws);
    EXPECT_TRUE(a.saveOk);
    EXPECT_TRUE(b.saveOk);
    EXPECT_EQ(F2Fingerprint::Compare(a.f500, b.f500).Summary(), "identical");
    EXPECT_EQ(F2Fingerprint::Compare(a.f1000, b.f1000).Summary(), "identical");
    EXPECT_EQ(a.draws, b.draws);
}

namespace {

// Adds `delta` to the saved draw count of stream `name` inside the quick-save JSON.
bool TamperDrawCount(const std::string& path, const char* name, int delta)
{
    Json::Value root;
    {
        std::ifstream in(path, std::ios::binary);
        Json::CharReaderBuilder b;
        std::string errs;
        if (!Json::parseFromStream(b, in, &root, &errs))
            return false;
    }
    bool done = false;
    struct Walk
    {
        static void Run(Json::Value& v, const char* name, int delta, bool& done)
        {
            if (!v.isObject())
                return;
            if (v.isMember(name) && v[name].isObject() && v[name].isMember("draws"))
            {
                v[name]["draws"] = Json::Value(static_cast<Json::Int64>(v[name]["draws"].asInt64() + delta));
                done = true;
                return;
            }
            for (const std::string& k : v.getMemberNames())
                Run(v[k], name, delta, done);
        }
    };
    Walk::Run(root, name, delta, done);
    if (!done)
        return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    Json::StreamWriterBuilder w;
    out << Json::writeString(w, root);
    return true;
}

} // namespace

TEST(F2TestRunnerModule, ReloadAndReplayPassC5aAndC3)
{
    Fixture f("f2r_replay");
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::Rerun));

    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::SaveLoadRng), F2CheckStatus::Pass) << f.runner->GetLastResult(F2CheckId::SaveLoadRng);
    EXPECT_STREQ(f.runner->GetLastResult(F2CheckId::SaveLoadRng), "800 / 800 draws match, state identical");
    EXPECT_TRUE(f.runner->HasC5aResult());
    EXPECT_TRUE(f.runner->C5aPassed()) << f.runner->GetC5aSummary();
    // C5 as a whole waits for part b (task 6).
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::SaveLoadProof), F2CheckStatus::Running);
    EXPECT_EQ(F2Fingerprint::Compare(f.runner->GetFingerprint1000(), f.runner->GetReplayFingerprint1000()).Summary(), "identical");
    EXPECT_GE(g_save->GetLoadCount(), 1u);
}

TEST(F2TestRunnerModule, CorruptedSaveFailsC5aAndC3NamingTheStream)
{
    Fixture f("f2r_corrupt");
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::ReferenceFuture));   // quick save exists now
    const char* stream = RandomStreamModule::kStreamNames[2];
    ASSERT_TRUE(TamperDrawCount(f.SlotPath("quick"), stream, 7));
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::Rerun));

    EXPECT_FALSE(f.runner->C5aPassed());
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::SaveLoadProof), F2CheckStatus::Fail);
    EXPECT_NE(std::string(f.runner->GetLastResult(F2CheckId::SaveLoadProof)).find(std::string("stream ") + stream + " drawCount"), std::string::npos)
        << f.runner->GetLastResult(F2CheckId::SaveLoadProof);
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::SaveLoadRng), F2CheckStatus::Fail);
    EXPECT_NE(std::string(f.runner->GetLastResult(F2CheckId::SaveLoadRng)).find(stream), std::string::npos)
        << f.runner->GetLastResult(F2CheckId::SaveLoadRng);
}

TEST(F2TestRunnerModule, PerturbedReplayFailsC3)
{
    Fixture f("f2r_perturb");
    ASSERT_TRUE(f.Boot());
    ASSERT_TRUE(f.runner->RunScenario());
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::Replay));
    ASSERT_TRUE(f.PumpUntil([&] { return f.runner->GetScenarioTick() >= 700; }));
    ASSERT_EQ(f.runner->GetCurrentPhase(), F2PhaseId::Replay);
    const char* stream = RandomStreamModule::kStreamNames[5];
    g_random->GetStreamByName(StringCRC(stream))->RandomInt();   // one extra draw mid-replay
    ASSERT_TRUE(f.PumpToPhase(F2PhaseId::Rerun));

    EXPECT_TRUE(f.runner->C5aPassed()) << "the perturbation happens after the reload";
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::SaveLoadRng), F2CheckStatus::Fail);
    EXPECT_NE(std::string(f.runner->GetLastResult(F2CheckId::SaveLoadRng)).find(stream), std::string::npos)
        << f.runner->GetLastResult(F2CheckId::SaveLoadRng);
    EXPECT_EQ(f.runner->GetCheckStatus(F2CheckId::SaveLoadProof), F2CheckStatus::Running);
}
