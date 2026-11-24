// A4 WeatherModule Task 1: AC 3 (stage id `weather` registered in DoStart, unregistered in DoStop) and the
// WeatherUpdated message round trip. The module is driven headless through its test seam.
#include <gtest/gtest.h>

#include <type_traits>

#include <Modules/Weather/WeatherModule.h>   // first: pulls WorldGenerationModule.h before windows.h
#include <Modules/Weather/WeatherMessages.h>
#include "../WorldStep/WorldStepTestRig.h"
#include <DiaMessageBus/Bus.h>

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
