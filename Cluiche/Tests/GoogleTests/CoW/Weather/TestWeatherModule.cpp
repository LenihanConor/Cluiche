// A4 WeatherModule Task 1: AC 3 (stage id `weather` registered in DoStart, unregistered in DoStop) and the
// WeatherUpdated message round trip. The module is driven headless through its test seam.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>

#include <Modules/Weather/WeatherModule.h>   // first: pulls WorldGenerationModule.h before windows.h
#include <Modules/Weather/WeatherMessages.h>
#include "../WorldStep/WorldStepTestRig.h"
#include <DiaMessageBus/Bus.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <Modules/WorldGen/WorldGenerationMessages.h>
#include "WeatherTestWorld.h"
#include "WeatherParamTestKit.h"

namespace
{
using Dia::Core::StringCRC;

class WeatherProbe : public CoW::WeatherModule
{
public:
    explicit WeatherProbe(const StringCRC& id) : CoW::WeatherModule(id) {}
    Dia::ApplicationFlow::StartResult CallStart() { return DoStart(); }
    Dia::ApplicationFlow::StopResult CallStop() { return DoStop(); }
    void CallConfigure(const char* json) { OnConfigure(json); }
};
}

TEST(WeatherModule, RegistersStageIdWeatherInDoStartAndUnregistersInDoStop)
{
    WsTest::Harness h(WsTest::Config("weather", 7, true));
    WsTest::FakeStepStage probeStage{ StringCRC("weather"), *new WsTest::CallLog() };   // leaked log is test-local
    WeatherProbe module(StringCRC("WeatherModule"));
    CoW::WeatherModule::TestHosts hosts;
    hosts.worldStep = &h.module;
    module.UseTestHosts(hosts);

    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, module.CallStart());
    // A duplicate registration is refused, which proves the module owns the id.
    EXPECT_FALSE(h.module.RegisterStage(StringCRC("weather"), probeStage));

    module.CallStop();
    EXPECT_TRUE(h.module.RegisterStage(StringCRC("weather"), probeStage));
    h.module.UnregisterStage(StringCRC("weather"));
}

TEST(WeatherModule, StartWithoutWorldStepHostIsLoadingNotCrash)
{
    WeatherProbe module(StringCRC("WeatherModule"));
    CoW::WeatherModule::TestHosts hosts;   // worldStep null
    module.UseTestHosts(hosts);
    EXPECT_EQ(Dia::ApplicationFlow::StartResult::kLoading, module.CallStart());
}

TEST(WeatherModule, ModuleCanStartAgainAfterStop)
{
    WsTest::Harness h(WsTest::Config("weather", 7, true));
    WeatherProbe module(StringCRC("WeatherModule"));
    CoW::WeatherModule::TestHosts hosts;
    hosts.worldStep = &h.module;
    module.UseTestHosts(hosts);
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, module.CallStart());
    module.CallStop();
    EXPECT_EQ(Dia::ApplicationFlow::StartResult::kReady, module.CallStart());
    module.CallStop();
}

TEST(WeatherMessages, WeatherUpdatedIsTriviallyCopyableWithTypeId)
{
    static_assert(std::is_trivially_copyable<CoW::WeatherUpdated>::value, "bus messages must be trivially copyable");
    EXPECT_EQ(StringCRC("CoW.WeatherUpdated"), CoW::WeatherUpdated::kTypeId);
}

TEST(WeatherMessages, WeatherUpdatedRoundTripsThroughTheBus)
{
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    bus.RegisterType<CoW::WeatherUpdated, 4>();
    CoW::WeatherUpdated received;
    bool got = false;
    auto sub = bus.Subscribe<CoW::WeatherUpdated>(StringCRC("WeatherTest"), [&](const CoW::WeatherUpdated& m) {
        received = m;
        got = true;
    });
    ASSERT_TRUE(sub.IsValid());

    CoW::WeatherUpdated sent;
    sent.week = 41;
    sent.season = Draoi::HexWorld::Weather::Season::Autumn;
    ASSERT_TRUE(bus.Broadcast(sent));
    bus.Update();

    ASSERT_TRUE(got);
    EXPECT_EQ(41u, received.week);
    EXPECT_EQ(Draoi::HexWorld::Weather::Season::Autumn, received.season);
}

// ---- Task 2: world events through the bus, generation hold (AC 10), no WeatherUpdated during spin-up (AC 7) ----------

namespace
{
class OneWorldSource : public CoW::Weather::IWeatherWorldSource
{
public:
    bool GetWorld(CoW::WorldStep::WorldView& view, Draoi::HexWorld::Weather::WeatherTerrain& terrain) override
    {
        Draoi::HexWorld::Weather::Testing::HexTerrainOptions o;
        o.waterColumns = 1;
        WeatherTestWorld::CopyTerrain(Draoi::HexWorld::Weather::Testing::MakeHexTerrain(6, 5, o), terrain);
        view.present = true;
        view.width = 6;
        view.height = 5;
        return true;
    }
};

struct CountingHold : public CoW::WorldStep::IGenerationHold
{
    void Hold() override { ++holds; }
    void Release() override { ++releases; }
    uint32_t holds = 0, releases = 0;
};

struct ModuleRig
{
    ModuleRig()
        : harness(WsTest::Config("weather", 7, true))
        , module(StringCRC("WeatherModule"))
    {
        bus.Initialize();
        bus.RegisterType<CoW::WorldGenerationComplete, 4>();
        bus.RegisterType<CoW::WorldGenerationFailed, 4>();
        bus.RegisterType<CoW::WeatherUpdated, 4>();
        updates = bus.Subscribe<CoW::WeatherUpdated>(StringCRC("WeatherModuleTestWatcher"),
                                                     [this](const CoW::WeatherUpdated&) { ++weatherUpdated; });
        CoW::WeatherModule::TestHosts hosts;
        hosts.worldStep = &harness.module;
        hosts.bus = &bus;
        hosts.worldSource = &source;
        hosts.generationHold = &hold;
        module.UseTestHosts(hosts);
        module.GetStage().SetRegistry(&Draoi::HexWorld::Weather::Testing::TestRegistry());
        module.GetStage().SetParameters(Draoi::HexWorld::Weather::Testing::MakeTestParams());
        module.GetStage().SetSpinupWeeks(4);
        module.GetStage().SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kSynchronous);
    }
    void Announce(uint32_t revision)
    {
        CoW::WorldGenerationComplete msg;
        msg.worldRevision = revision;
        ASSERT_TRUE(bus.Broadcast(msg));
        bus.Update();
        module.GetStage().Update();
    }
    WsTest::Harness harness;
    WeatherProbe module;
    Dia::MessageBus::Bus bus;
    OneWorldSource source;
    CountingHold hold;
    Dia::MessageBus::BusSubscriptionHandle updates;
    unsigned weatherUpdated = 0;
};
}

TEST(WeatherModule, HoldsGenerationThroughStartAndReleasesOnceSubscribed)
{
    ModuleRig rig;
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    EXPECT_EQ(1u, rig.hold.holds);
    EXPECT_EQ(1u, rig.hold.releases);   // released by NotifySubscribed at the end of DoStart
    rig.module.CallStop();
    EXPECT_EQ(1u, rig.hold.releases);   // Stop does not release a second time
}

TEST(WeatherModule, WorldGenerationCompleteOnTheBusSpinsUpTheStage)
{
    ModuleRig rig;
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    EXPECT_FALSE(rig.module.GetStage().IsReadyForRevision(5));
    rig.Announce(5);
    EXPECT_TRUE(rig.module.GetStage().IsReadyForRevision(5));
    EXPECT_EQ(30u, rig.module.GetStage().GetSnapshot().cellCount);
    EXPECT_EQ(0u, rig.weatherUpdated);   // spin-up never publishes WeatherUpdated
    rig.bus.Update();
    EXPECT_EQ(0u, rig.weatherUpdated);
    rig.module.CallStop();
}

TEST(WeatherModule, WorldGenerationFailedOnTheBusMakesTheStageFailing)
{
    ModuleRig rig;
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    rig.Announce(5);
    ASSERT_TRUE(rig.module.GetStage().IsReady());
    CoW::WorldGenerationFailed failed;
    ASSERT_TRUE(rig.bus.Broadcast(failed));
    rig.bus.Update();
    EXPECT_FALSE(rig.module.GetStage().IsReady());
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kFailing, rig.module.GetStage().GetHealth().status);
    rig.module.CallStop();
}

TEST(WeatherModule, NoEventsAreHandledAfterStop)
{
    ModuleRig rig;
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    rig.module.CallStop();
    rig.Announce(5);
    EXPECT_FALSE(rig.module.GetStage().IsReadyForRevision(5));
}

// ---- Task 5: config (AC 2), production sidecar from config, lifecycle (AC 29, 30), health vocabulary ---------------

namespace
{
using Dia::ApplicationFlow::StartResult;
using Dia::Observation::Health::HealthStatus;
using Dia::Observation::Log::LogLevel;

struct ConfigCase { const char* json; bool ok; uint32_t weeks; };

// Captures WTH log lines for the scope of a test.
struct LogCapture
{
    LogCapture()
    {
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.RegisterThreadBuffer();
        l.RegisterSink(&log);
    }
    ~LogCapture()
    {
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.FlushSync();
        l.UnregisterSink(&log);
        l.UnregisterThreadBuffer();
    }
    unsigned Count(LogLevel level, const char* needle)
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
        return log.Count(level, needle);
    }
    WeatherParamKit::WthLog log;
};

std::filesystem::path WriteTemp(const char* name, const std::string& text)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    return path;
}

std::string JsonEscape(const std::string& s)
{
    std::string r;
    for (char c : s) { if (c == '\\') { r += "\\\\"; } else { r += c; } }
    return r;
}
}

TEST(WeatherModuleConfig, SpinupWeeksAcceptsTheWholeRangeAndDefaultsTo52)
{
    const ConfigCase cases[] = {
        { "", true, 52 }, { "{}", true, 52 }, { "{\"spinup_weeks\":0}", true, 0 }, { "{\"spinup_weeks\":1}", true, 1 },
        { "{\"spinup_weeks\":52}", true, 52 }, { "{\"spinup_weeks\":520}", true, 520 },
        { "{\"spinup_weeks\":13.0}", true, 13 },
    };
    for (const ConfigCase& c : cases)
    {
        WeatherProbe module(StringCRC("WeatherModule"));
        module.CallConfigure(c.json);
        EXPECT_TRUE(module.ConfigOk()) << c.json;
        EXPECT_EQ(c.weeks, module.ConfiguredSpinupWeeks()) << c.json;
    }
}

TEST(WeatherModuleConfig, SpinupWeeksOutOfRangeOrNonIntegerIsAConfigErrorNamingTheKey)
{
    const char* bad[] = { "{\"spinup_weeks\":-1}", "{\"spinup_weeks\":521}", "{\"spinup_weeks\":1000000}",
                          "{\"spinup_weeks\":52.5}", "{\"spinup_weeks\":\"52\"}", "{\"spinup_weeks\":null}",
                          "{\"spinup_weeks\":true}", "{\"spinup_weeks\":[52]}" };
    for (const char* json : bad)
    {
        LogCapture logs;
        WeatherProbe module(StringCRC("WeatherModule"));
        module.CallConfigure(json);
        EXPECT_FALSE(module.ConfigOk()) << json;
        EXPECT_EQ("spinup_weeks", module.ConfigErrorKey()) << json;
        EXPECT_FALSE(module.ConfigErrorReason().empty()) << json;
        EXPECT_GE(logs.Count(LogLevel::kError, "spinup_weeks"), 1u) << json;
    }
}

TEST(WeatherModuleConfig, ConfigErrorStopsTheModuleStartingAndRegistersNothing)
{
    WsTest::Harness h(WsTest::Config("weather", 7, true));
    WeatherProbe module(StringCRC("WeatherModule"));
    CoW::WeatherModule::TestHosts hosts;
    hosts.worldStep = &h.module;
    module.UseTestHosts(hosts);
    module.CallConfigure("{\"spinup_weeks\":9999}");
    EXPECT_EQ(StartResult::kFailed, module.CallStart());
    WsTest::FakeStepStage probeStage{ StringCRC("weather"), *new WsTest::CallLog() };
    EXPECT_TRUE(h.module.RegisterStage(StringCRC("weather"), probeStage));   // the id is still free
    h.module.UnregisterStage(StringCRC("weather"));
}

TEST(WeatherModuleConfig, MalformedOrWrongTypedKeysAreConfigErrors)
{
    struct Bad { const char* json; const char* key; };
    const Bad cases[] = {
        { "[1,2]", "config" }, { "{\"sidecar\":5}", "sidecar" }, { "{\"sidecar\":\"\"}", "sidecar" },
        { "{\"packs\":\"a\"}", "packs" }, { "{\"packs\":[5]}", "packs" }, { "{\"packs\":[\"\"]}", "packs" },
    };
    for (const Bad& c : cases)
    {
        WeatherProbe module(StringCRC("WeatherModule"));
        module.CallConfigure(c.json);
        EXPECT_FALSE(module.ConfigOk()) << c.json;
        EXPECT_EQ(c.key, module.ConfigErrorKey()) << c.json;
    }
}

TEST(WeatherModuleConfig, ClimateBlockIsAcceptedAndIgnoredWithOneInfoLog)
{
    LogCapture logs;
    WeatherProbe module(StringCRC("WeatherModule"));
    module.CallConfigure("{\"climate\":{\"file\":\"x.json\",\"start\":\"warm\"},\"spinup_weeks\":10}");
    EXPECT_TRUE(module.ConfigOk());
    EXPECT_TRUE(module.ClimateConfigIgnored());
    EXPECT_EQ(10u, module.ConfiguredSpinupWeeks());
    EXPECT_EQ(1u, logs.Count(LogLevel::kInfo, "climate"));
}

TEST(WeatherModuleConfig, ConfiguredSpinupWeeksDrivesTheSpinUpLength)
{
    for (uint32_t weeks : { 0u, 2u, 7u })
    {
        ModuleRig rig;
        rig.module.CallConfigure((std::string("{\"spinup_weeks\":") + std::to_string(weeks) + "}").c_str());
        uint32_t seen = 0;
        rig.module.GetStage().SetSpinUpWeekObserver([&](uint32_t) { ++seen; });
        ASSERT_EQ(StartResult::kReady, rig.module.CallStart());
        rig.Announce(3);
        EXPECT_TRUE(rig.module.GetStage().IsReadyForRevision(3)) << weeks;
        EXPECT_EQ(weeks, seen);
        rig.module.CallStop();
    }
}

TEST(WeatherModuleConfig, SidecarPathFromConfigBuildsTheProductionSourceAndLoadsParameters)
{
    const std::filesystem::path sidecar = WriteTemp("cow_weather_cfg_sidecar.json", WeatherParamKit::SidecarText());
    ModuleRig rig;
    rig.module.CallConfigure(("{\"sidecar\":\"" + JsonEscape(sidecar.string()) + "\"}").c_str());
    ASSERT_TRUE(rig.module.ConfigOk());
    ASSERT_EQ(StartResult::kReady, rig.module.CallStart());
    EXPECT_TRUE(rig.module.GetStage().Parameters().IsLoaded());
    EXPECT_EQ(HealthStatus::kOK, rig.module.GetStage().GetHealth().status);
    rig.module.CallStop();
    std::filesystem::remove(sidecar);
}

TEST(WeatherModuleConfig, UnreadableConfiguredSidecarIsFailingAndNeverReady)
{
    ModuleRig rig;
    rig.module.CallConfigure("{\"sidecar\":\"Z:/definitely/not/here/weather.json\"}");
    ASSERT_EQ(StartResult::kReady, rig.module.CallStart());   // the module starts; the stage is Failing
    EXPECT_EQ(HealthStatus::kFailing, rig.module.GetStage().GetHealth().status);
    rig.Announce(1);
    EXPECT_FALSE(rig.module.GetStage().IsReady());
    rig.module.CallStop();
}

TEST(WeatherModuleConfig, InlinePackAndPackFileFromConfigAreApplied)
{
    const std::filesystem::path sidecar = WriteTemp("cow_weather_cfg_sidecar2.json", WeatherParamKit::SidecarText());
    const std::filesystem::path pack = WriteTemp(
        "cow_weather_cfg_pack.json", "{\"format\":\"cow.weather_params\",\"version\":1,\"solar.cloud_albedo\":0.55}");
    ModuleRig rig;
    // Later packs win: the inline 0.45 is overridden by the file's 0.55.
    rig.module.CallConfigure(("{\"sidecar\":\"" + JsonEscape(sidecar.string())
                              + "\",\"packs\":[{\"format\":\"cow.weather_params\",\"version\":1,\"solar.cloud_albedo\":0.45}, \""
                              + JsonEscape(pack.string()) + "\"]}").c_str());
    ASSERT_TRUE(rig.module.ConfigOk());
    ASSERT_EQ(StartResult::kReady, rig.module.CallStart());
    ASSERT_TRUE(rig.module.GetStage().Parameters().IsLoaded());
    EXPECT_EQ(HealthStatus::kOK, rig.module.GetStage().GetHealth().status);
    Draoi::HexWorld::Weather::WeatherParameters p;
    rig.module.GetStage().Parameters().Snapshot(p);
    EXPECT_FLOAT_EQ(0.55f, p.cloudAlbedo);
    rig.module.CallStop();
    std::filesystem::remove(sidecar);
    std::filesystem::remove(pack);
}

TEST(WeatherModuleConfig, UnreadablePackFileFailsTheStage)
{
    const std::filesystem::path sidecar = WriteTemp("cow_weather_cfg_sidecar3.json", WeatherParamKit::SidecarText());
    ModuleRig rig;
    rig.module.CallConfigure(("{\"sidecar\":\"" + JsonEscape(sidecar.string()) + "\",\"packs\":[\"Z:/nope/pack.json\"]}").c_str());
    ASSERT_EQ(StartResult::kReady, rig.module.CallStart());
    EXPECT_EQ(HealthStatus::kFailing, rig.module.GetStage().GetHealth().status);
    EXPECT_EQ(StringCRC("packs"), rig.module.GetStage().GetHealth().reason);
    rig.module.CallStop();
    std::filesystem::remove(sidecar);
}

// ---- Lifecycle: AC 29 (enter, exit, enter) ------------------------------------------------------------------------

TEST(WeatherModuleLifecycle, NothingFromTheFirstVisitSurvivesExitAndReenter)
{
    ModuleRig rig;
    ASSERT_EQ(StartResult::kReady, rig.module.CallStart());
    rig.Announce(5);
    ASSERT_TRUE(rig.module.GetStage().IsReadyForRevision(5));
    ASSERT_TRUE(rig.module.QueueClearWindOverrides());
    ASSERT_TRUE(rig.module.QueueReload());
    EXPECT_EQ(2u, rig.module.GetStage().QueueCount());
    const uint32_t visit1 = rig.module.GetStage().VisitGeneration();

    rig.module.CallStop();
    // Exit: unregistered (the id is free again), sims gone, queue empty.
    WsTest::FakeStepStage probeStage{ StringCRC("weather"), *new WsTest::CallLog() };
    EXPECT_TRUE(rig.harness.module.RegisterStage(StringCRC("weather"), probeStage));
    rig.harness.module.UnregisterStage(StringCRC("weather"));
    EXPECT_FALSE(rig.module.GetStage().IsReady());
    EXPECT_EQ(0u, rig.module.GetStage().QueueCount());
    EXPECT_EQ(0u, rig.module.GetStage().GetSnapshot().cellCount);

    ASSERT_EQ(StartResult::kReady, rig.module.CallStart());
    EXPECT_GT(rig.module.GetStage().VisitGeneration(), visit1);
    EXPECT_FALSE(rig.module.GetStage().IsReady());
    EXPECT_FALSE(rig.module.GetStage().IsReadyForRevision(5));
    EXPECT_EQ(0u, rig.module.GetStage().QueueCount());
    EXPECT_EQ(0u, rig.module.GetStage().GetSnapshot().cellCount);
    EXPECT_FALSE(rig.harness.module.RegisterStage(StringCRC("weather"), probeStage));   // registered again by the new visit
    rig.module.CallStop();
}

TEST(WeatherModuleLifecycle, ASpinUpJobFinishingAfterExitIsDiscardedByTheNextVisit)
{
    ModuleRig rig;
    rig.module.GetStage().SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kManual);
    ASSERT_EQ(StartResult::kReady, rig.module.CallStart());
    rig.Announce(5);
    ASSERT_EQ(1u, rig.module.GetStage().QueuedJobCount());   // the job has not run yet
    rig.module.CallStop();

    ASSERT_EQ(StartResult::kReady, rig.module.CallStart());
    rig.module.GetStage().RunNextJob();   // the first visit's job finishes now, in the second visit
    rig.module.GetStage().Update();
    rig.module.GetStage().Update();
    EXPECT_FALSE(rig.module.GetStage().IsReadyForRevision(5));
    EXPECT_FALSE(rig.module.GetStage().IsReady());
    EXPECT_EQ(0u, rig.module.GetStage().GetSnapshot().cellCount);
    EXPECT_EQ(0u, rig.weatherUpdated);

    // A world announced in the new visit spins up normally.
    rig.module.GetStage().SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kSynchronous);
    rig.Announce(6);
    EXPECT_TRUE(rig.module.GetStage().IsReadyForRevision(6));
    rig.module.CallStop();
}

TEST(WeatherModuleLifecycle, RepeatedEnterExitCyclesStayClean)
{
    ModuleRig rig;
    for (uint32_t visit = 0; visit < 4; ++visit)
    {
        ASSERT_EQ(StartResult::kReady, rig.module.CallStart()) << visit;
        rig.Announce(visit + 1);
        EXPECT_TRUE(rig.module.GetStage().IsReadyForRevision(visit + 1)) << visit;
        rig.module.CallStop();
        EXPECT_FALSE(rig.module.GetStage().IsReady()) << visit;
    }
    EXPECT_EQ(rig.hold.holds, rig.hold.releases);
}

// ---- AC 30: DoStop during a running spin-up cancels and joins -----------------------------------------------------

TEST(WeatherModuleLifecycle, DoStopDuringARunningSpinUpCancelsAndJoinsWithoutCrash)
{
    ModuleRig rig;
    rig.module.CallConfigure("{\"spinup_weeks\":520}");
    rig.module.GetStage().SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kThread);
    std::atomic<uint32_t> weeksRun{ 0 };
    std::atomic<bool> started{ false };
    rig.module.GetStage().SetSpinUpWeekObserver([&](uint32_t) {
        ++weeksRun;
        started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));   // keeps the job running long enough to stop it
    });
    ASSERT_EQ(StartResult::kReady, rig.module.CallStart());
    CoW::WorldGenerationComplete msg;
    msg.worldRevision = 5;
    ASSERT_TRUE(rig.bus.Broadcast(msg));
    rig.bus.Update();
    for (int i = 0; i < 2000 && !started.load(); ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    ASSERT_TRUE(started.load());

    rig.module.CallStop();   // cancels and joins
    const uint32_t atStop = weeksRun.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(atStop, weeksRun.load());   // the job is gone: nothing runs after Stop returns
    EXPECT_LT(atStop, 520u);
    EXPECT_FALSE(rig.module.GetStage().IsReady());
    EXPECT_EQ(0u, rig.weatherUpdated);
}

// ---- Observability: spin-up and world logs ------------------------------------------------------------------------

TEST(WeatherModuleObservability, WorldAnnounceAndSpinUpStartAndFinishAreLoggedOnWth)
{
    LogCapture logs;
    ModuleRig rig;
    ASSERT_EQ(StartResult::kReady, rig.module.CallStart());
    rig.Announce(5);
    EXPECT_EQ(1u, logs.Count(LogLevel::kInfo, "world 5 announced"));
    EXPECT_EQ(1u, logs.Count(LogLevel::kInfo, "spin-up start for world 5"));
    EXPECT_EQ(1u, logs.Count(LogLevel::kInfo, "spin-up finished for world 5"));
    rig.module.CallStop();
}

// ---- Health vocabulary: Degraded / Failing, never the other word --------------------------------------------------

TEST(WeatherModuleHealth, NoWeatherSourceOrTestUsesTheForbiddenHealthWord)
{
    const std::string needle = std::string("Un") + "healthy";
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    std::filesystem::path root;
    while (!dir.empty())
    {
        if (std::filesystem::is_directory(dir / "Cluiche/CoW/Shared/Weather", ec)) { root = dir; break; }
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) { break; }
        dir = parent;
    }
    ASSERT_FALSE(root.empty());
    const std::filesystem::path dirs[] = { root / "Cluiche/CoW/Shared/Weather", root / "Cluiche/CoW/Modules/Weather",
                                           root / "Cluiche/Tests/GoogleTests/CoW/Weather", root / "Draoi/DraoiHexWorldWeather" };
    unsigned scanned = 0;
    for (const auto& d : dirs)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(d, ec))
        {
            if (!entry.is_regular_file()) { continue; }
            std::ifstream in(entry.path(), std::ios::binary);
            std::stringstream ss;
            ss << in.rdbuf();
            ++scanned;
            EXPECT_EQ(std::string::npos, ss.str().find(needle)) << entry.path().string();
        }
    }
    EXPECT_GT(scanned, 10u);
}
