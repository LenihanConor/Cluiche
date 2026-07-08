////////////////////////////////////////////////////////////////////////////////
// Filename: IDebugContext.h
// Description: Neutral debug context contract. Provides per-frame scalars and
//              selection state that domain visual debuggers need without
//              depending on DebugLayerManager.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <stdint.h>

namespace Dia
{
    namespace Core
    {
        ////////////////////////////////////////////////////////////////////////
        // IDebugContext
        //
        // Read-only view of DebugLayerManager state needed by domain drawers.
        // Implementors: Dia::Debug::DebugLayerManager.
        ////////////////////////////////////////////////////////////////////////
        class IDebugContext
        {
        public:
            virtual ~IDebugContext() = default;

            // World-space scale factor for debug geometry (e.g. circle radii, arrow lengths).
            virtual float GetDebugScale() const = 0;

            // Entity ID currently selected in the inspector (0 = none).
            virtual uint32_t GetSelectedEntityId() const = 0;

            // Set selected entity (used by picking drawers).
            virtual void SetSelectedEntityId(uint32_t id) = 0;
        };

    } // namespace Core
} // namespace Dia

#endif // DIA_DEBUG
