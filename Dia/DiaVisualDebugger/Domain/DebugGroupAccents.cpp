////////////////////////////////////////////////////////////////////////////////
// Filename: DebugGroupAccents.cpp
// Description: RGBA constant definitions for DebugGroupAccents
////////////////////////////////////////////////////////////////////////////////
#include "DebugGroupAccents.h"

#ifdef DIA_DEBUG

namespace Dia
{
    namespace VisualDebugger
    {
        // grey-500   — core / utility domains
        const Dia::Core::RGBA DebugGroupAccents::kCoreDebug  (107, 114, 128, 255);

        // amber-400  — physics simulation
        const Dia::Core::RGBA DebugGroupAccents::kPhysics    (245, 158,  11, 255);

        // emerald-500 — animation systems
        const Dia::Core::RGBA DebugGroupAccents::kAnimation  ( 16, 185, 129, 255);

        // blue-500   — pathfinding / steering
        const Dia::Core::RGBA DebugGroupAccents::kNavigation ( 59, 130, 246, 255);

        // violet-500 — rendering / GPU
        const Dia::Core::RGBA DebugGroupAccents::kRendering  (139,  92, 246, 255);

        // cyan-500   — spatial / geometry
        const Dia::Core::RGBA DebugGroupAccents::kSpatial    (  6, 182, 212, 255);

        // yellow-500 — entity / component
        const Dia::Core::RGBA DebugGroupAccents::kEntity     (234, 179,   8, 255);

        // red-500    — AI / behaviour trees
        const Dia::Core::RGBA DebugGroupAccents::kAIBehavior (239,  68,  68, 255);

    } // namespace VisualDebugger
} // namespace Dia

#endif // DIA_DEBUG
