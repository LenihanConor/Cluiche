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
#include <DiaDebugDraw/Domain/IDebugLayerRegistry.h>
#include <DiaGraphics/Camera/Camera2D.h>
#include <DiaGraphics/Camera/ViewportTransform.h>
#include <DiaGraphics3D/Camera3D.h>
#include <DiaMaths/Vector/Vector2D.h>
#include "IVisualDebugger.h"
#include "IObjectRenderer.h"
#include "FixedDrawRegistry.h"

namespace Dia
{
    namespace Graphics
    {
        class DebugFrameDataVisitor;
    }

    namespace DebugServer
    {
        class DebugServer;  // Forward declaration — no header included; caller passes nullptr if absent
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
        class DebugLayerManager : public IDebugLayerRegistry
        {
        public:
            static const unsigned int kMaxLayers = 128;

            // ----------------------------------------------------------------
            // Registration (dynamic layers)
            // ----------------------------------------------------------------

            // Register a draw class. DIA_ASSERT fires if debugger is null or if the
            // layer name is already registered in either registry (SD-DBG-006).
            // priority: lower value drawn first (underneath); higher value drawn last (on top).
            void Register(IVisualDebugger* debugger, int priority = 0) override;

            // Register a draw class with a stage tag. Same layer name + same pointer = idempotent
            // (reactivates the layer). Same name + different pointer = DIA_ASSERT (SD-DBG-006).
            void Register(IVisualDebugger* debugger, int priority, const Dia::Core::StringCRC& stageTag) override;

            // Like Register(), but Draw() will never call IVisualDebugger::Draw() on this entry.
            // Use when a drawer's Draw() targets a different frame type than what this manager
            // provides (e.g. a 3D drawer registered for console/ImGui access only).
            void RegisterWithoutDraw(IVisualDebugger* debugger, int priority, const Dia::Core::StringCRC& stageTag);

            // Unregister a layer by name. No-op if the name is not registered.
            void Unregister(Dia::Core::StringCRC layerName) override;

            // Remove all dynamic layers at once (e.g. on module stop before drawers are freed).
            // Does not touch the fixed registry.
            void ClearDynamicLayers();

            // ----------------------------------------------------------------
            // Registration (fixed layers)
            // ----------------------------------------------------------------

            // Lock registration — after this call, RegisterFixed() will DIA_ASSERT.
            // Call once during application startup, after all fixed layers are registered.
            void LockRegistration();

            // Register a fixed-topology object. DIA_ASSERT if locked, or if name already
            // registered in either registry (SD-DBG-006).
            // capacity: max primitives for this object's buffer.
            // priority: lower value drawn first.
            void RegisterFixed(Dia::Core::StringCRC name,
                               const void*          sourceObject,
                               IObjectRenderer*     renderer,
                               unsigned int         capacity,
                               int                  priority = 0);

            void UnregisterFixed(Dia::Core::StringCRC name);

            // Mark a fixed layer dirty — next DrawFixed() will rebuild its buffer.
            void InvalidateFixed(Dia::Core::StringCRC name);

            // ----------------------------------------------------------------
            // Layer toggle
            // ----------------------------------------------------------------

            // Routes through both dynamic and fixed registries.
            void EnableLayer (Dia::Core::StringCRC layerName) override;
            void DisableLayer(Dia::Core::StringCRC layerName) override;
            bool IsLayerEnabled(Dia::Core::StringCRC layerName) const override;

            // Activate or deactivate all layers owned by stageTag.
            // Layers with an empty stageTag are unaffected.
            void SetStageActive(const Dia::Core::StringCRC& stageTag, bool active);

            // ----------------------------------------------------------------
            // Global debug scale (SD-DBG-005)
            // Draw classes read this before submitting size/length values.
            // ----------------------------------------------------------------
            void  SetDebugScale(float scale) override;
            float GetDebugScale() const override;

            // ----------------------------------------------------------------
            // Viewport / coordinate transform (used by coord2d overlay layers)
            // ----------------------------------------------------------------
            void SetViewport(const Dia::Graphics::Camera2D& camera, const Dia::Maths::Vector2D& windowSize);
            Dia::Graphics::ViewportTransform GetViewportTransform() const;

            // ----------------------------------------------------------------
            // 3D camera — stored for use by Coord3D overlay drawers
            // ----------------------------------------------------------------
            void SetCamera3D(const Dia::Graphics3D::Camera3D& camera);
            const Dia::Graphics3D::Camera3D& GetCamera3D() const;

            // ----------------------------------------------------------------
            // 2D cursor world position — updated by Coord2DCursorDrawer each frame
            // ----------------------------------------------------------------
            void SetCursorWorld(const Dia::Maths::Vector2D& worldPos);
            Dia::Maths::Vector2D GetCursorWorld() const;

            // ----------------------------------------------------------------
            // Picking seam — no-op stubs until scene editor (SD-DBG-008)
            // ----------------------------------------------------------------
            void     SetSelectedEntityId(uint32_t id) override;
            uint32_t GetSelectedEntityId() const override;

            // ----------------------------------------------------------------
            // Draw
            // ----------------------------------------------------------------

            // Call once per frame after simulation update, before rendering.
            // Lazily sorts by priority if dirty, then calls Draw() on each enabled layer.
            void Draw(Dia::Core::IDebugDraw& draw);

            // Renders all enabled fixed layers into visitor.
            // Call from render loop after Draw().
            void DrawFixed(const Dia::Graphics::DebugFrameDataVisitor& visitor);

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
            // Only broadcasts when mLayersDirty is true (set on Register/Enable/Disable/Unregister).
            // Clears mLayersDirty after broadcast.
            void BroadcastLayerState(Dia::DebugServer::DebugServer* debugServer);

            // ----------------------------------------------------------------
            // Query
            // ----------------------------------------------------------------

            // Returns dynamic count + fixed count.
            int  GetLayerCount() const;
            // Returns true if name is registered in either dynamic or fixed registry.
            bool HasLayer(Dia::Core::StringCRC layerName) const;

            // Returns the layer name at position index (0-based).
            // Used by editor console to enumerate registered layers.
            // Returns StringCRC::kZero if index is out of range.
            Dia::Core::StringCRC GetLayerName(int index) const;

            // Returns the IVisualDebugger at position index (0-based), or nullptr.
            IVisualDebugger* GetLayer(int index) const;

            // Returns the stage tag for the dynamic layer at position index (0-based).
            // Returns StringCRC::kZero (empty) if index is out of range or layer is global.
            Dia::Core::StringCRC GetLayerStageTag(int index) const;

            // Returns true if any layer with this stageTag is currently active.
            bool IsStageActive(const Dia::Core::StringCRC& stageTag) const;

            // Fills `out` with unique stage tags from all registered layers (excluding empty tag).
            // Uses DynamicArrayC — caller provides the buffer.
            void GetStageTags(Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& out) const;

        private:
            struct LayerEntry
            {
                IVisualDebugger*     debugger  = nullptr;
                int                  priority  = 0;
                Dia::Core::StringCRC stageTag;           // empty = always active (global layer)
                bool                 active    = true;   // false = skip Draw, show grayed in console
                bool                 skipDraw  = false;  // true = skip Draw() but keep ImGui/toggle
            };

            Dia::Core::Containers::DynamicArrayC<LayerEntry, kMaxLayers> mLayers;
            FixedDrawRegistry mFixedRegistry;
            float    mDebugScale          = 1.0f;
            uint32_t mSelectedEntityId    = 0;
            bool     mSortDirty           = false;
            bool     mRegistrationLocked  = false;

            // Viewport state — stored as inputs; ViewportTransform constructed on demand
            Dia::Graphics::Camera2D      mViewportCamera;
            Dia::Maths::Vector2D         mViewportWindowSize;

            // 3D camera — stored for Coord3D overlay drawers
            Dia::Graphics3D::Camera3D    mCamera3D;

            // 2D cursor world position — updated by Coord2DCursorDrawer each Draw() call
            Dia::Maths::Vector2D         mCursorWorld;

            // Broadcast state tracking (debug-editor-panel)
            uint32_t mLastDroppedCount = 0;  // cached from FrameData at end of Draw()
            bool     mLayersDirty      = false;  // set on Register/Unregister/Enable/Disable
            bool     mAPICommandsRegistered = false;

            // Insertion sort — stable, O(N²) acceptable for kMaxLayers = 128
            void SortByPriority();

            // Returns the index of the layer with the given name, or -1 if not found.
            int FindLayerIndex(Dia::Core::StringCRC layerName) const;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
