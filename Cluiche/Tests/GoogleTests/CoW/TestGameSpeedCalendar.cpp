// Equivalence tests: CoW's calendar and TimeBand, built from data (CoWCalendar()), must reproduce the
// original hard-coded ResolveBand / WeekOf / SeasonOf / YearOf results exactly.
// Spec: docs/specs/features/draoi/extract-draoigamespeed.md (task 1)
#include <gtest/gtest.h>
#include <Shared/CoWCalendar.h>
#include <DiaCore/Json/external/json/json.h>
#include <fstream>
#include <functional>
#include <string>

using namespace CoW;

namespace {

// The original formulas, re-implemented here as the oracle (copied from the pre-parameterisation header).
TimeBand OriginalResolveBand(uint64_t day)
{
    if (day % 36400 == 0) return TimeBand::Century;
    if (day % 3640  == 0) return TimeBand::Decade;
    if (day % 364   == 0) return TimeBand::Year;
    if (day % 91    == 0) return TimeBand::Season;
    if (day % 7     == 0) return TimeBand::Week;
    return TimeBand::Day;
}
uint64_t OriginalWeekOf(uint64_t day)   { return day / 7; }
uint64_t OriginalSeasonOf(uint64_t day) { return day / 91; }
uint64_t OriginalYearOf(uint64_t day)   { return day / 364; }

void ExpectMatchesOriginal(uint64_t day)
{
    const Calendar cal = CoWCalendar();
    EXPECT_EQ(static_cast<TimeBand>(cal.BandOf(day)), OriginalResolveBand(day)) << "day " << day;
    EXPECT_EQ(ResolveBand(day), OriginalResolveBand(day)) << "day " << day;
    EXPECT_EQ(cal.PeriodOf(0, day), OriginalWeekOf(day)) << "day " << day;
    EXPECT_EQ(cal.PeriodOf(1, day), OriginalSeasonOf(day)) << "day " << day;
    EXPECT_EQ(cal.PeriodOf(2, day), OriginalYearOf(day)) << "day " << day;
    EXPECT_EQ(WeekOf(day), OriginalWeekOf(day)) << "day " << day;
    EXPECT_EQ(SeasonOf(day), OriginalSeasonOf(day)) << "day " << day;
    EXPECT_EQ(YearOf(day), OriginalYearOf(day)) << "day " << day;
}

} // namespace

TEST(GameSpeedCalendarData, CoWCalendarShape)
{
    const Calendar cal = CoWCalendar();
    ASSERT_EQ(cal.periods.size(), 5u);
    EXPECT_EQ(cal.PeriodLengthDays(0), 7u);
    EXPECT_EQ(cal.PeriodLengthDays(1), 91u);
    EXPECT_EQ(cal.PeriodLengthDays(2), 364u);
    EXPECT_EQ(cal.PeriodLengthDays(3), 3640u);
    EXPECT_EQ(cal.PeriodLengthDays(4), 36400u);
    ASSERT_EQ(cal.bandNames.size(), 6u);
    EXPECT_STREQ(cal.BandName(static_cast<uint8_t>(TimeBand::Day)), "Day");
    EXPECT_STREQ(cal.BandName(static_cast<uint8_t>(TimeBand::Week)), "Week");
    EXPECT_STREQ(cal.BandName(static_cast<uint8_t>(TimeBand::Season)), "Season");
    EXPECT_STREQ(cal.BandName(static_cast<uint8_t>(TimeBand::Year)), "Year");
    EXPECT_STREQ(cal.BandName(static_cast<uint8_t>(TimeBand::Decade)), "Decade");
    EXPECT_STREQ(cal.BandName(static_cast<uint8_t>(TimeBand::Century)), "Century");
    EXPECT_TRUE(cal.IsValid());
}

TEST(GameSpeedCalendarData, DayZeroAndBoundariesMatchOriginal)
{
    const uint64_t days[] = { 0, 1, 6, 7, 8, 89, 90, 91, 92, 181, 182, 363, 364, 365, 3639, 3640, 3641,
                              36399, 36400, 36401, 72800, 364000 };
    for (uint64_t d : days)
        ExpectMatchesOriginal(d);
}

TEST(GameSpeedCalendarData, ExplicitBandAtBoundaries)
{
    EXPECT_EQ(ResolveBand(0), TimeBand::Century);
    EXPECT_EQ(ResolveBand(6), TimeBand::Day);
    EXPECT_EQ(ResolveBand(7), TimeBand::Week);
    EXPECT_EQ(ResolveBand(91), TimeBand::Season);
    EXPECT_EQ(ResolveBand(364), TimeBand::Year);
    EXPECT_EQ(ResolveBand(3640), TimeBand::Decade);
    EXPECT_EQ(ResolveBand(36400), TimeBand::Century);
}

TEST(GameSpeedCalendarData, LargeDaySweepMatchesOriginal)
{
    // Dense sweep of the first ~110k days, then strided sweep of large values.
    for (uint64_t d = 0; d < 110000; ++d)
        ExpectMatchesOriginal(d);
    for (uint64_t d = 1; d < (1ull << 40); d = d * 3 + 1)
        ExpectMatchesOriginal(d);
    for (uint64_t k = 1; k < 2000; ++k)
        ExpectMatchesOriginal(k * 36400ull * 1000003ull);
}

TEST(GameSpeedCalendarData, NoCalendarEveryTickIsPlainDay)
{
    const Calendar none;
    EXPECT_TRUE(none.periods.empty());
    for (uint64_t d : { 0ull, 1ull, 7ull, 91ull, 364ull, 36400ull, 123456789ull })
        EXPECT_EQ(none.BandOf(d), 0) << "day " << d;
    // PeriodOf on a missing rung is the day itself (no period structure).
    EXPECT_EQ(none.PeriodOf(0, 42), 42u);
}

TEST(GameSpeedCalendarData, CustomSmallCalendar)
{
    // day -> "cycle" 3 days -> "epoch" 2 cycles (6 days) -> "era" 5 epochs (30 days)
    Calendar cal;
    cal.periods = { { "Cycle", 3 }, { "Epoch", 2 }, { "Era", 5 } };
    cal.bandNames = { "Tick", "Cycle", "Epoch", "Era" };
    ASSERT_TRUE(cal.IsValid());
    EXPECT_EQ(cal.PeriodLengthDays(2), 30u);
    EXPECT_EQ(cal.PeriodOf(0, 7), 2u);
    EXPECT_EQ(cal.PeriodOf(1, 13), 2u);
    EXPECT_EQ(cal.PeriodOf(2, 65), 2u);
    EXPECT_EQ(cal.BandOf(1), 0);
    EXPECT_EQ(cal.BandOf(3), 1);
    EXPECT_EQ(cal.BandOf(9), 1);
    EXPECT_EQ(cal.BandOf(6), 2);
    EXPECT_EQ(cal.BandOf(30), 3);
    EXPECT_EQ(cal.BandOf(60), 3);
    EXPECT_STREQ(cal.BandName(3), "Era");
    EXPECT_STREQ(cal.BandName(9), "Unknown");
}

TEST(GameSpeedCalendarData, InvalidCalendarRejected)
{
    Calendar zero;
    zero.periods = { { "Bad", 0 } };
    EXPECT_FALSE(zero.IsValid());
    Calendar tooManyBands;
    tooManyBands.periods = { { "Week", 7 } };
    tooManyBands.bandNames = { "A", "B", "C" };
    EXPECT_FALSE(tooManyBands.IsValid());
}

// Drift guard: the "calendar" block shipped in cow_main.diaapp must equal CoWCalendar().
TEST(GameSpeedCalendarData, ShippedDiaappCalendarMatchesCoWCalendar)
{
    // Located relative to this source file: <repo>/Cluiche/Tests/GoogleTests/CoW -> <repo>/Cluiche/Assets/CoW.
    const std::string path = std::string(__FILE__) + "/../../../../Assets/CoW/cow_main.diaapp";
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.is_open()) << path;
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(text, root));

    const Json::Value* block = nullptr;
    std::function<void(const Json::Value&)> find = [&](const Json::Value& v)
    {
        if (block) return;
        if (v.isObject())
        {
            if (v.isMember("instance_id") && v["instance_id"].asString() == "GameSpeedControllerModule" && v["config"].isMember("calendar"))
                block = &v["config"]["calendar"];
            for (const std::string& k : v.getMemberNames()) find(v[k]);
        }
        else if (v.isArray())
            for (const Json::Value& e : v) find(e);
    };
    find(root);
    ASSERT_NE(block, nullptr);

    const Calendar expected = CoWCalendar();
    const Json::Value& periods = (*block)["periods"];
    ASSERT_EQ(periods.size(), expected.periods.size());
    for (Json::ArrayIndex i = 0; i < periods.size(); ++i)
    {
        EXPECT_EQ(periods[i]["name"].asString(), expected.periods[i].name);
        EXPECT_EQ(periods[i]["multiple"].asUInt64(), expected.periods[i].multiple);
    }
    const Json::Value& bands = (*block)["bands"];
    ASSERT_EQ(bands.size(), expected.bandNames.size());
    for (Json::ArrayIndex i = 0; i < bands.size(); ++i)
        EXPECT_EQ(bands[i].asString(), expected.bandNames[i]);
}
