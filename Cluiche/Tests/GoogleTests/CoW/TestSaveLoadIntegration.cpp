// Integration tests for E1 Save/Load (F-2 Test 5 precursor): a real Application with a 30 Hz SimPU
// holding MessageBusHost + DiaSimTime + SaveLoad + GameSpeedController + RandomStream, exercising
// the full round trip, the no-tick-burst restore rule, the frame-boundary rule and bad-load safety.
#include <gtest/gtest.h>
#include <Modules/RandomStreamModule.h>   // before anything that pulls in <windows.h> (min macro)
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/SimTime/SimTimeDomain.h>
#include <DiaSimTime/DiaSimTimeModule.h>
#include <Modules/GameSpeedControllerModule.h>
#include <Modules/GameSpeedMessages.h>
#include <Modules/MessageBusHostModule.h>
#include <Modules/SaveLoadModule.h>
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

Cluiche::AppFlow::MessageBusHostModule* g_bus  = nullptr;
GameSpeedControllerModule*              g_game = nullptr;
SaveLoadModule*                         g_save = nullptr;
RandomStreamModule*                     g_rng  = nullptr;

Module* CreateBus(const StringCRC& id)  { g_bus  = new Cluiche::AppFlow::MessageBusHostModule(id); return g_bus; }
Module* CreateGame(const StringCRC& id) { g_game = new GameSpeedControllerModule(id); return g_game; }
Module* CreateSave(const StringCRC& id) { g_save = new SaveLoadModule(id); return g_save; }
Module* CreateRng(const StringCRC& id)  { g_rng  = new RandomStreamModule(id); return g_rng; }
Module* CreateSimTime(const StringCRC& id) { return new DiaSimTimeModule(id); }

constexpr float kDt = 1.0f / 30.0f;
constexpr int kStreams = RandomStreamModule::kStreamCount;

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    Cluiche::AppFlow::MessageBusHostModule* bus = nullptr;
    GameSpeedControllerModule* game = nullptr;
    SaveLoadModule* save = nullptr;
    RandomStreamModule* rng = nullptr;
    Dia::MessageBus::BusSubscriptionHandle tickSub;
    std::vector<SimulationTick> ticks;
    std::string dir;
    std::string saveConfig;
    int frame = 0;

    explicit Fixture(const char* testName)
    {
        dir = std::string("out/test_saves/") + testName + "/";
        std::filesystem::remove_all(dir);

        reg.Register(StringCRC("MessageBusHostModule"), &CreateBus);
        reg.Register(DiaSimTimeModule::kTypeId, &CreateSimTime);
        reg.Register(StringCRC("SaveLoadModule"), &CreateSave);
        reg.Register(StringCRC("GameSpeedControllerModule"), &CreateGame);
        reg.Register(StringCRC("RandomStreamModule"), &CreateRng);

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

        ModuleDeclaration rngDecl;
        rngDecl.instanceId = StringCRC("RandomStreamModule");
        rngDecl.typeId = StringCRC("RandomStreamModule");
        rngDecl.stages.Add(StringCRC("all"));
        rngDecl.dependencies.Add(StringCRC("SaveLoadModule"));
        pu.modules.Add(rngDecl);

        manifest.processingUnits.Add(pu);
    }

    ~Fixture()
    {
        tickSub = Dia::MessageBus::BusSubscriptionHandle();
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
        g_bus = nullptr; g_game = nullptr; g_save = nullptr; g_rng = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start())
            return false;
        bus = g_bus; game = g_game; save = g_save; rng = g_rng;
        if (!bus || !game || !save || !rng)
            return false;
        for (int i = 0; i < 30; ++i)
        {
            app->Update(kDt);
            if (game->GetState() == ModuleState::kActive && save->GetState() == ModuleState::kActive
                && rng->GetState() == ModuleState::kActive)
            {
                Pump(2);   // lets the game/rng modules register their participants lazily
                tickSub = bus->GetBus().Subscribe<SimulationTick>(StringCRC("IntegrationTickSink"),
                    [this](const SimulationTick& t) { ticks.push_back(t); });
                return tickSub.IsValid();
            }
        }
        return false;
    }

    void Pump(int frames) { for (int i = 0; i < frames; ++i) { app->Update(kDt); ++frame; } }

    template <class Pred> bool PumpUntil(Pred done, int maxFrames = 20)
    {
        for (int i = 0; i < maxFrames && !done(); ++i) { app->Update(kDt); ++frame; }
        return done();
    }

    // Pumps frames, drawing a varying number of values from rotating streams each frame.
    template <class Pred> bool RunDrawingUntil(Pred done, int maxFrames)
    {
        for (int i = 0; i < maxFrames && !done(); ++i)
        {
            for (int n = 0; n < 1 + (frame % 3); ++n)
                Stream(frame % kStreams)->RandomInt();
            app->Update(kDt);
            ++frame;
        }
        return done();
    }

    void SetSpeed(SpeedTier tier)
    {
        SpeedChangeRequested m; m.tier = tier;
        ASSERT_TRUE(bus->GetBus().Broadcast(m));
    }

    Dia::SimTime::SimTimeDomain& World() { return game->GetProcessingUnit()->GetWorldDomain(); }
    uint64_t Day() const { return game->GetController().GetState().dayCounter; }
    Dia::Maths::RandomStream* Stream(int i) { return rng->GetStreamByName(StringCRC(RandomStreamModule::kStreamNames[i])); }
    std::string SlotPath(const char* slot) const { return dir + "slot_" + slot + ".sav"; }

    bool SaveSlot(const char* slot)
    {
        const uint32_t before = save->GetSaveCount();
        save->RequestSave(StringCRC(slot));
        return PumpUntil([&] { return save->GetSaveCount() == before + 1; }) && save->GetLastSaveResult().Ok();
    }
};

} // namespace

TEST(SaveLoadIntegration, FullRoundTripRestoresDayClockAndRng)
{
    Fixture f("sli_roundtrip");
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    ASSERT_TRUE(f.RunDrawingUntil([&] { return f.Day() >= 2; }, 300));
    ASSERT_FALSE(f.game->GetController().IsPaused());

    f.save->RequestSave(StringCRC("rt"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetSaveCount() == 1; }));
    ASSERT_TRUE(f.save->GetLastSaveResult().Ok());
    const uint64_t savedDay = f.Day();
    const TimeAbsolute savedClock = f.World().Now();
    ASSERT_GT(savedDay, 0u);
    ASSERT_GT(savedClock, TimeAbsolute::Zero());

    std::vector<int> expected;
    for (int i = 0; i < kStreams; ++i)
        for (int n = 0; n < 100; ++n)
            expected.push_back(f.Stream(i)->RandomInt());

    // Diverge: advance the day counter, the world clock and every RNG stream.
    ASSERT_TRUE(f.RunDrawingUntil([&] { return f.Day() >= savedDay + 2; }, 300));
    for (int i = 0; i < kStreams; ++i)
        for (int n = 0; n < 10 + i; ++n)
            f.Stream(i)->RandomInt();
    ASSERT_GT(f.World().Now(), savedClock);
    ASSERT_FALSE(f.game->GetController().IsPaused());

    f.save->RequestLoad(StringCRC("rt"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetLoadCount() == 1; }));
    ASSERT_TRUE(f.save->GetLastLoadResult().Ok());
    EXPECT_EQ(f.World().Now(), savedClock) << "world clock must be restored on the load frame";
    f.Pump(2);

    EXPECT_EQ(f.Day(), savedDay);
    EXPECT_TRUE(f.game->GetController().IsPaused());
    EXPECT_EQ(f.World().Now(), savedClock) << "world clock must stay at the saved value while paused";

    size_t k = 0;
    for (int i = 0; i < kStreams; ++i)
        for (int n = 0; n < 100; ++n, ++k)
            ASSERT_EQ(f.Stream(i)->RandomInt(), expected[k]) << "stream " << i << " draw " << n;
}

TEST(SaveLoadIntegration, NoTickBurstAfterLoadAndResumeContinuesFromSavedDay)
{
    Fixture f("sli_noburst");
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    ASSERT_TRUE(f.PumpUntil([&] { return f.Day() >= 2; }, 300));

    ASSERT_TRUE(f.SaveSlot("nb"));
    const uint64_t savedDay = f.Day();

    // Run well past the save so there are "lost" days to (not) replay.
    ASSERT_TRUE(f.PumpUntil([&] { return f.Day() >= savedDay + 3; }, 400));
    const uint64_t lostDay = f.Day();

    f.save->RequestLoad(StringCRC("nb"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetLoadCount() == 1; }));
    ASSERT_TRUE(f.save->GetLastLoadResult().Ok());

    f.ticks.clear();   // only ticks published from the load frame onwards count
    f.Pump(30);
    EXPECT_TRUE(f.ticks.empty()) << "ticks published after load while paused: " << f.ticks.size()
                                 << " (first day " << (f.ticks.empty() ? 0 : f.ticks[0].day) << ")";
    EXPECT_EQ(f.Day(), savedDay);
    EXPECT_TRUE(f.game->GetController().IsPaused());

    // Resume: ticks continue from savedDay + 1, one per day, nothing replayed.
    f.SetSpeed(SpeedTier::Fastest);
    ASSERT_TRUE(f.PumpUntil([&] { return f.ticks.size() >= 2; }, 200));
    EXPECT_EQ(f.ticks[0].day, savedDay + 1);
    EXPECT_EQ(f.ticks[1].day, savedDay + 2);
    EXPECT_LE(f.ticks[0].day, lostDay) << "sanity: resumed days overlap the lost range, so replay would be visible";
}

TEST(SaveLoadIntegration, SaveRequestedInsideBusHandlerRunsAtNextSaveLoadUpdate)
{
    Fixture f("sli_boundary");
    ASSERT_TRUE(f.Boot());

    int handlerCalls = 0;
    bool existsInHandler = true;
    bool pendingInHandler = false;
    uint32_t savesInHandler = 99;
    uint64_t tickDay = 0;
    auto sub = f.bus->GetBus().Subscribe<SimulationTick>(StringCRC("IntegrationSaveFromHandler"),
        [&](const SimulationTick& t) {
            if (handlerCalls++ != 0)
                return;
            tickDay = t.day;
            f.save->RequestSave(StringCRC("fb"));
            existsInHandler = std::filesystem::exists(f.SlotPath("fb"));
            pendingInHandler = f.save->HasPendingSave();
            savesInHandler = f.save->GetSaveCount();
        });
    ASSERT_TRUE(sub.IsValid());

    f.SetSpeed(SpeedTier::Fastest);
    ASSERT_TRUE(f.PumpUntil([&] { return handlerCalls > 0; }, 300));

    EXPECT_FALSE(existsInHandler) << "save must not touch disk inside the handler";
    EXPECT_TRUE(pendingInHandler);
    EXPECT_EQ(savesInHandler, 0u);

    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetSaveCount() == 1; }, 3)) << "save should run within a frame";
    EXPECT_TRUE(std::filesystem::exists(f.SlotPath("fb")));
    EXPECT_FALSE(f.save->HasPendingSave());

    std::ifstream in(f.SlotPath("fb"), std::ios::binary);
    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(in, root));
    ASSERT_TRUE(root.isMember("game_speed"));
    EXPECT_EQ(root["game_speed"]["day"].asInt64(), static_cast<Json::Int64>(tickDay))
        << "saved day must match the tick that triggered the request (no half-ticked state)";
    EXPECT_EQ(f.Day(), tickDay);

    sub = Dia::MessageBus::BusSubscriptionHandle();
}

TEST(SaveLoadIntegration, FailedLoadsLeaveDayClockAndRngUntouched)
{
    Fixture f("sli_badload");
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    ASSERT_TRUE(f.RunDrawingUntil([&] { return f.Day() >= 2; }, 300));
    ASSERT_TRUE(f.SaveSlot("good"));

    // Corrupted slots: raw garbage, and valid JSON that is not a save file.
    { std::ofstream out(f.SlotPath("garbage"), std::ios::binary); out << "{ this is not json \x01\x02"; }
    { std::ofstream out(f.SlotPath("nomanifest"), std::ios::binary); out << "{\"game_speed\":{\"day\":0}}"; }
    ASSERT_TRUE(std::filesystem::exists(f.SlotPath("garbage")));

    // Settle paused so the day and clock are stable to compare.
    f.SetSpeed(SpeedTier::Paused);
    f.Pump(3);
    ASSERT_TRUE(f.game->GetController().IsPaused());
    // Diverge from the "good" save so a (wrong) successful load would be visible.
    for (int i = 0; i < kStreams; ++i)
        for (int n = 0; n < 5 + i; ++n)
            f.Stream(i)->RandomInt();

    const uint64_t day = f.Day();
    const TimeAbsolute clock = f.World().Now();
    uint64_t draws[kStreams];
    unsigned seeds[kStreams];
    for (int i = 0; i < kStreams; ++i) { draws[i] = f.Stream(i)->GetDrawCount(); seeds[i] = f.Stream(i)->GetSeed(); }

    const char* const kBadSlots[] = { "missing_slot", "garbage", "nomanifest" };
    for (const char* slot : kBadSlots)
    {
        const uint32_t loadsBefore = f.save->GetLoadCount();
        f.save->RequestLoad(StringCRC(slot));
        f.Pump(3);
        EXPECT_FALSE(f.save->GetLastLoadResult().Ok()) << slot;
        EXPECT_EQ(f.save->GetLoadCount(), loadsBefore) << slot;
        EXPECT_EQ(f.Day(), day) << slot;
        EXPECT_EQ(f.World().Now(), clock) << slot;
        for (int i = 0; i < kStreams; ++i)
        {
            EXPECT_EQ(f.Stream(i)->GetDrawCount(), draws[i]) << slot << " stream " << i;
            EXPECT_EQ(f.Stream(i)->GetSeed(), seeds[i]) << slot << " stream " << i;
        }
    }

    // Draws continue as if no load happened: counts advance by exactly the draws made.
    for (int i = 0; i < kStreams; ++i)
    {
        f.Stream(i)->RandomInt();
        EXPECT_EQ(f.Stream(i)->GetDrawCount(), draws[i] + 1) << "stream " << i;
    }

    // The game still runs on from the untouched day.
    f.SetSpeed(SpeedTier::Fastest);
    ASSERT_TRUE(f.PumpUntil([&] { return f.Day() > day; }, 300));
    EXPECT_EQ(f.Day(), day + 1);
}
