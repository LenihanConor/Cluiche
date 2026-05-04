////////////////////////////////////////////////////////////////////////////////
// Filename: DebugLayerManager.h
// Description: Central registry for debug draw classes (IVisualDebugger).
//              Sorts by priority, calls enabled layers each frame, exposes
//              global debug scale, and registers DiaAPI commands.
// System spec: docs/specs/systems/dia/diavisualdebugger.md
// Feature spec: docs/specs/features/dia/diavisualdebugger/debug-layer-manager.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "IVisualDebugger.h"

namespace Dia
{
    namespace Graphics
    {
        class FrameData;
    }

    namespace DebugServer
    {
        class DebugServerModule;  // Forward declaration — no header included; caller passes nullptr if absent
    }
}

namespace Dia
{
    namespace Debug
    {
        ////////////////////////////////////////////////////////////////////////////////
        // DebugLayerManager
        //
        // Central registry for all IVisualDebugger instances.
        //
        // Lifetime requirement for RegisterDiaAPICommands():
        //   The manager instance MUST outlive the application. Command lambdas capture
        //   `this` — if the manager is destroyed before commands are unregistered, the
        //   lambda becomes a dangling capture. RegisterDiaAPICommands() should be called
        //   once during application startup (e.g. from a Module::DoStart()).
        ////////////////////////////////////////////////////////////////////////////////
        class DebugLayerManager
        {
        public:
            static const unsigned int kMaxLayers = 64;

            // ----------------------------------------------------------------
            // Registration
            // ----------------------------------------------------------------

            // Register a draw class. DIA_ASSERT fires if debugger is null or if the
            // layer name is already registered (SD-DBG-006).
            // priority: lower value drawn first (underneath); higher value drawn last (on top).
            void Register(IVisualDebugger* debugger, int priority = 0);

            // Unregister a layer by name. No-op if the name is not registered.
            void Unregister(Dia::Core::StringCRC layerName);

            // ----------------------------------------------------------------
            // Layer toggle
            // ----------------------------------------------------------------

            void EnableLayer (Dia::Core::StringCRC layerName);
            void DisableLayer(Dia::Core::StringCRC layerName);
            bool IsLayerEnabled(Dia::Core::StringCRC layerName) const;

            // ----------------------------------------------------------------
            // Global debug scale (SD-DBG-005)
            // Draw classes read this before submitting size/length values.
            // ----------------------------------------------------------------
            void  SetDebugScale(float scale);
            float GetDebugScale() const;

            // ----------------------------------------------------------------
            // Picking seam — no-op stubs until scene editor (SD-DBG-008)
            // ----------------------------------------------------------------
            void     SetSelectedEntityId(uint32_t id);
            uint32_t GetSelectedEntityId() const;

            // ----------------------------------------------------------------
            // Draw
            // ----------------------------------------------------------------

            // Call once per frame after simulation update, before rendering.
            // Lazily sorts by priority if dirty, then calls Draw() on each enabled layer.
            void Draw(Dia::Graphics::FrameData& frameData);

            // ----------------------------------------------------------------
            // DiaAPI commands (call once during application startup)
            // ----------------------------------------------------------------

            // Registers five commands: debug.layer.enable, debug.layer.disable,
            // debug.layer.list, debug.scale, debug.pick (no-op stub).
            // LIFETIME: manager must outlive the application (see class comment).
            void RegisterDiaAPICommands();

            // ----------------------------------------------------------------
            // Editor broadcast (optional)
            // ----------------------------------------------------------------

            // Broadcast current layer state to DiaDebugServer subscribers.
            // No-op if debugServer is nullptr (DiaDebugServer is an optional dependency).
            // Full implementation deferred to feature debug-editor-panel (feature 10).
            void BroadcastLayerState(Dia::DebugServer::DebugServerModule* debugServer);

            // ----------------------------------------------------------------
            // Query
            // ----------------------------------------------------------------

            int  GetLayerCount() const;
            bool HasLayer(Dia::Core::StringCRC layerName) const;

            // Returns the layer name at position index (0-based).
            // Used by editor console to enumerate registered layers.
            // Returns StringCRC::kZero if index is out of range.
            Dia::Core::StringCRC GetLayerName(int index) const;

        private:
            struct LayerEntry
            {
                IVisualDebugger* debugger = nullptr;
                int              priority = 0;
            };

            Dia::Core::Containers::DynamicArrayC<LayerEntry, kMaxLayers> mLayers;
            float    mDebugScale       = 1.0f;
            uint32_t mSelectedEntityId = 0;
            bool     mSortDirty        = false;

            // Insertion sort — stable, O(N²) acceptable for kMaxLayers = 64
            void SortByPriority();

            // Returns the index of the layer with the given name, or -1 if not found.
            int FindLayerIndex(Dia::Core::StringCRC layerName) const;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
