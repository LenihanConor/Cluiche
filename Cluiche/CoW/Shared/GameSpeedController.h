// GameSpeedController core: calendar, speed presets, pause/modal state machine,
// and day-boundary accumulator. Pure logic with no Dia module dependencies, so it
// is header-only and unit-tested directly from GoogleTests.
// Spec: docs/specs/applications/cow/systems/gamespeedcontroller/gamespeedcontroller.md
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace CoW {

// ---------------------------------------------------------------------------
// Calendar (data, supplied by the application)
// ---------------------------------------------------------------------------
// 1 game-second = 1 day, so a SimTimeScheduler deadline of +7.0s means "in one week" (CoW calendar).
constexpr int64_t kGameMicrosecondsPerDay = 1000000;

// One rung of the period ladder: a named period whose length is `multiple` times the previous rung
// (the first rung is a multiple of one day).
struct CalendarPeriod
{
    std::string name;
    uint64_t    multiple = 1;
};

// An ordered ladder of named periods plus a ladder of named bands over the day. Band 0 is a plain day;
// band i (i >= 1) is "this day completes a period of rung i-1". No periods = no calendar: every tick is
// a plain day (band 0). Names and lengths are application data; nothing here is game specific.
struct Calendar
{
    std::vector<CalendarPeriod> periods;     // smallest -> largest
    std::vector<std::string>    bandNames;   // index = band; size 0 or periods.size() + 1

    // Valid when every multiple is >= 1, the cumulative length does not overflow and the band ladder
    // is empty or has at most periods.size() + 1 names.
    bool IsValid() const
    {
        uint64_t length = 1;
        for (const CalendarPeriod& p : periods)
        {
            if (p.multiple == 0 || length > UINT64_MAX / p.multiple)
                return false;
            length *= p.multiple;
        }
        return bandNames.size() <= periods.size() + 1;
    }

    // Length of rung `rung` in days (cumulative product of the multiples up to and including it).
    // 0 if the rung does not exist.
    uint64_t PeriodLengthDays(size_t rung) const
    {
        if (rung >= periods.size())
            return 0;
        uint64_t length = 1;
        for (size_t i = 0; i <= rung; ++i)
            length *= periods[i].multiple;
        return length;
    }

    // Index of the period of rung `rung` that `day` falls in (day / length). A missing rung has no
    // period structure, so the day itself is returned.
    uint64_t PeriodOf(size_t rung, uint64_t day) const
    {
        const uint64_t length = PeriodLengthDays(rung);
        return (length == 0) ? day : day / length;
    }

    // Band = index of the highest rung whose period length divides `day`, plus one; 0 = plain day.
    // Day 0 divides every rung, so it resolves to the top band (never ticked, resolved for completeness).
    uint8_t BandOf(uint64_t day) const
    {
        for (size_t i = periods.size(); i > 0; --i)
        {
            const uint64_t length = PeriodLengthDays(i - 1);
            if (length != 0 && day % length == 0)
                return static_cast<uint8_t>(i);
        }
        return 0;
    }

    const char* BandName(uint8_t band) const
    {
        return band < bandNames.size() ? bandNames[band].c_str() : "Unknown";
    }
};

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
    explicit GameSpeedController(const SpeedPresetConfig& config, Calendar calendar = Calendar{})
        : mConfig(config)
        , mCalendar(std::move(calendar))
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
    const Calendar& GetCalendar() const { return mCalendar; }

private:
    SpeedPresetConfig mConfig;
    Calendar          mCalendar;      // application data, not part of the saved state
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
