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
    EXPECT_EQ(f.game->GetSaveParticipant().GetVersion(), 1u);
}
