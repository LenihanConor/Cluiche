////////////////////////////////////////////////////////////////////////////////
// Filename: DebugColourPalette.h
// Description: Semantic colour constants for all Dia debug draw classes.
// All draw classes MUST use these constants — do not hardcode colours.
// System spec: docs/specs/systems/dia/diavisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Colour/RGBA.h>

namespace Dia
{
    namespace Debug
    {
        ////////////////////////////////////////////////////////////////////////////////
        // DebugColourPalette
        //
        // 9 RGBA constants with semantic meanings.
        // Binding decision SD-DBG-010: all draw classes must use this palette.
        ////////////////////////////////////////////////////////////////////////////////
        struct DebugColourPalette
        {
            static const Dia::Core::RGBA kActive;     ///< white     (255,255,255,255) — dynamic/active
            static const Dia::Core::RGBA kInactive;   ///< grey      (128,128,128,255) — static/sleeping/inactive
            static const Dia::Core::RGBA kHealthy;    ///< green     (0,220,0,255)     — converged/solved/ok
            static const Dia::Core::RGBA kWarning;    ///< yellow    (255,220,0,255)   — best-effort/warning
            static const Dia::Core::RGBA kError;      ///< red       (220,0,0,255)     — failed/torn/error
            static const Dia::Core::RGBA kGoal;       ///< cyan      (0,220,220,255)   — target/goal position
            static const Dia::Core::RGBA kPinned;     ///< magenta   (220,0,220,255)   — pinned/constrained
            static const Dia::Core::RGBA kCapped;     ///< orange    (255,140,0,255)   — capped/limit-hit
            static const Dia::Core::RGBA kDeepSleep;  ///< dark blue (0,0,80,255)      — deep sleep

            static const Dia::Core::RGBA kGridCellPassable;   ///< green, semi-transparent — passable grid cell fill
            static const Dia::Core::RGBA kGridCellImpassable; ///< red, semi-transparent   — impassable grid cell fill
        };

    } // namespace Debug
} // namespace Dia
