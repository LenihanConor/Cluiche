////////////////////////////////////////////////////////////////////////////////
// Filename: DebugGroupAccents.h
// Description: Canonical accent colour constants for each debug domain group.
//              IDebugDomain::GetAccentColour() MUST return one of these —
//              never an inline literal.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaCore/Colour/RGBA.h>

namespace Dia
{
    namespace VisualDebugger
    {
        ////////////////////////////////////////////////////////////////////////////////
        // DebugGroupAccents
        //
        // 8 RGBA accent constants — one per canonical debug group.
        // Binding decision: all IDebugDomain implementations use these constants.
        ////////////////////////////////////////////////////////////////////////////////
        struct DebugGroupAccents
        {
            static const Dia::Core::RGBA kCoreDebug;   ///< grey-500   (#6b7280) — core / utility domains
            static const Dia::Core::RGBA kPhysics;     ///< amber-400  (#f59e0b) — physics simulation
            static const Dia::Core::RGBA kAnimation;   ///< emerald-500 (#10b981) — animation systems
            static const Dia::Core::RGBA kNavigation;  ///< blue-500   (#3b82f6) — pathfinding / steering
            static const Dia::Core::RGBA kRendering;   ///< violet-500 (#8b5cf6) — rendering / GPU
            static const Dia::Core::RGBA kSpatial;     ///< cyan-500   (#06b6d4) — spatial / geometry
            static const Dia::Core::RGBA kEntity;      ///< yellow-500 (#eab308) — entity / component
            static const Dia::Core::RGBA kAIBehavior;  ///< red-500    (#ef4444) — AI / behaviour trees
        };

    } // namespace VisualDebugger
} // namespace Dia

#endif // DIA_DEBUG
