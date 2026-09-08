////////////////////////////////////////////////////////////////////////////////
// Filename: IVisualDebugger.h
// Description: Interface for all debug draw classes registered with DebugLayerManager
// System spec: docs/specs/systems/dia/diavisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <atomic>

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
        // mEnabled is atomic so SimPU drawers can be toggled safely from MainPU console.
        ////////////////////////////////////////////////////////////////////////////////
        class IVisualDebugger
        {
        public:
            virtual ~IVisualDebugger() = default;

            // Returns the canonical StringCRC layer name (see DebugLayerNames.h).
            virtual Dia::Core::StringCRC GetLayerName() const = 0;

            // Called each frame by DebugLayerManager::Draw() if this layer is enabled.
            // draw: IDebugDraw interface for submitting debug primitives.
            virtual void Draw(Dia::Core::IDebugDraw& draw) = 0;

            // Enable/disable this layer. Atomic so MainPU console can toggle SimPU drawers.
            virtual void SetEnabled(bool enabled) { mEnabled.store(enabled, std::memory_order_relaxed); }
            virtual bool IsEnabled() const        { return mEnabled.load(std::memory_order_relaxed); }

        private:
            std::atomic<bool> mEnabled{true};
        };

    } // namespace Debug
} // namespace Dia
