// A4 WeatherModule Task 1: AC 3 (stage id `weather` registered in DoStart, unregistered in DoStop) and the
// WeatherUpdated message round trip. The module is driven headless through its test seam.
#include <gtest/gtest.h>

#include <type_traits>

#include <Modules/Weather/WeatherModule.h>   // first: pulls WorldGenerationModule.h before windows.h
#include <Modules/Weather/WeatherMessages.h>
#include "../WorldStep/WorldStepTestRig.h"
#include <DiaMessageBus/Bus.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <Modules/WorldGen/WorldGenerationMessages.h>
#include "WeatherTestWorld.h"

namespace
{
using Dia::Core::StringCRC;

class WeatherProbe : public CoW::WeatherModule
{
public:
    explicit WeatherProbe(const StringCRC& id) : CoW::WeatherModule(id) {}
    Dia::ApplicationFlow::StartResult CallStart() { return DoStart(); }
    Dia::ApplicationFlow::StopResult CallStop() { return DoStop(); }
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
