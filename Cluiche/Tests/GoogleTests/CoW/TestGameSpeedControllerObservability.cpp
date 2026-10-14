// Observability, failure-path, lifecycle and soak tests for CoW::GameSpeedControllerModule.
// Runs a real Application (MessageBusHostModule + GameSpeedControllerModule on a 30 Hz SimPU).
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/SimTime/SimTimeDomain.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <Modules/GameSpeedControllerModule.h>
#include <Modules/MessageBusHostModule.h>
#include <algorithm>
#include <memory>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Observation;
using namespace CoW;

namespace {

constexpr float kDt = 1.0f / 30.0f;

// Fills the bus handler pool so the game module's later Subscribe calls fail.
// The mailbox caps subscribers per type, so spread the hog over several types.
struct HogEvent0 { static inline const StringCRC kTypeId{ "CoW.Test.HogEvent0" }; };
struct HogEvent1 { static inline const StringCRC kTypeId{ "CoW.Test.HogEvent1" }; };
struct HogEvent2 { static inline const StringCRC kTypeId{ "CoW.Test.HogEvent2" }; };
struct HogEvent3 { static inline const StringCRC kTypeId{ "CoW.Test.HogEvent3" }; };
struct HogEvent4 { static inline const StringCRC kTypeId{ "CoW.Test.HogEvent4" }; };
struct HogEvent5 { static inline const StringCRC kTypeId{ "CoW.Test.HogEvent5" }; };

class HogModule : public SimModule
{
public:
    explicit HogModule(const StringCRC& id) : SimModule(id) {}
    int leaveFree = 2;

protected:
    StartResult DoStart() override
    {
        auto* host = mBus.Get();
        if (!host) return StartResult::kLoading;
        auto& bus = host->GetBus();
        FillWith<HogEvent0>(bus);
        FillWith<HogEvent1>(bus);
        FillWith<HogEvent2>(bus);
        FillWith<HogEvent3>(bus);
        FillWith<HogEvent4>(bus);
        FillWith<HogEvent5>(bus);
        return StartResult::kReady;
    }
    void DoUpdate(const Dia::SimTime::SimTimeContext&) override {}
    StopResult DoStop() override { mHandles.clear(); return StopResult::kDone; }

private:
    template <class T>
    void FillWith(Dia::MessageBus::Bus& bus)
    {
        bus.RegisterType<T, 4>();
        while (bus.GetHandlerCount() + leaveFree < bus.GetHandlerCapacity())
        {
            char name[32];
            snprintf(name, sizeof(name), "Hog%u", static_cast<unsigned>(mHandles.size()));
            auto h = bus.Subscribe<T>(StringCRC(name), [](const T&) {});
            if (!h.IsValid()) break;
            mHandles.push_back(std::move(h));
        }
    }

    ModuleRef<Cluiche::AppFlow::MessageBusHostModule> mBus{ this };
    std::vector<Dia::MessageBus::BusSubscriptionHandle> mHandles;
};

Cluiche::AppFlow::MessageBusHostModule* g_bus  = nullptr;
GameSpeedControllerModule*              g_game = nullptr;

Module* CreateBus(const StringCRC& id)  { g_bus  = new Cluiche::AppFlow::MessageBusHostModule(id); return g_bus; }
Module* CreateGame(const StringCRC& id) { g_game = new GameSpeedControllerModule(id); return g_game; }
Module* CreateHog(const StringCRC& id)  { return new HogModule(id); }

uint64_t CounterValue(const char* name)
{
    auto* c = Metric::MetricRegistry::Instance().FindCounter(StringCRC(name));
    return c ? c->Value() : 0;
}
double GaugeValue(const char* name)
{
    auto* g = Metric::MetricRegistry::Instance().FindGauge(StringCRC(name));
    return g ? g->Value() : -1.0;
}

struct Options
{
    const char* config = nullptr;
    bool hog = false;
    bool busOnlyInFirstStage = false;     // bus active in "Run" only
    bool gameOnlyInRunStage = false;      // game active in "Run" only
};

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    Cluiche::AppFlow::MessageBusHostModule* bus = nullptr;
    GameSpeedControllerModule* game = nullptr;
    Dia::MessageBus::BusSubscriptionHandle tickSub;
    std::vector<SimulationTick> ticks;
    std::vector<int> tickFrames;      // frame index of each tick
    int frame = 0;
    int ticksThisFrame = 0;
    int maxTicksInOneFrame = 0;

    explicit Fixture(const Options& o = Options())
    {
        reg.Register(StringCRC("MessageBusHostModule"), &CreateBus);
        reg.Register(StringCRC("GameSpeedControllerModule"), &CreateGame);
        reg.Register(StringCRC("HogModule"), &CreateHog);

        manifest.version = 3;
        StageDeclaration run;  run.name  = StringCRC("Run");
        StageDeclaration idle; idle.name = StringCRC("Idle");
        manifest.stages.Add(run);
        manifest.stages.Add(idle);
        manifest.initialStage = StringCRC("Run");

        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("SimPU");
        pu.frequencyHz = 30.0f;
        pu.dedicatedThread = false;

        ModuleDeclaration busDecl;
        busDecl.instanceId = StringCRC("MessageBusHostModule");
        busDecl.typeId = StringCRC("MessageBusHostModule");
        busDecl.stages.Add(StringCRC(o.busOnlyInFirstStage ? "Run" : "all"));
        pu.modules.Add(busDecl);

        if (o.hog)
        {
            ModuleDeclaration hog;
            hog.instanceId = StringCRC("HogModule");
            hog.typeId = StringCRC("HogModule");
            hog.stages.Add(StringCRC("all"));
            hog.dependencies.Add(StringCRC("MessageBusHostModule"));
            pu.modules.Add(hog);
        }

        ModuleDeclaration gameDecl;
        gameDecl.instanceId = StringCRC("GameSpeedControllerModule");
        gameDecl.typeId = StringCRC("GameSpeedControllerModule");
        gameDecl.stages.Add(StringCRC(o.gameOnlyInRunStage ? "Run" : "all"));
        if (!o.busOnlyInFirstStage)
            gameDecl.dependencies.Add(StringCRC("MessageBusHostModule"));
        if (o.hog)
            gameDecl.dependencies.Add(StringCRC("HogModule"));
        if (o.config)
            gameDecl.configJson = o.config;
        pu.modules.Add(gameDecl);

        manifest.processingUnits.Add(pu);
    }

    bool Boot(bool subscribeTicks = true)
    {
        g_bus = nullptr;
        g_game = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start())
            return false;
        bus = g_bus;
        game = g_game;
        if (!bus || !game)
            return false;
        for (int i = 0; i < 20; ++i)
        {
            app->Update(kDt);
            if (game->GetState() == ModuleState::kActive)
            {
                if (subscribeTicks)
                    SubscribeTicks();
                return true;
            }
        }
        return false;
    }

    void SubscribeTicks()
    {
        tickSub = bus->GetBus().Subscribe<SimulationTick>(StringCRC("TestTickSink"),
            [this](const SimulationTick& t) { ticks.push_back(t); tickFrames.push_back(frame); ++ticksThisFrame; });
    }

    Dia::SimTime::SimTimeDomain& World() { return game->GetProcessingUnit()->GetWorldDomain(); }

    template <class T> void Send(const T& msg) { bus->GetBus().Broadcast(msg); }
    void SetSpeed(SpeedTier tier) { SpeedChangeRequested m; m.tier = tier; Send(m); }

    void Pump(int frames)
    {
        for (int i = 0; i < frames; ++i)
        {
            ++frame;
            ticksThisFrame = 0;
            app->Update(kDt);
            if (ticksThisFrame > maxTicksInOneFrame)
                maxTicksInOneFrame = ticksThisFrame;
        }
    }

    Health::HealthStatus Status() const { return game->GetHealthReporter().Report().status; }
    StringCRC Reason() const { return game->GetHealthReporter().Report().reason; }
};

// Deterministic LCG so the soak is reproducible.
struct Lcg
{
    uint64_t s;
    explicit Lcg(uint64_t seed) : s(seed) {}
    uint32_t Next() { s = s * 6364136223846793005ULL + 1442695040888963407ULL; return static_cast<uint32_t>(s >> 33); }
    uint32_t Below(uint32_t n) { return Next() % n; }
};

const char* kFastConfig = "{\"wall_seconds_per_day\":{\"slow\":1.0,\"normal\":0.5,\"fast\":0.2,\"fastest\":0.05}}";

} // namespace

// ---------------------------------------------------------------------------
// Metrics
// ---------------------------------------------------------------------------
TEST(GameSpeedObservability, TickCounterAndDayGaugeTrackPublishedDays)
{
    Options o; o.config = kFastConfig;
    Fixture f(o);
    const uint64_t before = CounterValue("cow.gamespeed.ticks");
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(120);
    ASSERT_GT(f.ticks.size(), 10u);
    EXPECT_EQ(CounterValue("cow.gamespeed.ticks") - before, f.ticks.size());
    EXPECT_EQ(GaugeValue("cow.gamespeed.day"), static_cast<double>(f.game->GetController().GetState().dayCounter));
    EXPECT_EQ(GaugeValue("cow.gamespeed.tier"), static_cast<double>(SpeedTier::Fastest));
    EXPECT_EQ(GaugeValue("cow.gamespeed.paused"), 0.0);
}

TEST(GameSpeedObservability, PausedModalAndDepthGauges)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    EXPECT_EQ(GaugeValue("cow.gamespeed.paused"), 1.0);
    f.SetSpeed(SpeedTier::Normal);
    f.Pump(3);
    EXPECT_EQ(GaugeValue("cow.gamespeed.paused"), 0.0);
    f.Send(ModalOpened{});
    f.Send(ModalOpened{});
    f.Pump(3);
    EXPECT_EQ(GaugeValue("cow.gamespeed.modal_depth"), 2.0);
    EXPECT_EQ(GaugeValue("cow.gamespeed.paused"), 1.0);
    f.Send(ModalClosed{});
    f.Send(ModalClosed{});
    f.Pump(3);
    EXPECT_EQ(GaugeValue("cow.gamespeed.modal_depth"), 0.0);
    EXPECT_EQ(GaugeValue("cow.gamespeed.paused"), 0.0);
}

TEST(GameSpeedObservability, SpeedChangeCounterCountsTierChangesOnly)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    const uint64_t before = CounterValue("cow.gamespeed.speed_changes");
    f.SetSpeed(SpeedTier::Fast);       // +1
    f.Pump(2);
    f.SetSpeed(SpeedTier::Fast);       // no-op
    f.Pump(2);
    f.SetSpeed(SpeedTier::Slow);       // +1
    f.Pump(2);
    f.SetSpeed(SpeedTier::Paused);     // +1
    f.Pump(2);
    EXPECT_EQ(CounterValue("cow.gamespeed.speed_changes") - before, 3u);
}

TEST(GameSpeedObservability, StepCountersSplitAcceptedAndRejected)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    const uint64_t steps0 = CounterValue("cow.gamespeed.steps");
    const uint64_t rej0   = CounterValue("cow.gamespeed.steps_rejected");
    f.Send(ManualAdvanceRequested{});     // paused: accepted
    f.Pump(5);
    f.SetSpeed(SpeedTier::Slow);
    f.Pump(3);
    f.Send(ManualAdvanceRequested{});     // running: rejected
    f.Send(ManualAdvanceRequested{});
    f.Pump(3);
    EXPECT_EQ(CounterValue("cow.gamespeed.steps") - steps0, 1u);
    EXPECT_EQ(CounterValue("cow.gamespeed.steps_rejected") - rej0, 2u);
}

TEST(GameSpeedObservability, ConfigFallbackCounterAndDegradedHealth)
{
    Options o; o.config = "{\"wall_seconds_per_day\":{\"slow\":1,\"normal\":2,\"fast\":3,\"fastest\":4}}";
    Fixture f(o);
    const uint64_t before = CounterValue("cow.gamespeed.config_fallbacks");
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_EQ(CounterValue("cow.gamespeed.config_fallbacks") - before, 1u);
    EXPECT_EQ(f.Status(), Health::HealthStatus::kDegraded);
    EXPECT_EQ(f.Reason(), StringCRC("fallback_preset_config"));
}

TEST(GameSpeedObservability, ValidConfigDoesNotCountAFallback)
{
    Fixture f;
    const uint64_t before = CounterValue("cow.gamespeed.config_fallbacks");
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_EQ(CounterValue("cow.gamespeed.config_fallbacks"), before);
    EXPECT_EQ(f.Status(), Health::HealthStatus::kOK);
}

// ---------------------------------------------------------------------------
// Tick lag (one tick per frame cap) and external clock moves
// ---------------------------------------------------------------------------
TEST(GameSpeedLag, ClockJumpDrainsOneTickPerFrameThenHealthRecovers)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Normal);
    f.Pump(3);
    ASSERT_TRUE(f.ticks.empty());

    // Something else pushes the world clock forward 3.5 days at once.
    f.World().AdvanceTo(f.World().Now() + TimeRelative::CreateFromSeconds(3.5f));
    f.Pump(1);
    EXPECT_EQ(f.game->GetController().GetState().dayCounter, 1u);   // published; delivered next bus pass
    EXPECT_EQ(GaugeValue("cow.gamespeed.carry_days"), 2.0);
    EXPECT_EQ(f.Status(), Health::HealthStatus::kDegraded);
    EXPECT_EQ(f.Reason(), StringCRC("day_ticks_lagging"));

    f.Pump(10);
    ASSERT_EQ(f.ticks.size(), 3u);
    for (size_t i = 0; i < f.ticks.size(); ++i)
        EXPECT_EQ(f.ticks[i].day, i + 1);
    EXPECT_LE(f.maxTicksInOneFrame, 1);
    EXPECT_EQ(GaugeValue("cow.gamespeed.carry_days"), 0.0);
    EXPECT_EQ(f.Status(), Health::HealthStatus::kOK);
}

TEST(GameSpeedLag, RestoreInSameFrameAsExternalClockMoveDoesNotBurst)
{
    // E1 loads: DiaSimTime restores the world clock and RestoreState is called before the
    // module's next update. The module must re-baseline instead of replaying the jump as days.
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.World().AdvanceTo(f.World().Now() + TimeRelative::CreateFromSeconds(1000));
    GameSpeedState saved;
    saved.dayCounter = 1000;
    f.game->RestoreState(saved);
    f.Pump(10);
    EXPECT_TRUE(f.ticks.empty());
    EXPECT_EQ(f.game->GetController().GetState().dayCounter, 1000u);
    f.SetSpeed(SpeedTier::Normal);
    f.Pump(5);
    EXPECT_TRUE(f.ticks.empty());   // 30 s/day: nothing yet
}

TEST(GameSpeedLag, PauseLagIsBoundedToOneExtraTick)
{
    Options o; o.config = kFastConfig;
    Fixture f(o);
    ASSERT_TRUE(f.Boot());
    for (int round = 0; round < 40; ++round)
    {
        f.SetSpeed(SpeedTier::Fastest);
        f.Pump(round + 3);
        const size_t beforePause = f.ticks.size();
        f.SetSpeed(SpeedTier::Paused);
        f.Pump(3);   // request delivery + queued scale applying
        const size_t settled = f.ticks.size();
        EXPECT_LE(settled - beforePause, 2u) << "round " << round;
        f.Pump(30);
        EXPECT_EQ(f.ticks.size(), settled) << "round " << round;
    }
    EXPECT_LE(f.maxTicksInOneFrame, 1);
}

// ---------------------------------------------------------------------------
// Failure paths
// ---------------------------------------------------------------------------
TEST(GameSpeedFailure, FailedSubscriptionsReportFailingHealth)
{
    Options o; o.hog = true;
    Fixture f(o);
    ASSERT_TRUE(f.Boot(false));
    f.Pump(3);
    EXPECT_EQ(f.Status(), Health::HealthStatus::kFailing);
    EXPECT_EQ(f.Reason(), StringCRC("subscription_failed"));
    EXPECT_GE(f.game->GetHealthReporter().Report().errors, 1u);
}

TEST(GameSpeedFailure, PublishWithoutBusCountsDropsAndReportsFailing)
{
    Options o; o.config = kFastConfig; o.busOnlyInFirstStage = true;
    Fixture f(o);
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(30);
    ASSERT_GT(f.ticks.size(), 0u);
    const uint64_t dropped0 = CounterValue("cow.gamespeed.tick_publish_dropped");
    const size_t delivered = f.ticks.size();

    f.app->TransitionTo(StringCRC("Idle"));   // stops the bus, game keeps running
    f.Pump(60);
    ASSERT_EQ(f.game->GetState(), ModuleState::kActive);
    EXPECT_GT(CounterValue("cow.gamespeed.tick_publish_dropped"), dropped0);
    EXPECT_EQ(f.Status(), Health::HealthStatus::kFailing);
    EXPECT_EQ(f.Reason(), StringCRC("tick_publish_failed"));
    EXPECT_EQ(f.ticks.size(), delivered);   // nothing reached subscribers
}

// ---------------------------------------------------------------------------
// Message storms
// ---------------------------------------------------------------------------
TEST(GameSpeedStorm, BalancedModalBurstWithinCapacityReturnsToRunning)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fast);
    f.Pump(3);
    for (int i = 0; i < 12; ++i) f.Send(ModalOpened{});
    for (int i = 0; i < 12; ++i) f.Send(ModalClosed{});
    f.Pump(3);
    EXPECT_EQ(f.game->GetController().GetState().modalDepth, 0);
    EXPECT_EQ(f.game->GetController().GetState().speedTier, SpeedTier::Fast);
}

TEST(GameSpeedStorm, OverflowingModalBurstKeepsStateValid)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fast);
    f.Pump(3);
    for (int i = 0; i < 200; ++i) f.Send(ModalOpened{});   // 16-slot queue drops the oldest
    f.Pump(3);
    const GameSpeedState& s = f.game->GetController().GetState();
    EXPECT_GT(s.modalDepth, 0);
    EXPECT_LE(s.modalDepth, 16);
    EXPECT_TRUE(f.game->GetController().IsPaused());
    EXPECT_FLOAT_EQ(f.World().GetScale(), 0.0f);
}

TEST(GameSpeedStorm, ManySpeedRequestsInOneFrameLastOneWins)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    const SpeedTier seq[] = { SpeedTier::Slow, SpeedTier::Fast, SpeedTier::Paused, SpeedTier::Normal,
                              SpeedTier::Fastest, SpeedTier::Slow };
    for (SpeedTier t : seq) f.SetSpeed(t);
    f.Pump(3);
    EXPECT_EQ(f.game->GetController().GetState().speedTier, SpeedTier::Slow);
    EXPECT_FLOAT_EQ(f.World().GetScale(), 1.0f / 60.0f);
}

TEST(GameSpeedStorm, SameFrameOpenAndCloseApplyOpenedFirst)
{
    // Different message types drain in registration order (Opened before Closed), not send order.
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Normal);
    f.Pump(3);
    f.Send(ModalClosed{});
    f.Send(ModalOpened{});
    f.Pump(3);
    EXPECT_EQ(f.game->GetController().GetState().modalDepth, 0);
    EXPECT_EQ(f.game->GetController().GetState().speedTier, SpeedTier::Normal);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
TEST(GameSpeedLifecycle, StopAndRestartComesBackPausedAtDayZeroWithoutDuplicateTicks)
{
    Options o; o.config = kFastConfig; o.gameOnlyInRunStage = true;
    Fixture f(o);
    ASSERT_TRUE(f.Boot());
    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(60);
    ASSERT_GT(f.ticks.size(), 3u);

    f.app->TransitionTo(StringCRC("Idle"));
    f.Pump(10);
    ASSERT_EQ(f.game->GetState(), ModuleState::kInactive);
    const size_t during = f.ticks.size();
    f.Pump(30);
    EXPECT_EQ(f.ticks.size(), during);   // stopped modules publish nothing

    f.app->TransitionTo(StringCRC("Run"));
    f.Pump(10);
    ASSERT_EQ(f.game->GetState(), ModuleState::kActive);
    EXPECT_TRUE(f.game->GetController().IsPaused());
    EXPECT_EQ(f.game->GetController().GetState().dayCounter, 0u);

    f.SetSpeed(SpeedTier::Fastest);
    f.Pump(60);
    ASSERT_GT(f.ticks.size(), during + 3);
    // Restarted run counts from day 1 again, each day exactly once (no leaked subscription).
    for (size_t i = during; i < f.ticks.size(); ++i)
        EXPECT_EQ(f.ticks[i].day, i - during + 1);
    EXPECT_LE(f.maxTicksInOneFrame, 1);
}

// ---------------------------------------------------------------------------
// Soak: random speed / pause / modal / step traffic
// ---------------------------------------------------------------------------
namespace {

struct SoakResult { std::vector<uint64_t> tickFrames; uint64_t lastDay = 0; int maxPerFrame = 0; bool sequential = true; bool bandsOk = true; };

SoakResult RunSoak(uint64_t seed, int frames)
{
    Options o; o.config = kFastConfig;
    Fixture f(o);
    SoakResult r;
    if (!f.Boot()) { r.sequential = false; return r; }
    Lcg rng(seed);
    for (int i = 0; i < frames; ++i)
    {
        const uint32_t roll = rng.Below(100);
        if (roll < 8)       f.SetSpeed(static_cast<SpeedTier>(rng.Below(5)));
        else if (roll < 11) f.Send(ModalOpened{});
        else if (roll < 14) f.Send(ModalClosed{});
        else if (roll < 17) f.Send(ManualAdvanceRequested{});
        f.Pump(1);
    }
    for (size_t i = 0; i < f.ticks.size(); ++i)
    {
        if (f.ticks[i].day != i + 1) r.sequential = false;
        if (f.ticks[i].band != ResolveBand(f.ticks[i].day)) r.bandsOk = false;
        r.tickFrames.push_back(static_cast<uint64_t>(f.tickFrames[i]));
    }
    r.lastDay = f.ticks.empty() ? 0 : f.ticks.back().day;
    r.maxPerFrame = f.maxTicksInOneFrame;
    return r;
}

} // namespace

TEST(SLOW_GameSpeedSoak, RandomTrafficKeepsDaysSequentialAndCapped)
{
    for (uint64_t seed : { 1ull, 7ull, 42ull, 1234ull })
    {
        const SoakResult r = RunSoak(seed, 4000);
        EXPECT_GT(r.lastDay, 20u) << "seed " << seed;
        EXPECT_TRUE(r.sequential) << "seed " << seed;
        EXPECT_TRUE(r.bandsOk) << "seed " << seed;
        EXPECT_LE(r.maxPerFrame, 1) << "seed " << seed;
    }
}

TEST(SLOW_GameSpeedSoak, SameSeedGivesIdenticalTickTimeline)
{
    const SoakResult a = RunSoak(99, 3000);
    const SoakResult b = RunSoak(99, 3000);
    ASSERT_GT(a.tickFrames.size(), 10u);
    EXPECT_EQ(a.tickFrames, b.tickFrames);
}

TEST(SLOW_GameSpeedSoak, DifferentTrafficSameDaySequence)
{
    // The days and bands published are the same sequence however the player drove the speed.
    const SoakResult a = RunSoak(3, 4000);
    const SoakResult b = RunSoak(5, 4000);
    const uint64_t common = (std::min)(a.lastDay, b.lastDay);
    ASSERT_GT(common, 10u);
    EXPECT_TRUE(a.sequential && b.sequential);
    EXPECT_TRUE(a.bandsOk && b.bandsOk);
}
