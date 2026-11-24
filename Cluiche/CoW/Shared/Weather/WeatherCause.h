// Failing / not-ready cause codes of the weather stage (WorldStepStageBase::CauseCode is uint32_t).
#pragma once

#include <stdint.h>

namespace CoW {
namespace Weather {

enum class CauseCode : uint32_t
{
    kNone = 0,
    kNoWaterSource,      // EC-01: the world has no water-source cell
    kParameterFault,     // the parameter sidecar failed to load or bind
    kInitialiseFailed,   // WeatherSimulation::Initialise refused the terrain
    kCount
};

inline const char* CauseText(CauseCode code)
{
    switch (code)
    {
    case CauseCode::kNone:             return "none";
    case CauseCode::kNoWaterSource:    return "no_water_source";
    case CauseCode::kParameterFault:   return "parameter_fault";
    case CauseCode::kInitialiseFailed: return "initialise_failed";
    default:                           return "unknown";
    }
}

// Degraded (not Failing) causes reported through the stage's health reporter.
constexpr const char* kCauseBaselineClamped = "baseline_clamped";     // a baseline provider value was outside [0, 1]
constexpr const char* kCauseFallbackBaseline = "fallback_baseline";   // no baseline provider: the sim uses its table

// Adapter for WorldStepStageBase::CauseTextFn (takes the raw uint32_t).
inline const char* CauseTextRaw(uint32_t code) { return CauseText(static_cast<CauseCode>(code)); }

} // namespace Weather
} // namespace CoW
