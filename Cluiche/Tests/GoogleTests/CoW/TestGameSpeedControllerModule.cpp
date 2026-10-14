// Integration tests for CoW::GameSpeedControllerModule running inside a real Application:
// MessageBusHostModule + GameSpeedControllerModule on a 30 Hz SimPU, driven by app.Update().
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/SimTime/SimTimeDomain.h>
#include <Modules/GameSpeedControllerModule.h>
#include <Modules/MessageBusHostModule.h>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace CoW;

namespace {

// Factories are plain function pointers, so file-scope pointers capture the created instances.
Cluiche::AppFlow::MessageBusHostModule* g_bus  = nullptr;
GameSpeedControllerModule*              g_game = nullptr;

Module* CreateBus(const StringCRC& id)  { g_bus  = new Cluiche::AppFlow::MessageBusHostModule(id); return g_bus; }
Module* CreateGame(const StringCRC& id) { g_game = new GameSpeedControllerModule(id); return g_game; }

constexpr float kDt = 1.0f / 30.0f;

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    Cluiche::AppFlow::MessageBusHostModule* bus = nullptr;
    GameSpeedControllerModule* game = nullptr;
    Dia::MessageBus::BusSubscriptionHandle tickSub;
    std::vector<SimulationTick> ticks;
    int ticksThisFrame = 0;
    int maxTicksInOneFrame = 0;

    explicit Fixture(const char* configJson = nullptr)
    {
        reg.Register(StringCRC("MessageBusHostModule"), &CreateBus);
        reg.Register(StringCRC("GameSpeedControllerModule"), &CreateGame);

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

        ModuleDeclaration gameDecl;
        gameDecl.instanceId = StringCRC("GameSpeedControllerModule");
        gameDecl.typeId = StringCRC("GameSpeedControllerModule");
        gameDecl.stages.Add(StringCRC("all"));
        gameDecl.dependencies.Add(StringCRC("MessageBusHostModule"));
        if (configJson)
            gameDecl.configJson = configJson;
        pu.modules.Add(gameDecl);

        manifest.processingUnits.Add(pu);
    }

    // Returns false if the app failed to come up.
    bool Boot()
    {
        g_bus = nullptr;
        g_game = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start())
            return false;
        bus  = g_bus;
        game = g_game;
        if (!bus || !game)
            return false;
        for (int i = 0; i < 20; ++i)
        {
            app->Update(kDt);
            if (game->GetState() == ModuleState::kActive)
            {
                tickSub = bus->GetBus().Subscribe<SimulationTick>(StringCRC("TestTickSink"),
                    [this](const SimulationTick& t) { ticks.push_back(t); ++ticksThisFrame; });
                return tickSub.IsValid();
            }
        }
        return false;
    }

    Dia::SimTime::SimTimeDomain& World() { return game->GetProcessingUnit()->GetWorldDomain(); }

    template <class T> void Send(const T& msg) { ASSERT_TRUE(bus->GetBus().Broadcast(msg)); }
    void SetSpeed(SpeedTier tier) { SpeedChangeRequested m; m.tier = tier; Send(m); }

    void Pump(int frames)
    {
        for (int i = 0; i < frames; ++i)
        {
            ticksThisFrame = 0;
            app->Update(kDt);
            if (ticksThisFrame > maxTicksInOneFrame)
                maxTicksInOneFrame = ticksThisFrame;
        }
    }
};

} // namespace

TEST(GameSpeedModule, StartsPausedAndPublishesNoTicks)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    EXPECT_TRUE(f.game->GetController().IsPaused());
    EXPECT_EQ(f.game->GetController().GetState().dayCounter, 0u);
    f.Pump(300);
    EXPECT_TRUE(f.ticks.empty());
    EXPECT_FLOAT_EQ(f.World().GetScale(), 0.0f);
}

TEST(GameSpeedModule, FastestPublishesSequentialDaysAtConfiguredRate)
{
    Fixture f;   // default presets: fastest = 1 s/day
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(305);   // ~10.1 s of sim time
    EXPECT_GT(f.World().GetScale(), 0.0f);
    EXPECT_FLOAT_EQ(f.World().GetScale(), 1.0f);
    ASSERT_GE(f.ticks.size(), 9u);
    ASSERT_LE(f.ticks.size(), 11u);
    for (size_t i = 0; i < f.ticks.size(); ++i)
    {
        EXPECT_EQ(f.ticks[i].day, i + 1);
        EXPECT_EQ(f.ticks[i].band, ResolveBand(i + 1));
    }
    EXPECT_EQ(f.game->GetController().GetState().dayCounter, f.ticks.size());
}

TEST(GameSpeedModule, EachPresetRunsAtItsConfiguredRate)
{
    const char* config = "{\"wall_seconds_per_day\":{\"slow\":8.0,\"normal\":4.0,\"fast\":2.0,\"fastest\":1.0}}";
    const struct { SpeedTier tier; double secPerDay; } cases[] = {
        { SpeedTier::Slow, 8.0 }, { SpeedTier::Normal, 4.0 }, { SpeedTier::Fast, 2.0 }, { SpeedTier::Fastest, 1.0 } };
    for (const auto& c : cases)
    {
        Fixture f(config);
        ASSERT_TRUE(f.Boot());
        f.SetSpeed(c.tier);
        f.Pump(30 * 16);   // 16 wall seconds
        const double expected = 16.0 / c.secPerDay;
        EXPECT_NEAR(static_cast<double>(f.ticks.size()), expected, 1.5) << SpeedTierName(c.tier);
    }
}

TEST(GameSpeedModule, NeverMoreThanOneTickPerFrame)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(600);
    EXPECT_GT(f.ticks.size(), 0u);
    EXPECT_LE(f.maxTicksInOneFrame, 1);
}

TEST(GameSpeedModule, PauseStopsTicksAndResumeContinuesWithoutSkippingDays)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(100);
    f.SetSpeed(SpeedTier::Paused);
    f.Pump(3);   // let the pause propagate
    const size_t atPause = f.ticks.size();
    f.Pump(300);
    EXPECT_EQ(f.ticks.size(), atPause);
    EXPECT_FLOAT_EQ(f.World().GetScale(), 0.0f);
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(100);
    EXPECT_GT(f.ticks.size(), atPause);
    for (size_t i = 0; i < f.ticks.size(); ++i)
        EXPECT_EQ(f.ticks[i].day, i + 1);
}

TEST(GameSpeedModule, ManualStepProducesExactlyOneTickPerRequest)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Send(ManualAdvanceRequested{});
    f.Pump(10);
    ASSERT_EQ(f.ticks.size(), 1u);
    EXPECT_EQ(f.ticks[0].day, 1u);
    EXPECT_FLOAT_EQ(f.World().GetScale(), 0.0f);
    f.Pump(60);
    EXPECT_EQ(f.ticks.size(), 1u);   // still paused: no further ticks
    f.Send(ManualAdvanceRequested{});
    f.Pump(10);
    ASSERT_EQ(f.ticks.size(), 2u);
    EXPECT_EQ(f.ticks[1].day, 2u);
}

TEST(GameSpeedModule, ManualStepAfterPartialDayAdvancesToNextBoundary)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(15);   // about half a day
    f.SetSpeed(SpeedTier::Paused);
    f.Pump(3);
    const size_t before = f.ticks.size();
    f.Send(ManualAdvanceRequested{});
    f.Pump(10);
    EXPECT_EQ(f.ticks.size(), before + 1);
}

TEST(GameSpeedModule, ManualStepRejectedWhileRunning)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Slow);   // 60 s/day: nothing should tick in 2 s
    f.Pump(5);
    f.Send(ManualAdvanceRequested{});
    f.Pump(60);
    EXPECT_TRUE(f.ticks.empty());
    EXPECT_GT(f.World().GetScale(), 0.0f);
}

TEST(GameSpeedModule, ModalPausesAndRestoresOnClose)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(60);
    f.Send(ModalOpened{});
    f.Pump(3);
    const size_t atOpen = f.ticks.size();
    f.Pump(200);
    EXPECT_EQ(f.ticks.size(), atOpen);
    EXPECT_FLOAT_EQ(f.World().GetScale(), 0.0f);
    f.Send(ModalClosed{});
    f.Pump(100);
    EXPECT_GT(f.ticks.size(), atOpen);
    EXPECT_EQ(f.game->GetController().GetState().speedTier, SpeedTier::Fastest);
}

TEST(GameSpeedModule, PlayerPauseSurvivesModalClose)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(5);
    f.SetSpeed(SpeedTier::Paused);
    f.Pump(3);
    f.Send(ModalOpened{});
    f.Pump(3);
    f.Send(ModalClosed{});
    f.Pump(3);
    const size_t n = f.ticks.size();
    f.Pump(200);
    EXPECT_EQ(f.ticks.size(), n);
    EXPECT_FLOAT_EQ(f.World().GetScale(), 0.0f);
}

TEST(GameSpeedModule, WorldGenerationCompleteResetsToDayZeroPaused)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(100);
    ASSERT_GT(f.ticks.size(), 0u);
    f.Send(WorldGenerationComplete{});
    f.Pump(5);
    EXPECT_EQ(f.game->GetController().GetState().dayCounter, 0u);
    EXPECT_TRUE(f.game->GetController().IsPaused());
    const size_t n = f.ticks.size();
    f.Pump(100);
    EXPECT_EQ(f.ticks.size(), n);
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(100);
    ASSERT_GT(f.ticks.size(), n);
    EXPECT_EQ(f.ticks[n].day, 1u);   // counting restarts from day 1
}

TEST(GameSpeedModule, RestoreStateResumesFromSavedDayPaused)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(50);
    GameSpeedState saved;
    saved.dayCounter = 500;
    saved.speedTier = SpeedTier::Fastest;
    f.game->RestoreState(saved);
    f.Pump(100);
    const size_t n = f.ticks.size();
    EXPECT_TRUE(f.game->GetController().IsPaused());
    EXPECT_EQ(f.game->GetController().GetState().dayCounter, 500u);
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(100);
    ASSERT_GT(f.ticks.size(), n);
    EXPECT_EQ(f.ticks[n].day, 501u);
}

TEST(GameSpeedModule, CalendarBandsPublishedAtBoundaries)
{
    // 0.05 s/day at 30 Hz = 0.67 day/frame: ticks nearly every frame, reaching day 91 in ~140 frames.
    Fixture f("{\"wall_seconds_per_day\":{\"slow\":60,\"normal\":30,\"fast\":10,\"fastest\":0.05}}");
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(200);
    ASSERT_GE(f.ticks.size(), 91u);
    EXPECT_EQ(f.ticks[6].band, TimeBand::Week);     // day 7
    EXPECT_EQ(f.ticks[90].band, TimeBand::Season);  // day 91
    EXPECT_EQ(f.ticks[0].band, TimeBand::Day);
    EXPECT_LE(f.maxTicksInOneFrame, 1);
}

TEST(GameSpeedModule, ValidConfigIsApplied)
{
    Fixture f("{\"wall_seconds_per_day\":{\"slow\":8,\"normal\":4,\"fast\":2,\"fastest\":0.5}}");
    ASSERT_TRUE(f.Boot());
    EXPECT_FLOAT_EQ(f.game->GetController().GetConfig().wallSecondsPerDay[3], 0.5f);
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(3);
    EXPECT_FLOAT_EQ(f.World().GetScale(), 2.0f);
}

TEST(GameSpeedModule, TooFastConfigFallsBackToDefaults)
{
    Fixture f("{\"wall_seconds_per_day\":{\"slow\":60,\"normal\":30,\"fast\":10,\"fastest\":0.001}}");
    ASSERT_TRUE(f.Boot());
    const SpeedPresetConfig defaults = DefaultSpeedPresetConfig();
    for (int i = 0; i < 4; ++i)
        EXPECT_FLOAT_EQ(f.game->GetController().GetConfig().wallSecondsPerDay[i], defaults.wallSecondsPerDay[i]);
}

TEST(GameSpeedModule, NonDecreasingConfigFallsBackToDefaults)
{
    Fixture f("{\"wall_seconds_per_day\":{\"slow\":1,\"normal\":2,\"fast\":3,\"fastest\":4}}");
    ASSERT_TRUE(f.Boot());
    EXPECT_FLOAT_EQ(f.game->GetController().GetConfig().wallSecondsPerDay[0], DefaultSpeedPresetConfig().wallSecondsPerDay[0]);
}

TEST(GameSpeedModule, MalformedConfigFallsBackToDefaults)
{
    {
        Fixture f("{not json");
        ASSERT_TRUE(f.Boot());
        EXPECT_FLOAT_EQ(f.game->GetController().GetConfig().wallSecondsPerDay[1], DefaultSpeedPresetConfig().wallSecondsPerDay[1]);
    }
    {
        Fixture f("{\"wall_seconds_per_day\":{\"slow\":\"fast\"}}");
        ASSERT_TRUE(f.Boot());
        EXPECT_FLOAT_EQ(f.game->GetController().GetConfig().wallSecondsPerDay[0], DefaultSpeedPresetConfig().wallSecondsPerDay[0]);
    }
    {
        Fixture f("{\"wall_seconds_per_day\":5}");
        ASSERT_TRUE(f.Boot());
        EXPECT_FLOAT_EQ(f.game->GetController().GetConfig().wallSecondsPerDay[0], DefaultSpeedPresetConfig().wallSecondsPerDay[0]);
    }
}

TEST(GameSpeedModule, MissingConfigUsesDefaults)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    EXPECT_FLOAT_EQ(f.game->GetController().GetConfig().wallSecondsPerDay[2], DefaultSpeedPresetConfig().wallSecondsPerDay[2]);
}

TEST(GameSpeedModule, PartialConfigKeepsDefaultsForMissingKeys)
{
    Fixture f("{\"wall_seconds_per_day\":{\"normal\":20}}");
    ASSERT_TRUE(f.Boot());
    const SpeedPresetConfig d = DefaultSpeedPresetConfig();
    EXPECT_FLOAT_EQ(f.game->GetController().GetConfig().wallSecondsPerDay[0], d.wallSecondsPerDay[0]);
    EXPECT_FLOAT_EQ(f.game->GetController().GetConfig().wallSecondsPerDay[1], 20.0f);
}

TEST(GameSpeedModule, SameDaysRegardlessOfSpeed)
{
    // Same number of days at Fast (2 s/day) and Fastest (0.5 s/day): sequences are identical.
    const char* config = "{\"wall_seconds_per_day\":{\"slow\":8,\"normal\":4,\"fast\":2,\"fastest\":0.5}}";
    std::vector<uint64_t> slowDays, fastDays;
    {
        Fixture f(config);
        ASSERT_TRUE(f.Boot());
        f.SetSpeed(SpeedTier::Fast);
        while (f.ticks.size() < 20) f.Pump(1);
        for (const auto& t : f.ticks) slowDays.push_back(t.day * 100 + static_cast<uint64_t>(t.band));
    }
    {
        Fixture f(config);
        ASSERT_TRUE(f.Boot());
        f.SetSpeed(SpeedTier::Fastest);
        while (f.ticks.size() < 20) f.Pump(1);
        for (const auto& t : f.ticks) fastDays.push_back(t.day * 100 + static_cast<uint64_t>(t.band));
    }
    EXPECT_EQ(slowDays, fastDays);
}

TEST(GameSpeedModule, SpeedChangeMidRunLosesNoDay)
{
    Fixture f("{\"wall_seconds_per_day\":{\"slow\":8,\"normal\":4,\"fast\":2,\"fastest\":0.5}}");
    ASSERT_TRUE(f.Boot());
    const SpeedTier cycle[] = { SpeedTier::Slow, SpeedTier::Fastest, SpeedTier::Normal, SpeedTier::Paused, SpeedTier::Fast };
    for (int round = 0; round < 6; ++round)
    {
        for (SpeedTier t : cycle)
        {
            f.SetSpeed(t);
            f.Pump(17);
        }
    }
    ASSERT_GT(f.ticks.size(), 5u);
    for (size_t i = 0; i < f.ticks.size(); ++i)
        EXPECT_EQ(f.ticks[i].day, i + 1);
    EXPECT_LE(f.maxTicksInOneFrame, 1);
}
