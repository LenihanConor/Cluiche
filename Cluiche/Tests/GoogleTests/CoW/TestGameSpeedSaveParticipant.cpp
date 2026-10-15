// Tests for the GameSpeedControllerModule save participant (E1): only the day counter persists,
// restore always resumes paused. Real Application: MessageBusHost + SaveLoad + GameSpeedController.
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaSaveGame/LoadContext.h>
#include <Modules/GameSpeedControllerModule.h>
#include <Modules/MessageBusHostModule.h>
#include <Modules/SaveLoadModule.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace CoW;

namespace {

Cluiche::AppFlow::MessageBusHostModule* g_bus  = nullptr;
GameSpeedControllerModule*              g_game = nullptr;
SaveLoadModule*                         g_save = nullptr;

Module* CreateBus(const StringCRC& id)  { g_bus  = new Cluiche::AppFlow::MessageBusHostModule(id); return g_bus; }
Module* CreateGame(const StringCRC& id) { g_game = new GameSpeedControllerModule(id); return g_game; }
Module* CreateSave(const StringCRC& id) { g_save = new SaveLoadModule(id); return g_save; }

constexpr float kDt = 1.0f / 30.0f;

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    GameSpeedControllerModule* game = nullptr;
    SaveLoadModule* save = nullptr;
    Cluiche::AppFlow::MessageBusHostModule* bus = nullptr;
    std::string dir;
    std::string saveConfig;

    explicit Fixture(const char* testName)
    {
        dir = std::string("out/test_saves/") + testName + "/";
        std::filesystem::remove_all(dir);

        reg.Register(StringCRC("MessageBusHostModule"), &CreateBus);
        reg.Register(StringCRC("GameSpeedControllerModule"), &CreateGame);
        reg.Register(StringCRC("SaveLoadModule"), &CreateSave);

        manifest.version = 3;
        StageDeclaration run; run.name = StringCRC("Run");
        manifest.stages.Add(run);
        manifest.initialStage = StringCRC("Run");

        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("SimPU");
        pu.frequencyHz = 30.0f;
        pu.dedicatedThread = false;

        ModuleDeclaration busDecl;
        busDecl.instanceId = StringCRC("MessageBusHostModule");
        busDecl.typeId = StringCRC("MessageBusHostModule");
        busDecl.stages.Add(StringCRC("all"));
        pu.modules.Add(busDecl);

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

        manifest.processingUnits.Add(pu);
    }

    ~Fixture()
    {
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
        g_bus = nullptr; g_game = nullptr; g_save = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start())
            return false;
        bus = g_bus; game = g_game; save = g_save;
        if (!bus || !game || !save)
            return false;
        for (int i = 0; i < 20; ++i)
        {
            app->Update(kDt);
            if (game->GetState() == ModuleState::kActive && save->GetState() == ModuleState::kActive)
            {
                Pump(2);   // lets the game module register its participant lazily
                return true;
            }
        }
        return false;
    }

    void Pump(int frames) { for (int i = 0; i < frames; ++i) app->Update(kDt); }

    template <class Pred> bool PumpUntil(Pred done, int maxFrames = 20)
    {
        for (int i = 0; i < maxFrames && !done(); ++i)
            app->Update(kDt);
        return done();
    }

    void SetSpeed(SpeedTier tier)
    {
        SpeedChangeRequested m; m.tier = tier;
        ASSERT_TRUE(bus->GetBus().Broadcast(m));
    }

    std::string SlotPath(const char* slot) const { return dir + "slot_" + slot + ".sav"; }
};

GameSpeedState MakeState(uint64_t day, SpeedTier tier)
{
    GameSpeedState s;
    s.dayCounter = day;
    s.speedTier = tier;
    return s;
}

} // namespace

TEST(GameSpeedSaveParticipant, SaveWritesGameSpeedDayKey)
{
    Fixture f("gs_participant_key");
    ASSERT_TRUE(f.Boot());
    f.game->RestoreState(MakeState(7, SpeedTier::Paused));
    f.save->RequestSave(StringCRC("k"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetSaveCount() == 1; }));

    std::ifstream in(f.SlotPath("k"), std::ios::binary);
    ASSERT_TRUE(in.good());
    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(in, root));
    ASSERT_TRUE(root.isMember("game_speed"));
    ASSERT_TRUE(root["game_speed"].isMember("day"));
    EXPECT_EQ(root["game_speed"]["day"].asInt64(), 7);
}

TEST(GameSpeedSaveParticipant, RoundTripRestoresDayAndResumesPaused)
{
    Fixture f("gs_participant_roundtrip");
    ASSERT_TRUE(f.Boot());
    f.game->RestoreState(MakeState(5, SpeedTier::Paused));
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(2);
    ASSERT_FALSE(f.game->GetController().IsPaused());
    ASSERT_GE(f.game->GetController().GetState().dayCounter, 5u);
    const uint64_t savedDay = f.game->GetController().GetState().dayCounter;

    f.save->RequestSave(StringCRC("q"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetSaveCount() == 1; }));
    ASSERT_TRUE(f.save->GetLastSaveResult().Ok());

    // Advance past the saved day (fastest = 1 s/day).
    ASSERT_TRUE(f.PumpUntil([&] { return f.game->GetController().GetState().dayCounter > savedDay + 1; }, 120));
    ASSERT_FALSE(f.game->GetController().IsPaused());

    f.save->RequestLoad(StringCRC("q"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetLoadCount() == 1; }));
    f.Pump(1);

    EXPECT_EQ(f.game->GetController().GetState().dayCounter, savedDay);
    EXPECT_TRUE(f.game->GetController().IsPaused());
}

TEST(GameSpeedSaveParticipant, DeserializeWithoutDayKeyLeavesStateUnchanged)
{
    Fixture f("gs_participant_missing");
    ASSERT_TRUE(f.Boot());
    f.game->RestoreState(MakeState(9, SpeedTier::Paused));

    Json::Value empty(Json::objectValue);
    Dia::SaveGame::LoadContext ctx(empty);
    f.game->GetSaveParticipant().Deserialize(ctx);

    EXPECT_EQ(f.game->GetController().GetState().dayCounter, 9u);
    EXPECT_EQ(f.game->GetSaveParticipant().GetVersion(), 2u);
}

// ---------------------------------------------------------------------------
// M7: every speed tier restores paused with the saved day and no tick burst
// ---------------------------------------------------------------------------

namespace {
SpeedTier kRunningTiers[] = { SpeedTier::Slow, SpeedTier::Normal, SpeedTier::Fast, SpeedTier::Fastest };
}

TEST(GameSpeedSaveParticipant, SaveAtEachSpeedTierLoadsPausedWithSavedDayAndNoTickBurst)
{
    for (SpeedTier tier : kRunningTiers)
    {
        SCOPED_TRACE(SpeedTierName(tier));
        Fixture f("gs_participant_tiers");
        ASSERT_TRUE(f.Boot());

        std::vector<SimulationTick> ticks;
        auto sub = f.bus->GetBus().Subscribe<SimulationTick>(StringCRC("TierTickSink"),
            [&](const SimulationTick& t) { ticks.push_back(t); });
        ASSERT_TRUE(sub.IsValid());

        f.game->RestoreState(MakeState(40, SpeedTier::Paused));
        f.SetSpeed(tier);
        f.Pump(3);
        ASSERT_EQ(f.game->GetController().GetState().speedTier, tier);
        ASSERT_FALSE(f.game->GetController().IsPaused());
        if (tier == SpeedTier::Fastest)
            ASSERT_TRUE(f.PumpUntil([&] { return f.game->GetController().GetState().dayCounter > 40; }, 120));
        const uint64_t savedDay = f.game->GetController().GetState().dayCounter;

        f.save->RequestSave(StringCRC("tier"));
        ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetSaveCount() == 1; }));
        ASSERT_TRUE(f.save->GetLastSaveResult().Ok());

        // Diverge: a different day, still running.
        f.game->RestoreState(MakeState(999, SpeedTier::Paused));
        f.SetSpeed(tier);
        f.Pump(2);
        ASSERT_FALSE(f.game->GetController().IsPaused());

        f.save->RequestLoad(StringCRC("tier"));
        ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetLoadCount() == 1; }));
        ticks.clear();
        f.Pump(45);
        EXPECT_EQ(f.game->GetController().GetState().dayCounter, savedDay);
        EXPECT_TRUE(f.game->GetController().IsPaused());
        EXPECT_EQ(f.game->GetController().GetState().speedTier, SpeedTier::Paused);
        EXPECT_TRUE(ticks.empty()) << ticks.size() << " tick(s) after load";
    }
}

// ---------------------------------------------------------------------------
// Load while a modal is open (Part 1.4): the restore clears the modal bookkeeping; a later
// ModalClosed must not underflow or resume the game.
// ---------------------------------------------------------------------------

TEST(GameSpeedSaveParticipant, LoadWhileModalOpenThenModalClosedStaysPausedWithoutUnderflow)
{
    Fixture f("gs_participant_modal");
    ASSERT_TRUE(f.Boot());

    f.game->RestoreState(MakeState(12, SpeedTier::Paused));
    f.SetSpeed(SpeedTier::Fast);
    f.Pump(2);
    f.save->RequestSave(StringCRC("m"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetSaveCount() == 1; }));

    f.SetSpeed(SpeedTier::Fast);
    ModalOpened open;
    ASSERT_TRUE(f.bus->GetBus().Broadcast(open));
    ASSERT_TRUE(f.bus->GetBus().Broadcast(open));   // nested
    f.Pump(2);
    ASSERT_EQ(f.game->GetController().GetState().modalDepth, 2u);
    ASSERT_TRUE(f.game->GetController().IsPaused());

    f.save->RequestLoad(StringCRC("m"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetLoadCount() == 1; }));
    f.Pump(1);
    EXPECT_TRUE(f.game->GetController().IsPaused());
    EXPECT_EQ(f.game->GetController().GetState().modalDepth, 0u);

    ModalClosed close;
    for (int i = 0; i < 3; ++i)   // more closes than opens
    {
        ASSERT_TRUE(f.bus->GetBus().Broadcast(close));
        f.Pump(2);
        EXPECT_TRUE(f.game->GetController().IsPaused()) << "close " << i;
        EXPECT_EQ(f.game->GetController().GetState().modalDepth, 0u) << "close " << i;
        EXPECT_EQ(f.game->GetController().GetState().speedTier, SpeedTier::Paused) << "close " << i;
    }
    EXPECT_EQ(f.game->GetController().GetState().dayCounter, 12u);
}

TEST(GameSpeedController, RestoreStateWithModalsOpenThenClosesNeverResumes)
{
    GameSpeedController c(DefaultSpeedPresetConfig());
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    c.OnModalOpened();
    c.OnModalOpened();
    ASSERT_EQ(c.GetState().modalDepth, 2u);

    c.RestoreState(MakeState(7, SpeedTier::Fast));
    EXPECT_EQ(c.GetState().modalDepth, 0u);
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().dayCounter, 7u);

    c.OnModalClosed();
    c.OnModalClosed();
    c.OnModalClosed();
    EXPECT_EQ(c.GetState().modalDepth, 0u);
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Player);
}

// ---------------------------------------------------------------------------
// Version 2: the sub-day accumulator and preModalSpeedTier persist (F-2 C3 convergence).
// EC-03 is unchanged: speed tier / pause source / modal depth still come back at the paused defaults.
// ---------------------------------------------------------------------------

TEST(GameSpeedSaveParticipant, RoundTripRestoresSubDayAccumulatorAndPreModalTier)
{
    Fixture f("gs_participant_v2");
    ASSERT_TRUE(f.Boot());
    f.game->RestoreState(MakeState(3, SpeedTier::Paused));
    f.SetSpeed(SpeedTier::Fast);
    f.Pump(8);   // partway into a day
    ModalOpened open;
    ASSERT_TRUE(f.bus->GetBus().Broadcast(open));   // preModalSpeedTier = Fast, paused
    f.Pump(3);
    ASSERT_EQ(f.game->GetController().GetState().preModalSpeedTier, SpeedTier::Fast);
    const int64_t savedAcc = f.game->GetAccumulatedUs();
    const uint64_t savedDay = f.game->GetController().GetState().dayCounter;
    ASSERT_GT(savedAcc, 0);

    f.save->RequestSave(StringCRC("v2"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetSaveCount() == 1; }));

    // Diverge: plain restore resets the accumulator and the modal bookkeeping.
    f.game->RestoreState(MakeState(99, SpeedTier::Paused));
    f.Pump(3);
    ASSERT_EQ(f.game->GetAccumulatedUs(), 0);
    ASSERT_EQ(f.game->GetController().GetState().preModalSpeedTier, SpeedTier::Normal);

    f.save->RequestLoad(StringCRC("v2"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetLoadCount() == 1; }));
    f.Pump(2);

    EXPECT_EQ(f.game->GetController().GetState().dayCounter, savedDay);
    EXPECT_EQ(f.game->GetAccumulatedUs(), savedAcc);
    EXPECT_EQ(f.game->GetController().GetState().preModalSpeedTier, SpeedTier::Fast);
    // EC-03 unchanged.
    EXPECT_EQ(f.game->GetController().GetState().speedTier, SpeedTier::Paused);
    EXPECT_EQ(f.game->GetController().GetState().pauseSource, PauseSource::Player);
    EXPECT_EQ(f.game->GetController().GetState().modalDepth, 0u);
}

TEST(GameSpeedSaveParticipant, Version1SaveWithDayOnlyStillLoads)
{
    Fixture f("gs_participant_v1compat");
    ASSERT_TRUE(f.Boot());
    Json::Value v1(Json::objectValue);
    v1["day"] = Json::Value(static_cast<Json::Int64>(21));
    Dia::SaveGame::LoadContext ctx(v1);
    f.game->GetSaveParticipant().Deserialize(ctx);
    f.Pump(2);
    EXPECT_EQ(f.game->GetController().GetState().dayCounter, 21u);
    EXPECT_EQ(f.game->GetAccumulatedUs(), 0);
    EXPECT_EQ(f.game->GetController().GetState().preModalSpeedTier, SpeedTier::Normal);
    EXPECT_TRUE(f.game->GetController().IsPaused());
}
