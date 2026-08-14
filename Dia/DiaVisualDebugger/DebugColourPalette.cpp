////////////////////////////////////////////////////////////////////////////////
// Filename: DebugColourPalette.cpp
// Description: RGBA constant definitions for DebugColourPalette
////////////////////////////////////////////////////////////////////////////////
#include "DebugColourPalette.h"

namespace Dia
{
    namespace Debug
    {
        // white     — dynamic/active
        const Dia::Core::RGBA DebugColourPalette::kActive    (255, 255, 255, 255);

        // grey      — static/sleeping/inactive
        const Dia::Core::RGBA DebugColourPalette::kInactive  (128, 128, 128, 255);

        // green     — converged/solved/ok
        const Dia::Core::RGBA DebugColourPalette::kHealthy   (  0, 220,   0, 255);

        // yellow    — best-effort/warning
        const Dia::Core::RGBA DebugColourPalette::kWarning   (255, 220,   0, 255);

        // red       — failed/torn/error
        const Dia::Core::RGBA DebugColourPalette::kError     (220,   0,   0, 255);

        // cyan      — target/goal position
        const Dia::Core::RGBA DebugColourPalette::kGoal      (  0, 220, 220, 255);

        // magenta   — pinned/constrained
        const Dia::Core::RGBA DebugColourPalette::kPinned    (220,   0, 220, 255);

        // orange    — capped/limit-hit
        const Dia::Core::RGBA DebugColourPalette::kCapped    (255, 140,   0, 255);

        // dark blue — deep sleep
        const Dia::Core::RGBA DebugColourPalette::kDeepSleep (  0,   0,  80, 255);

        // green, semi-transparent — passable grid cell fill
        const Dia::Core::RGBA DebugColourPalette::kGridCellPassable   (  0, 180,   0,  60);

        // red, semi-transparent — impassable grid cell fill
        const Dia::Core::RGBA DebugColourPalette::kGridCellImpassable (180,   0,   0,  60);

        // sky blue, opaque — flow field direction arrow
        const Dia::Core::RGBA DebugColourPalette::kFlowDirection      (100, 200, 255, 255);

    } // namespace Debug
} // namespace Dia
