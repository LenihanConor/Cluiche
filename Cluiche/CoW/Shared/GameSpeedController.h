// GameSpeedController core: calendar, speed presets, pause/modal state machine,
// and day-boundary accumulator. Pure logic with no Dia module dependencies, so it
// is header-only and unit-tested directly from GoogleTests.
// Spec: docs/specs/applications/cow/systems/gamespeedcontroller/gamespeedcontroller.md
#pragma once

#include <cstdint>
#include <cmath>

namespace CoW {

// ---------------------------------------------------------------------------
// Calendar (fixed constants, not tunable)
// ---------------------------------------------------------------------------
constexpr uint64_t kDaysPerWeek       = 7;
constexpr uint64_t kWeeksPerSeason    = 13;
constexpr uint64_t kSeasonsPerYear    = 4;
constexpr uint64_t kYearsPerDecade    = 10;
constexpr uint64_t kDecadesPerCentury = 10;

constexpr uint64_t kDaysPerSeason  = kDaysPerWeek * kWeeksPerSeason;      // 91
constexpr uint64_t kDaysPerYear    = kDaysPerSeason * kSeasonsPerYear;    // 364
constexpr uint64_t kDaysPerDecade  = kDaysPerYear * kYearsPerDecade;      // 3,640
constexpr uint64_t kDaysPerCentury = kDaysPerDecade * kDecadesPerCentury; // 36,400

// 1 game-second = 1 day, so a SimTimeScheduler deadline of +7.0s means "in one week".
constexpr int64_t kGameMicrosecondsPerDay = 1000000;

// Ordered smallest -> largest so subscribers can gate with `band >= TimeBand::Season`.
enum class TimeBand : uint8_t { Day, Week, Season, Year, Decade, Century };

// Largest unit whose boundary this day completes. Day 0 is never ticked (the first
// tick is day 1) but resolves to Century for completeness.
inline TimeBand ResolveBand(uint64_t day)
{
    if (day % kDaysPerCentury == 0) return TimeBand::Century;
    if (day % kDaysPerDecade  == 0) return TimeBand::Decade;
    if (day % kDaysPerYear    == 0) return TimeBand::Year;
    if (day % kDaysPerSeason  == 0) return TimeBand::Season;
    if (day % kDaysPerWeek    == 0) return TimeBand::Week;
    return TimeBand::Day;
}

inline uint64_t WeekOf(uint64_t day)   { return day / kDaysPerWeek; }
inline uint64_t SeasonOf(uint64_t day) { return day / kDaysPerSeason; }
inline uint64_t YearOf(uint64_t day)   { return day / kDaysPerYear; }

inline const char* TimeBandName(TimeBand band)
{
    switch (band)
    {
    case TimeBand::Day:     return "Day";
    case TimeBand::Week:    return "Week";
    case TimeBand::Season:  return "Season";
    case TimeBand::Year:    return "Year";
    case TimeBand::Decade:  return "Decade";
    case TimeBand::Century: return "Century";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// Speed presets
// ---------------------------------------------------------------------------
enum class SpeedTier : uint8_t { Paused, Slow, Normal, Fast, Fastest };

inline const char* SpeedTierName(SpeedTier tier)
{
    switch (tier)
    {
    case SpeedTier::Paused:  return "Paused";
    case SpeedTier::Slow:    return "Slow";
    case SpeedTier::Normal:  return "Normal";
    case SpeedTier::Fast:    return "Fast";
    case SpeedTier::Fastest: return "Fastest";
    }
    return "Unknown";
}

inline bool IsValidSpeedTier(SpeedTier tier) { return static_cast<uint8_t>(tier) <= static_cast<uint8_t>(SpeedTier::Fastest); }

struct SpeedPresetConfig
{
    float wallSecondsPerDay[4];   // Slow, Normal, Fast, Fastest
};

// Placeholders, tuned in playtesting. Match the values in cow_main.diaapp.
inline SpeedPresetConfig DefaultSpeedPresetConfig()
{
    return SpeedPresetConfig{ { 60.0f, 30.0f, 10.0f, 1.0f } };
}

enum class PresetConfigError : uint8_t
{
    None,
    InvalidSimHz,    // simHz <= 0 or not finite
    NonPositive,     // a value is <= 0, NaN or infinite
    NotDecreasing,   // not strictly decreasing Slow -> Fastest
    TooFast          // Fastest would advance >= 1 day per sim frame
};

inline const char* PresetConfigErrorName(PresetConfigError error)
{
    switch (error)
    {
    case PresetConfigError::None:          return "None";
    case PresetConfigError::InvalidSimHz:  return "InvalidSimHz";
    case PresetConfigError::NonPositive:   return "NonPositive";
    case PresetConfigError::NotDecreasing: return "NotDecreasing";
    case PresetConfigError::TooFast:       return "TooFast";
    }
    return "Unknown";
}

// Every value > 0, strictly decreasing Slow -> Fastest, and Fastest satisfies the
// one-tick-per-frame rule: wallSecondsPerDay > 1 / simHz.
inline PresetConfigError ValidateSpeedPresetConfig(const SpeedPresetConfig& config, float simHz)
{
    if (!(simHz > 0.0f) || !std::isfinite(simHz))
        return PresetConfigError::InvalidSimHz;

    for (float v : config.wallSecondsPerDay)
    {
        if (!(v > 0.0f) || !std::isfinite(v))
            return PresetConfigError::NonPositive;
    }
    for (int i = 1; i < 4; ++i)
    {
        if (!(config.wallSecondsPerDay[i] < config.wallSecondsPerDay[i - 1]))
            return PresetConfigError::NotDecreasing;
    }
    if (!(config.wallSecondsPerDay[3] > 1.0f / simHz))
        return PresetConfigError::TooFast;

    return PresetConfigError::None;
}

// ---------------------------------------------------------------------------
// Pause state
// ---------------------------------------------------------------------------
enum class PauseSource : uint8_t { None, Player, Modal };

struct GameSpeedState
{
    uint64_t    dayCounter        = 0;                    // 0 at New Game, +1 per tick, never decreases
    SpeedTier   speedTier         = SpeedTier::Paused;
    PauseSource pauseSource       = PauseSource::Player;
    uint8_t     modalDepth        = 0;                    // nesting-safe count of open modals
    SpeedTier   preModalSpeedTier = SpeedTier::Normal;    // restored when modalDepth returns to 0
};

class GameSpeedController
{
public:
    explicit GameSpeedController(const SpeedPresetConfig& config)
        : mConfig(config)
    {}

    void OnSpeedChangeRequested(SpeedTier tier)
    {
        if (!IsValidSpeedTier(tier))
            return;

        if (mState.modalDepth > 0)
        {
            // Stay paused while a modal is open; remember what to resume to.
            if (tier == SpeedTier::Paused)
            {
                mState.pauseSource = PauseSource::Player;
            }
            else
            {
                mState.preModalSpeedTier = tier;
                // Selecting a speed is a resume: closing the last modal restores it,
                // even if the player had paused before the modal opened.
                mState.pauseSource = PauseSource::Modal;
            }
            return;
        }

        if (tier == SpeedTier::Paused)
        {
            mState.speedTier   = SpeedTier::Paused;
            mState.pauseSource = PauseSource::Player;
        }
        else
        {
            mState.speedTier   = tier;
            mState.pauseSource = PauseSource::None;
        }
    }

    // False if rejected (not paused). The caller steps the clock only on true.
    bool OnManualAdvanceRequested() const
    {
        return mState.speedTier == SpeedTier::Paused;
    }

    void OnModalOpened()
    {
        if (mState.modalDepth == 0 && mState.speedTier != SpeedTier::Paused)
        {
            mState.preModalSpeedTier = mState.speedTier;
            mState.speedTier         = SpeedTier::Paused;
            mState.pauseSource       = PauseSource::Modal;
        }
        if (mState.modalDepth < 255)
            ++mState.modalDepth;
    }

    void OnModalClosed()
    {
        if (mState.modalDepth > 0)
            --mState.modalDepth;

        if (mState.modalDepth == 0 && mState.pauseSource == PauseSource::Modal)
        {
            mState.speedTier   = mState.preModalSpeedTier;
            mState.pauseSource = PauseSource::None;
        }
        // A Player pause is never resumed by a modal closing (EC-01).
    }

    void OnNewGame()
    {
        mState = GameSpeedState{};
    }

    // Called by the module each time the game clock crosses a day boundary.
    uint64_t AdvanceDay()
    {
        return ++mState.dayCounter;
    }

    void RestoreState(const GameSpeedState& saved)
    {
        mState = GameSpeedState{};
        mState.dayCounter = saved.dayCounter;   // always comes back paused (EC-03)
        // Persisted since save v2: what a later modal-close would resume to. Everything else
        // (tier, pause source, modal depth) stays at the paused defaults.
        if (IsValidSpeedTier(saved.preModalSpeedTier) && saved.preModalSpeedTier != SpeedTier::Paused)
            mState.preModalSpeedTier = saved.preModalSpeedTier;
    }

    // Game-seconds (== days) per wall-second; 0 when paused.
    float GetTimeScale() const
    {
        if (mState.speedTier == SpeedTier::Paused)
            return 0.0f;
        return 1.0f / mConfig.wallSecondsPerDay[static_cast<int>(mState.speedTier) - 1];
    }

    bool IsPaused() const { return mState.speedTier == SpeedTier::Paused; }
    const GameSpeedState& GetState() const { return mState; }
    const SpeedPresetConfig& GetConfig() const { return mConfig; }

private:
    SpeedPresetConfig mConfig;
    GameSpeedState    mState;
};

// ---------------------------------------------------------------------------
// Day-boundary accumulator
// ---------------------------------------------------------------------------
// Fed the world game time every sim frame, reports whether a day boundary was
// crossed. Reports at most ONE day per Advance() call; any excess stays in the
// accumulator (carry) so no day is ever skipped, only delayed.
class DayClock
{
public:
    // Returns true when a day boundary was crossed this frame.
    bool Advance(int64_t gameTimeUs)
    {
        if (!mHasPrevious)
        {
            mPreviousUs  = gameTimeUs;
            mHasPrevious = true;
            return false;
        }

        int64_t delta = gameTimeUs - mPreviousUs;
        mPreviousUs   = gameTimeUs;
        if (delta < 0)
        {
            ++mBackwardsCount;
            delta = 0;
        }
        mAccumulatedUs += delta;

        if (mAccumulatedUs >= kGameMicrosecondsPerDay)
        {
            mAccumulatedUs -= kGameMicrosecondsPerDay;
            return true;
        }
        return false;
    }

    // Forget history (New Game / Restore): next Advance() only re-baselines.
    void Reset()
    {
        mHasPrevious   = false;
        mPreviousUs    = 0;
        mAccumulatedUs = 0;
    }

    // Restore sub-day progress (save v2). Clamped to [0, one day); the baseline is re-established by the
    // next Advance() (call after Reset()).
    void SetAccumulatedUs(int64_t us)
    {
        mAccumulatedUs = (us > 0 && us < kGameMicrosecondsPerDay) ? us : 0;
    }

    // Game microseconds until the next day boundary (what a manual step must advance).
    int64_t RemainingUs() const
    {
        const int64_t remaining = kGameMicrosecondsPerDay - mAccumulatedUs;
        return remaining > 0 ? remaining : kGameMicrosecondsPerDay;
    }

    // Whole days of lag still queued behind the one-tick-per-frame cap.
    int64_t CarryDays() const { return mAccumulatedUs / kGameMicrosecondsPerDay; }

    int64_t  GetAccumulatedUs() const { return mAccumulatedUs; }
    uint32_t GetBackwardsCount() const { return mBackwardsCount; }

private:
    bool     mHasPrevious    = false;
    int64_t  mPreviousUs     = 0;
    int64_t  mAccumulatedUs  = 0;
    uint32_t mBackwardsCount = 0;
};

} // namespace CoW
