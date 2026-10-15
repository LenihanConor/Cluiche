#include <gtest/gtest.h>
#include <Shared/GameSpeedController.h>
#include <limits>

using namespace CoW;

namespace {
SpeedPresetConfig Cfg() { return DefaultSpeedPresetConfig(); }
GameSpeedController Make() { return GameSpeedController(Cfg()); }
}

// ---------------------------------------------------------------------------
// Calendar
// ---------------------------------------------------------------------------
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
TEST(GameSpeedConfig, DefaultsAreValidAt30Hz)
{
    EXPECT_EQ(ValidateSpeedPresetConfig(DefaultSpeedPresetConfig(), 30.0f), PresetConfigError::None);
}

TEST(GameSpeedConfig, RejectsNonPositive)
{
    for (int i = 0; i < 4; ++i)
    {
        SpeedPresetConfig c = Cfg();
        c.wallSecondsPerDay[i] = 0.0f;
        EXPECT_EQ(ValidateSpeedPresetConfig(c, 30.0f), PresetConfigError::NonPositive) << i;
        c = Cfg(); c.wallSecondsPerDay[i] = -1.0f;
        EXPECT_EQ(ValidateSpeedPresetConfig(c, 30.0f), PresetConfigError::NonPositive) << i;
        c = Cfg(); c.wallSecondsPerDay[i] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_EQ(ValidateSpeedPresetConfig(c, 30.0f), PresetConfigError::NonPositive) << i;
        c = Cfg(); c.wallSecondsPerDay[i] = std::numeric_limits<float>::infinity();
        EXPECT_EQ(ValidateSpeedPresetConfig(c, 30.0f), PresetConfigError::NonPositive) << i;
    }
}

TEST(GameSpeedConfig, RejectsNonDecreasing)
{
    SpeedPresetConfig equal{ { 10.0f, 10.0f, 5.0f, 1.0f } };
    EXPECT_EQ(ValidateSpeedPresetConfig(equal, 30.0f), PresetConfigError::NotDecreasing);
    SpeedPresetConfig increasing{ { 1.0f, 2.0f, 3.0f, 4.0f } };
    EXPECT_EQ(ValidateSpeedPresetConfig(increasing, 30.0f), PresetConfigError::NotDecreasing);
    SpeedPresetConfig lastEqual{ { 60.0f, 30.0f, 10.0f, 10.0f } };
    EXPECT_EQ(ValidateSpeedPresetConfig(lastEqual, 30.0f), PresetConfigError::NotDecreasing);
}

TEST(GameSpeedConfig, RejectsFastestTooFast)
{
    SpeedPresetConfig c{ { 60.0f, 30.0f, 10.0f, 0.01f } };   // < 1/30
    EXPECT_EQ(ValidateSpeedPresetConfig(c, 30.0f), PresetConfigError::TooFast);
}

TEST(GameSpeedConfig, FastestExactlyOneFramePerDayIsRejected)
{
    // wallSecondsPerDay == 1/hz advances exactly one day per frame: the bound is strict.
    SpeedPresetConfig c{ { 60.0f, 30.0f, 10.0f, 0.5f } };
    EXPECT_EQ(ValidateSpeedPresetConfig(c, 2.0f), PresetConfigError::TooFast);
    c.wallSecondsPerDay[3] = 0.51f;
    EXPECT_EQ(ValidateSpeedPresetConfig(c, 2.0f), PresetConfigError::None);
}

TEST(GameSpeedConfig, ValidationDependsOnSimHz)
{
    SpeedPresetConfig c{ { 60.0f, 30.0f, 10.0f, 0.05f } };
    EXPECT_EQ(ValidateSpeedPresetConfig(c, 30.0f), PresetConfigError::None);
    EXPECT_EQ(ValidateSpeedPresetConfig(c, 60.0f), PresetConfigError::None);
    EXPECT_EQ(ValidateSpeedPresetConfig(c, 20.0f), PresetConfigError::TooFast);   // 0.05 == 1/20
    EXPECT_EQ(ValidateSpeedPresetConfig(c, 120.0f), PresetConfigError::None);
}

TEST(GameSpeedConfig, RejectsBadSimHz)
{
    EXPECT_EQ(ValidateSpeedPresetConfig(Cfg(), 0.0f), PresetConfigError::InvalidSimHz);
    EXPECT_EQ(ValidateSpeedPresetConfig(Cfg(), -30.0f), PresetConfigError::InvalidSimHz);
    EXPECT_EQ(ValidateSpeedPresetConfig(Cfg(), std::numeric_limits<float>::quiet_NaN()), PresetConfigError::InvalidSimHz);
    EXPECT_EQ(ValidateSpeedPresetConfig(Cfg(), std::numeric_limits<float>::infinity()), PresetConfigError::InvalidSimHz);
}

TEST(GameSpeedConfig, ErrorNames)
{
    EXPECT_STREQ(PresetConfigErrorName(PresetConfigError::None), "None");
    EXPECT_STREQ(PresetConfigErrorName(PresetConfigError::TooFast), "TooFast");
    EXPECT_STREQ(PresetConfigErrorName(PresetConfigError::NotDecreasing), "NotDecreasing");
    EXPECT_STREQ(PresetConfigErrorName(PresetConfigError::NonPositive), "NonPositive");
    EXPECT_STREQ(PresetConfigErrorName(PresetConfigError::InvalidSimHz), "InvalidSimHz");
}

// ---------------------------------------------------------------------------
// Initial state / time scale
// ---------------------------------------------------------------------------
TEST(GameSpeedController, StartsPausedAtDayZero)
{
    GameSpeedController c = Make();
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().dayCounter, 0u);
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Paused);
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Player);
    EXPECT_EQ(c.GetState().modalDepth, 0);
    EXPECT_EQ(c.GetTimeScale(), 0.0f);
}

TEST(GameSpeedController, TimeScaleMapsEachPreset)
{
    GameSpeedController c = Make();
    const SpeedTier tiers[4] = { SpeedTier::Slow, SpeedTier::Normal, SpeedTier::Fast, SpeedTier::Fastest };
    for (int i = 0; i < 4; ++i)
    {
        c.OnSpeedChangeRequested(tiers[i]);
        EXPECT_FALSE(c.IsPaused());
        EXPECT_FLOAT_EQ(c.GetTimeScale(), 1.0f / Cfg().wallSecondsPerDay[i]) << i;
    }
}

TEST(GameSpeedController, TimeScaleStrictlyIncreasesWithTier)
{
    GameSpeedController c = Make();
    float prev = 0.0f;
    for (SpeedTier t : { SpeedTier::Slow, SpeedTier::Normal, SpeedTier::Fast, SpeedTier::Fastest })
    {
        c.OnSpeedChangeRequested(t);
        EXPECT_GT(c.GetTimeScale(), prev);
        prev = c.GetTimeScale();
    }
}

TEST(GameSpeedController, CustomConfigDrivesScale)
{
    GameSpeedController c(SpeedPresetConfig{ { 8.0f, 4.0f, 2.0f, 0.5f } });
    c.OnSpeedChangeRequested(SpeedTier::Fastest);
    EXPECT_FLOAT_EQ(c.GetTimeScale(), 2.0f);
    c.OnSpeedChangeRequested(SpeedTier::Slow);
    EXPECT_FLOAT_EQ(c.GetTimeScale(), 0.125f);
}

// ---------------------------------------------------------------------------
// Speed changes
// ---------------------------------------------------------------------------
TEST(GameSpeedController, SelectingSpeedResumesFromPlayerPause)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Normal);
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Normal);
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::None);
}

TEST(GameSpeedController, PauseRequestSetsPlayerPause)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    c.OnSpeedChangeRequested(SpeedTier::Paused);
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Player);
    EXPECT_EQ(c.GetTimeScale(), 0.0f);
}

TEST(GameSpeedController, SameTierAgainIsNoOp_EC04)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    const GameSpeedState before = c.GetState();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    EXPECT_EQ(c.GetState().speedTier, before.speedTier);
    EXPECT_EQ(c.GetState().pauseSource, before.pauseSource);
    EXPECT_EQ(c.GetState().dayCounter, before.dayCounter);
    EXPECT_EQ(c.GetState().modalDepth, before.modalDepth);
}

TEST(GameSpeedController, PauseWhilePausedIsNoOp)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Paused);
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Player);
}

TEST(GameSpeedController, InvalidTierIgnored)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    c.OnSpeedChangeRequested(static_cast<SpeedTier>(42));
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Fast);
}

TEST(GameSpeedController, SwitchBetweenTiersKeepsDayCounter)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Slow);
    c.AdvanceDay(); c.AdvanceDay();
    c.OnSpeedChangeRequested(SpeedTier::Fastest);
    EXPECT_EQ(c.GetState().dayCounter, 2u);
}

// ---------------------------------------------------------------------------
// Manual step
// ---------------------------------------------------------------------------
TEST(GameSpeedController, ManualAdvanceAcceptedWhilePaused)
{
    GameSpeedController c = Make();
    EXPECT_TRUE(c.OnManualAdvanceRequested());
}

TEST(GameSpeedController, ManualAdvanceRejectedWhileRunning_EC06)
{
    GameSpeedController c = Make();
    for (SpeedTier t : { SpeedTier::Slow, SpeedTier::Normal, SpeedTier::Fast, SpeedTier::Fastest })
    {
        c.OnSpeedChangeRequested(t);
        EXPECT_FALSE(c.OnManualAdvanceRequested());
    }
}

TEST(GameSpeedController, ManualAdvanceAcceptedWhileModalPaused)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Normal);
    c.OnModalOpened();
    EXPECT_TRUE(c.OnManualAdvanceRequested());
}

TEST(GameSpeedController, ManualAdvanceDoesNotChangeState)
{
    GameSpeedController c = Make();
    c.OnManualAdvanceRequested();
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().dayCounter, 0u);
}

// ---------------------------------------------------------------------------
// Modal pause
// ---------------------------------------------------------------------------
TEST(GameSpeedController, ModalPausesRunningGameAndRestoresOnClose)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    c.OnModalOpened();
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Modal);
    EXPECT_EQ(c.GetState().modalDepth, 1);
    c.OnModalClosed();
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Fast);
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::None);
    EXPECT_EQ(c.GetState().modalDepth, 0);
}

TEST(GameSpeedController, PlayerPauseSurvivesModal_EC01)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Normal);
    c.OnSpeedChangeRequested(SpeedTier::Paused);
    c.OnModalOpened();
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Player);
    c.OnModalClosed();
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Player);
}

TEST(GameSpeedController, NestedModals_EC02)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    c.OnModalOpened();
    c.OnModalOpened();
    EXPECT_EQ(c.GetState().modalDepth, 2);
    c.OnModalClosed();
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().modalDepth, 1);
    c.OnModalClosed();
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Fast);
}

TEST(GameSpeedController, NestedModalDoesNotOverwriteSavedSpeed)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Slow);
    c.OnModalOpened();
    c.OnModalOpened();
    c.OnModalClosed();
    c.OnModalClosed();
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Slow);
}

TEST(GameSpeedController, ExtraModalCloseClampsAtZero)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Normal);
    c.OnModalClosed();
    c.OnModalClosed();
    EXPECT_EQ(c.GetState().modalDepth, 0);
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Normal);
    c.OnModalOpened();
    c.OnModalClosed();
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Normal);
}

TEST(GameSpeedController, ModalDepthSaturates)
{
    GameSpeedController c = Make();
    for (int i = 0; i < 400; ++i) c.OnModalOpened();
    EXPECT_EQ(c.GetState().modalDepth, 255);
}

TEST(GameSpeedController, SpeedRequestDuringModalUpdatesRestoredSpeed)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Slow);
    c.OnModalOpened();
    c.OnSpeedChangeRequested(SpeedTier::Fastest);
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetTimeScale(), 0.0f);
    c.OnModalClosed();
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Fastest);
}

TEST(GameSpeedController, PauseRequestDuringModalBecomesPlayerPause)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Normal);
    c.OnModalOpened();
    c.OnSpeedChangeRequested(SpeedTier::Paused);
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Player);
    c.OnModalClosed();
    EXPECT_TRUE(c.IsPaused());
}

TEST(GameSpeedController, SpeedPickDuringModalAfterPlayerPauseResumesOnClose)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Normal);
    c.OnSpeedChangeRequested(SpeedTier::Paused);
    c.OnModalOpened();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    EXPECT_TRUE(c.IsPaused());
    c.OnModalClosed();
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Fast);
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::None);
}

TEST(GameSpeedController, LastActionWinsDuringModal)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Normal);
    c.OnModalOpened();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    c.OnSpeedChangeRequested(SpeedTier::Paused);
    c.OnModalClosed();
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Player);
}

TEST(GameSpeedController, ReselectAfterModalClosed)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Normal);
    c.OnModalOpened();
    c.OnModalClosed();
    c.OnSpeedChangeRequested(SpeedTier::Slow);
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Slow);
}

TEST(GameSpeedController, ModalOpenedWhilePausedDoesNotClobberSavedSpeed)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    c.OnSpeedChangeRequested(SpeedTier::Paused);
    c.OnModalOpened();
    c.OnModalClosed();
    c.OnSpeedChangeRequested(SpeedTier::Slow);
    c.OnModalOpened();
    c.OnModalClosed();
    EXPECT_EQ(c.GetState().speedTier, SpeedTier::Slow);
}

// ---------------------------------------------------------------------------
// Day counter / New Game / Restore
// ---------------------------------------------------------------------------
TEST(GameSpeedController, AdvanceDayIncrementsAndReturnsNewDay)
{
    GameSpeedController c = Make();
    EXPECT_EQ(c.AdvanceDay(), 1u);
    EXPECT_EQ(c.AdvanceDay(), 2u);
    EXPECT_EQ(c.GetState().dayCounter, 2u);
}

TEST(GameSpeedController, NewGameResetsEverything)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    c.AdvanceDay();
    c.OnModalOpened();
    c.OnNewGame();
    EXPECT_EQ(c.GetState().dayCounter, 0u);
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Player);
    EXPECT_EQ(c.GetState().modalDepth, 0);
}

TEST(GameSpeedController, NewGameThenModalCloseDoesNotResume)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    c.OnModalOpened();
    c.OnNewGame();
    c.OnModalClosed();
    EXPECT_TRUE(c.IsPaused());
}

TEST(GameSpeedController, RestoreAlwaysComesBackPaused_EC03)
{
    GameSpeedController c = Make();
    GameSpeedState saved;
    saved.dayCounter = 1234;
    saved.speedTier = SpeedTier::Fastest;
    saved.pauseSource = PauseSource::None;
    saved.modalDepth = 3;
    saved.preModalSpeedTier = SpeedTier::Fast;
    c.RestoreState(saved);
    EXPECT_EQ(c.GetState().dayCounter, 1234u);
    EXPECT_TRUE(c.IsPaused());
    EXPECT_EQ(c.GetState().pauseSource, PauseSource::Player);
    EXPECT_EQ(c.GetState().modalDepth, 0);
    EXPECT_EQ(c.GetTimeScale(), 0.0f);
}

TEST(GameSpeedController, RestoredGameCanResume)
{
    GameSpeedController c = Make();
    GameSpeedState saved; saved.dayCounter = 50;
    c.RestoreState(saved);
    c.OnSpeedChangeRequested(SpeedTier::Normal);
    EXPECT_FALSE(c.IsPaused());
    EXPECT_EQ(c.AdvanceDay(), 51u);
}

TEST(GameSpeedController, RestoreMidModalLeavesNoModalResume)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Fast);
    c.OnModalOpened();
    GameSpeedState saved; saved.dayCounter = 9;
    c.RestoreState(saved);
    c.OnModalClosed();
    EXPECT_TRUE(c.IsPaused());
}

TEST(GameSpeedController, DayCounterNeverDecreasesThroughPauseAndModal)
{
    GameSpeedController c = Make();
    c.OnSpeedChangeRequested(SpeedTier::Normal);
    uint64_t last = 0;
    for (int i = 0; i < 100; ++i)
    {
        if (i % 7 == 0) c.OnModalOpened();
        if (i % 11 == 0) c.OnSpeedChangeRequested(SpeedTier::Paused);
        if (i % 5 == 0) c.OnModalClosed();
        const uint64_t d = c.AdvanceDay();
        EXPECT_GT(d, last);
        last = d;
    }
}

// ---------------------------------------------------------------------------
// DayClock
// ---------------------------------------------------------------------------
TEST(DayClock, FirstAdvanceOnlyBaselines)
{
    DayClock clock;
    EXPECT_FALSE(clock.Advance(5 * kGameMicrosecondsPerDay));
    EXPECT_EQ(clock.GetAccumulatedUs(), 0);
}

TEST(DayClock, NoTickWhilePaused)
{
    DayClock clock;
    clock.Advance(0);
    for (int i = 0; i < 1000; ++i)
        EXPECT_FALSE(clock.Advance(0));
}

TEST(DayClock, TicksExactlyAtBoundary)
{
    DayClock clock;
    clock.Advance(0);
    EXPECT_FALSE(clock.Advance(kGameMicrosecondsPerDay - 1));
    EXPECT_TRUE(clock.Advance(kGameMicrosecondsPerDay));
    EXPECT_EQ(clock.GetAccumulatedUs(), 0);
}

TEST(DayClock, OvershootCarriesIntoNextDay)
{
    DayClock clock;
    clock.Advance(0);
    EXPECT_TRUE(clock.Advance(1300000));
    EXPECT_EQ(clock.GetAccumulatedUs(), 300000);
    EXPECT_FALSE(clock.Advance(1900000));
    EXPECT_TRUE(clock.Advance(2000000));
}

TEST(DayClock, AtMostOneTickPerAdvanceAndNoDayLost)
{
    DayClock clock;
    clock.Advance(0);
    EXPECT_TRUE(clock.Advance(3500000));   // 3.5 days in one frame: one tick now
    EXPECT_EQ(clock.CarryDays(), 2);
    EXPECT_TRUE(clock.Advance(3500000));   // carry drains one per frame
    EXPECT_TRUE(clock.Advance(3500000));
    EXPECT_FALSE(clock.Advance(3500000));
    EXPECT_EQ(clock.GetAccumulatedUs(), 500000);
}

TEST(DayClock, TickCountEqualsElapsedDaysAtEverySpeed)
{
    const double hz = 30.0;
    for (double wallPerDay : { 60.0, 30.0, 10.0, 1.0, 0.05 })
    {
        DayClock clock;
        clock.Advance(0);
        const double usPerFrame = 1e6 / hz / wallPerDay;
        const int64_t target = 10 * kGameMicrosecondsPerDay;
        double t = 0.0;
        int ticks = 0;
        while (static_cast<int64_t>(t) < target)
        {
            t += usPerFrame;
            const int64_t now = static_cast<int64_t>(t) > target ? target : static_cast<int64_t>(t);
            if (clock.Advance(now)) ++ticks;
        }
        EXPECT_EQ(ticks, 10) << "wallSecondsPerDay=" << wallPerDay;
    }
}

TEST(DayClock, SpeedChangeMidDayLosesNoDay)
{
    DayClock clock;
    clock.Advance(0);
    int ticks = 0;
    int64_t t = 0, total = 0;
    const int64_t steps[] = { 100000, 100000, 100000, 100000, 450000, 450000, 450000, 450000,
                              100000, 100000, 100000, 100000, 100000, 100000 };
    for (int64_t s : steps) { t += s; total += s; if (clock.Advance(t)) ++ticks; }
    while (total < 3 * kGameMicrosecondsPerDay) { t += 500000; total += 500000; if (clock.Advance(t)) ++ticks; }
    EXPECT_EQ(ticks, static_cast<int>(total / kGameMicrosecondsPerDay));
}

TEST(DayClock, ResetRebaselines)
{
    DayClock clock;
    clock.Advance(0);
    clock.Advance(700000);
    clock.Reset();
    EXPECT_EQ(clock.GetAccumulatedUs(), 0);
    EXPECT_FALSE(clock.Advance(50 * kGameMicrosecondsPerDay));
    EXPECT_FALSE(clock.Advance(50 * kGameMicrosecondsPerDay + 999999));
    EXPECT_TRUE(clock.Advance(51 * kGameMicrosecondsPerDay));
}

TEST(DayClock, BackwardsTimeIsIgnoredAndCounted)
{
    DayClock clock;
    clock.Advance(1000000);
    clock.Advance(1500000);
    EXPECT_FALSE(clock.Advance(1200000));   // went backwards
    EXPECT_EQ(clock.GetBackwardsCount(), 1u);
    EXPECT_EQ(clock.GetAccumulatedUs(), 500000);
    EXPECT_TRUE(clock.Advance(1700000));    // 0.5 + 0.5 from the new baseline
}

TEST(DayClock, RemainingUsIsTimeToNextBoundary)
{
    DayClock clock;
    clock.Advance(0);
    EXPECT_EQ(clock.RemainingUs(), kGameMicrosecondsPerDay);
    clock.Advance(250000);
    EXPECT_EQ(clock.RemainingUs(), 750000);
}

TEST(DayClock, StepByRemainingProducesExactlyOneTick)
{
    DayClock clock;
    clock.Advance(0);
    clock.Advance(250000);
    const int64_t step = clock.RemainingUs();
    EXPECT_TRUE(clock.Advance(250000 + step));
    EXPECT_EQ(clock.GetAccumulatedUs(), 0);
    EXPECT_EQ(clock.RemainingUs(), kGameMicrosecondsPerDay);
    EXPECT_FALSE(clock.Advance(250000 + step));
}

TEST(DayClock, RemainingUsWithCarryFallsBackToFullDay)
{
    DayClock clock;
    clock.Advance(0);
    clock.Advance(2500000);   // one tick, 1.5 days carried
    EXPECT_EQ(clock.RemainingUs(), kGameMicrosecondsPerDay);
}

// ---------------------------------------------------------------------------
// Determinism: the day sequence is identical at every speed
// ---------------------------------------------------------------------------
namespace {
struct RunResult { uint64_t days; TimeBand lastBand; uint64_t bandChecksum; };

RunResult RunDays(float wallSecondsPerDay, uint64_t targetDays)
{
    GameSpeedController c(SpeedPresetConfig{ { 600.0f, 300.0f, 200.0f, wallSecondsPerDay } });
    c.OnSpeedChangeRequested(SpeedTier::Fastest);
    DayClock clock;
    clock.Advance(0);
    double timeUs = 0.0;
    RunResult r{ 0, TimeBand::Day, 0 };
    while (c.GetState().dayCounter < targetDays)
    {
        timeUs += 1e6 / 30.0 * c.GetTimeScale();
        if (clock.Advance(static_cast<int64_t>(timeUs)))
        {
            const uint64_t day = c.AdvanceDay();
            r.lastBand = ResolveBand(day);
            r.bandChecksum = r.bandChecksum * 31 + static_cast<uint64_t>(r.lastBand) + day;
        }
    }
    r.days = c.GetState().dayCounter;
    return r;
}
}

TEST(GameSpeedDeterminism, SameDaySequenceAtSlowAndFastest)
{
    const RunResult fast = RunDays(0.05f, 800);
    const RunResult slow = RunDays(5.0f, 800);
    EXPECT_EQ(fast.days, 800u);
    EXPECT_EQ(slow.days, 800u);
    EXPECT_EQ(fast.bandChecksum, slow.bandChecksum);
    EXPECT_EQ(fast.lastBand, slow.lastBand);
}

// ---------------------------------------------------------------------------
// Full-century band census
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
