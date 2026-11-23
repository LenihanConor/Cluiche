// CoW's calendar and TimeBand: the content layer over the generic, data-driven Calendar in
// GameSpeedController.h. The values are CoW data (also carried in cow_main.diaapp, "calendar" block);
// CoWCalendar() is the default used when config supplies none.
// Spec: docs/specs/features/draoi/extract-draoigamespeed.md (Decisions 1-2)
#pragma once

#include <Shared/GameSpeedController.h>

namespace CoW {

constexpr uint64_t kDaysPerWeek       = 7;
constexpr uint64_t kWeeksPerSeason    = 13;
constexpr uint64_t kSeasonsPerYear    = 4;
constexpr uint64_t kYearsPerDecade    = 10;
constexpr uint64_t kDecadesPerCentury = 10;

constexpr uint64_t kDaysPerSeason  = kDaysPerWeek * kWeeksPerSeason;      // 91
constexpr uint64_t kDaysPerYear    = kDaysPerSeason * kSeasonsPerYear;    // 364
constexpr uint64_t kDaysPerDecade  = kDaysPerYear * kYearsPerDecade;      // 3,640
constexpr uint64_t kDaysPerCentury = kDaysPerDecade * kDecadesPerCentury; // 36,400

// Ordered smallest -> largest so subscribers can gate with `band >= TimeBand::Season`.
// An alias over Calendar band indices (0 = plain day) for CoW's CoWCalendar() band ladder.
enum class TimeBand : uint8_t { Day, Week, Season, Year, Decade, Century };

// 7-day week, 13-week season, 4-season (364-day) year, 10-year decade, 10-decade century.
inline Calendar CoWCalendar()
{
    Calendar cal;
    cal.periods   = { { "Week", kDaysPerWeek }, { "Season", kWeeksPerSeason }, { "Year", kSeasonsPerYear },
                      { "Decade", kYearsPerDecade }, { "Century", kDecadesPerCentury } };
    cal.bandNames = { "Day", "Week", "Season", "Year", "Decade", "Century" };
    return cal;
}

inline const Calendar& DefaultCoWCalendar()
{
    static const Calendar kCalendar = CoWCalendar();
    return kCalendar;
}

// Thin CoW wrappers over the default CoW calendar (rung 0 = week, 1 = season, 2 = year).
inline TimeBand ResolveBand(uint64_t day)  { return static_cast<TimeBand>(DefaultCoWCalendar().BandOf(day)); }
inline uint64_t WeekOf(uint64_t day)       { return DefaultCoWCalendar().PeriodOf(0, day); }
inline uint64_t SeasonOf(uint64_t day)     { return DefaultCoWCalendar().PeriodOf(1, day); }
inline uint64_t YearOf(uint64_t day)       { return DefaultCoWCalendar().PeriodOf(2, day); }

inline const char* TimeBandName(TimeBand band)
{
    return DefaultCoWCalendar().BandName(static_cast<uint8_t>(band));
}

} // namespace CoW
