// A4 WeatherModule Task 1: the scaffold of the stage, entry, latch and cause codes.
#include <gtest/gtest.h>

#include <cstring>
#include <type_traits>

#include <Shared/Weather/WeatherCause.h>
#include <Shared/Weather/WeatherEntry.h>
#include <Shared/Weather/WeatherLatch.h>
#include <Shared/Weather/WeatherStage.h>

namespace
{
using CoW::Weather::WeatherEntry;
using CoW::Weather::WeatherLatch;
using CoW::Weather::WeatherStage;
using Draoi::HexWorld::Weather::WeatherSimulation;

typedef CoW::WorldStep::WorldStepStageBase<WeatherSimulation, WeatherEntry, WeatherLatch> WeatherBase;

static_assert(std::is_base_of<WeatherBase, WeatherStage>::value, "WeatherStage derives the world-step stage base");
static_assert(std::is_base_of<Dia::DeferredStep::IStepStage, WeatherStage>::value, "and so is an IStepStage");
static_assert(std::is_trivially_copyable<WeatherEntry>::value, "entries ride the mutation queue by value");
}

TEST(WeatherStage, IsAWorldStepStageBaseInstantiation)
{
    WeatherStage stage;
    Dia::DeferredStep::IStepStage* asStage = &stage;
    EXPECT_NE(nullptr, asStage);
}

TEST(WeatherStage, StageIdIsWeather)
{
    WeatherStage stage;
    EXPECT_STREQ("weather", stage.GetStageId().c_str());
    EXPECT_STREQ("weather", stage.SystemToken());
}

TEST(WeatherStage, NotReadyBeforeAnyWorld)
{
    WeatherStage stage;
    stage.Start();
    EXPECT_FALSE(stage.IsReady());
    stage.Stop();
}

TEST(WeatherStage, EntryDefaultsToNoneKindAndZeroPayload)
{
    WeatherEntry e;
    EXPECT_EQ(CoW::Weather::EntryKind::kNone, e.kind);
    EXPECT_EQ('\0', e.key[0]);
}

TEST(WeatherStage, EntryKeyHoldsSixtyThreeChars)
{
    WeatherEntry e;
    EXPECT_EQ(64u, sizeof(e.key));
}

TEST(WeatherStage, CauseTextNamesEveryCode)
{
    using namespace CoW::Weather;
    EXPECT_STREQ("none", CauseText(CauseCode::kNone));
    EXPECT_STREQ("no_water_source", CauseText(CauseCode::kNoWaterSource));
    EXPECT_STREQ("parameter_fault", CauseText(CauseCode::kParameterFault));
    EXPECT_STREQ("unknown", CauseText(static_cast<CauseCode>(250)));
}

TEST(WeatherStage, CauseTextFnAdaptsToBaseSignature)
{
    EXPECT_STREQ("no_water_source",
                 CoW::Weather::CauseTextRaw(static_cast<uint32_t>(CoW::Weather::CauseCode::kNoWaterSource)));
}
