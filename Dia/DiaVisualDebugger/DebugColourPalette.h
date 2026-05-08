////////////////////////////////////////////////////////////////////////////////
// Filename: DebugColourPalette.h
// Description: Semantic colour constants for all Dia debug draw classes.
// All draw classes MUST use these constants — do not hardcode colours.
// System spec: docs/specs/systems/dia/diavisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaGraphics/Misc/RGBA.h>

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
            static const Dia::Graphics::RGBA kActive;     ///< white     (255,255,255,255) — dynamic/active
            static const Dia::Graphics::RGBA kInactive;   ///< grey      (128,128,128,255) — static/sleeping/inactive
            static const Dia::Graphics::RGBA kHealthy;    ///< green     (0,220,0,255)     — converged/solved/ok
            static const Dia::Graphics::RGBA kWarning;    ///< yellow    (255,220,0,255)   — best-effort/warning
            static const Dia::Graphics::RGBA kError;      ///< red       (220,0,0,255)     — failed/torn/error
            static const Dia::Graphics::RGBA kGoal;       ///< cyan      (0,220,220,255)   — target/goal position
            static const Dia::Graphics::RGBA kPinned;     ///< magenta   (220,0,220,255)   — pinned/constrained
            static const Dia::Graphics::RGBA kCapped;     ///< orange    (255,140,0,255)   — capped/limit-hit
            static const Dia::Graphics::RGBA kDeepSleep;  ///< dark blue (0,0,80,255)      — deep sleep
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
