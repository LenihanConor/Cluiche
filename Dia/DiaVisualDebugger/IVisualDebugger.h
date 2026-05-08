////////////////////////////////////////////////////////////////////////////////
// Filename: IVisualDebugger.h
// Description: Interface for all debug draw classes registered with DebugLayerManager
// System spec: docs/specs/systems/dia/diavisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace Graphics
    {
        class FrameData;
    }
}

namespace Dia
{
    namespace Debug
    {
        ////////////////////////////////////////////////////////////////////////////////
        // IVisualDebugger
        //
        // Base interface for all debug draw classes.
        // Implementations must provide GetLayerName() and Draw(FrameData&).
        // SetEnabled/IsEnabled have default implementations — override only when
        // more complex enable semantics are needed (e.g. suppress bones when rig layer active).
        ////////////////////////////////////////////////////////////////////////////////
        class IVisualDebugger
        {
        public:
            virtual ~IVisualDebugger() = default;

            // Returns the canonical StringCRC layer name (see DebugLayerNames.h).
            virtual Dia::Core::StringCRC GetLayerName() const = 0;

            // Called each frame by DebugLayerManager::Draw() if this layer is enabled.
            virtual void Draw(Dia::Graphics::FrameData& frameData) = 0;

            // Enable/disable this layer. Default implementation stores a bool flag.
            virtual void SetEnabled(bool enabled) { mEnabled = enabled; }
            virtual bool IsEnabled() const        { return mEnabled; }

        private:
            bool mEnabled = true;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
