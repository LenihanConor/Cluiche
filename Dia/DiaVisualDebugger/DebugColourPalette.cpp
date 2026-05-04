////////////////////////////////////////////////////////////////////////////////
// Filename: DebugColourPalette.cpp
// Description: RGBA constant definitions for DebugColourPalette
////////////////////////////////////////////////////////////////////////////////
#include "DebugColourPalette.h"

#ifdef DIA_DEBUG

namespace Dia
{
    namespace Debug
    {
        // white     — dynamic/active
        const Dia::Graphics::RGBA DebugColourPalette::kActive    (255, 255, 255, 255);

        // grey      — static/sleeping/inactive
        const Dia::Graphics::RGBA DebugColourPalette::kInactive  (128, 128, 128, 255);

        // green     — converged/solved/ok
        const Dia::Graphics::RGBA DebugColourPalette::kHealthy   (  0, 220,   0, 255);

        // yellow    — best-effort/warning
        const Dia::Graphics::RGBA DebugColourPalette::kWarning   (255, 220,   0, 255);

        // red       — failed/torn/error
        const Dia::Graphics::RGBA DebugColourPalette::kError     (220,   0,   0, 255);

        // cyan      — target/goal position
        const Dia::Graphics::RGBA DebugColourPalette::kGoal      (  0, 220, 220, 255);

        // magenta   — pinned/constrained
        const Dia::Graphics::RGBA DebugColourPalette::kPinned    (220,   0, 220, 255);

        // orange    — capped/limit-hit
        const Dia::Graphics::RGBA DebugColourPalette::kCapped    (255, 140,   0, 255);

        // dark blue — deep sleep
        const Dia::Graphics::RGBA DebugColourPalette::kDeepSleep (  0,   0,  80, 255);

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
