#include <gtest/gtest.h>
#include <Shared/CoWCalendar.h>
#include <limits>

using namespace CoW;

// CoW calendar and TimeBand values over the generic Calendar (moved from TestGameSpeedController).

TEST(GameSpeedCalendar, ConstantsMatchSpec)
{
    EXPECT_EQ(kDaysPerWeek, 7u);
    EXPECT_EQ(kDaysPerSeason, 91u);
    EXPECT_EQ(kDaysPerYear, 364u);
    EXPECT_EQ(kDaysPerDecade, 3640u);
    EXPECT_EQ(kDaysPerCentury, 36400u);
    EXPECT_EQ(kGameMicrosecondsPerDay, 1000000);
}

TEST(GameSpeedCalendar, ResolveBandAtBoundaries)
{
    EXPECT_EQ(ResolveBand(1), TimeBand::Day);
    EXPECT_EQ(ResolveBand(7), TimeBand::Week);
    EXPECT_EQ(ResolveBand(91), TimeBand::Season);
    EXPECT_EQ(ResolveBand(364), TimeBand::Year);
    EXPECT_EQ(ResolveBand(3640), TimeBand::Decade);
    EXPECT_EQ(ResolveBand(36400), TimeBand::Century);
}

TEST(GameSpeedCalendar, ResolveBandBetweenBoundaries)
{
    EXPECT_EQ(ResolveBand(2), TimeBand::Day);
    EXPECT_EQ(ResolveBand(6), TimeBand::Day);
    EXPECT_EQ(ResolveBand(8), TimeBand::Day);
    EXPECT_EQ(ResolveBand(14), TimeBand::Week);
    EXPECT_EQ(ResolveBand(90), TimeBand::Day);
    EXPECT_EQ(ResolveBand(92), TimeBand::Day);
    EXPECT_EQ(ResolveBand(182), TimeBand::Season);
    EXPECT_EQ(ResolveBand(273), TimeBand::Season);
    EXPECT_EQ(ResolveBand(363), TimeBand::Day);
    EXPECT_EQ(ResolveBand(728), TimeBand::Year);
    EXPECT_EQ(ResolveBand(7280), TimeBand::Decade);
    EXPECT_EQ(ResolveBand(72800), TimeBand::Century);
}

TEST(GameSpeedCalendar, YearBoundaryIsAlsoSeasonAndWeekButResolvesLargest)
{
    EXPECT_EQ(364u % kDaysPerWeek, 0u);
    EXPECT_EQ(364u % kDaysPerSeason, 0u);
    EXPECT_EQ(ResolveBand(364), TimeBand::Year);
}

TEST(GameSpeedCalendar, DayZeroResolvesCentury)
{
    EXPECT_EQ(ResolveBand(0), TimeBand::Century);
}

TEST(GameSpeedCalendar, BandOrderingSupportsGating)
{
    EXPECT_LT(TimeBand::Day, TimeBand::Week);
    EXPECT_LT(TimeBand::Week, TimeBand::Season);
    EXPECT_LT(TimeBand::Season, TimeBand::Year);
    EXPECT_LT(TimeBand::Year, TimeBand::Decade);
    EXPECT_LT(TimeBand::Decade, TimeBand::Century);
    EXPECT_TRUE(ResolveBand(91) >= TimeBand::Season);
    EXPECT_FALSE(ResolveBand(7) >= TimeBand::Season);
    EXPECT_TRUE(ResolveBand(7) >= TimeBand::Week);
}

TEST(GameSpeedCalendar, BandCountsOverOneYear)
{
    int weeks = 0, seasons = 0, years = 0;
    for (uint64_t d = 1; d <= 364; ++d)
    {
        const TimeBand b = ResolveBand(d);
        if (b == TimeBand::Week)   ++weeks;
        if (b == TimeBand::Season) ++seasons;
        if (b == TimeBand::Year)   ++years;
    }
    EXPECT_EQ(years, 1);
    EXPECT_EQ(seasons, 3);     // 91, 182, 273 (364 is Year)
    EXPECT_EQ(weeks, 52 - 4);  // 52 week boundaries minus the 4 season-or-larger ones
}

TEST(GameSpeedCalendar, WeekSeasonYearOf)
{
    EXPECT_EQ(WeekOf(0), 0u);
    EXPECT_EQ(WeekOf(6), 0u);
    EXPECT_EQ(WeekOf(7), 1u);
    EXPECT_EQ(SeasonOf(90), 0u);
    EXPECT_EQ(SeasonOf(91), 1u);
    EXPECT_EQ(SeasonOf(363), 3u);
    EXPECT_EQ(SeasonOf(364), 4u);
    EXPECT_EQ(YearOf(363), 0u);
    EXPECT_EQ(YearOf(364), 1u);
    EXPECT_EQ(YearOf(728), 2u);
}

TEST(GameSpeedCalendar, NamesAreStable)
{
    EXPECT_STREQ(TimeBandName(TimeBand::Day), "Day");
    EXPECT_STREQ(TimeBandName(TimeBand::Century), "Century");
    EXPECT_STREQ(SpeedTierName(SpeedTier::Paused), "Paused");
    EXPECT_STREQ(SpeedTierName(SpeedTier::Fastest), "Fastest");
    EXPECT_STREQ(TimeBandName(static_cast<TimeBand>(99)), "Unknown");
    EXPECT_STREQ(SpeedTierName(static_cast<SpeedTier>(99)), "Unknown");
}

// ---------------------------------------------------------------------------
// Config validation
// ---------------------------------------------------------------------------
TEST(GameSpeedCalendar, CenturyCensusMatchesCalendarArithmetic)
{
    uint64_t counts[6] = {};
    for (uint64_t d = 1; d <= kDaysPerCentury; ++d)
        ++counts[static_cast<int>(ResolveBand(d))];
    EXPECT_EQ(counts[static_cast<int>(TimeBand::Century)], 1u);
    EXPECT_EQ(counts[static_cast<int>(TimeBand::Decade)], 9u);
    EXPECT_EQ(counts[static_cast<int>(TimeBand::Year)], 90u);
    EXPECT_EQ(counts[static_cast<int>(TimeBand::Season)], 300u);
    EXPECT_EQ(counts[static_cast<int>(TimeBand::Week)], 4800u);
    EXPECT_EQ(counts[static_cast<int>(TimeBand::Day)], kDaysPerCentury - 1 - 9 - 90 - 300 - 4800);
}

TEST(GameSpeedCalendar, BandNeverSmallerThanWeekOnWeekMultiples)
{
    for (uint64_t d = 7; d <= kDaysPerCentury; d += 7)
        EXPECT_GE(ResolveBand(d), TimeBand::Week) << d;
}
