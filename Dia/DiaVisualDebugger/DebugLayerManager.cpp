////////////////////////////////////////////////////////////////////////////////
// Filename: DebugLayerManager.cpp
// Description: Implementation of DebugLayerManager
////////////////////////////////////////////////////////////////////////////////
#include "DebugLayerManager.h"

#ifdef DIA_DEBUG

#include <DiaCore/Core/Assert.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaGraphics/Camera/Camera2D.h>
#include <DiaGraphics/Camera/ViewportTransform.h>
#include <DiaGraphics3D/Camera3D.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoJsonCodec.h>
#include <cstdlib>  // std::atof

namespace Dia
{
    namespace Debug
    {
        // --------------------------------------------------------------------
        // Registration
        // --------------------------------------------------------------------

        void DebugLayerManager::Register(IVisualDebugger* debugger, int priority)
        {
            Register(debugger, priority, Dia::Core::StringCRC());
        }

        void DebugLayerManager::Register(IVisualDebugger* debugger, int priority,
                                          const Dia::Core::StringCRC& stageTag)
        {
            DIA_ASSERT(debugger != nullptr, "DebugLayerManager::Register — debugger must not be null");

            // Idempotency: same name + same pointer → just reactivate
            int existing = FindLayerIndex(debugger->GetLayerName());
            if (existing >= 0)
            {
                if (mLayers[static_cast<unsigned int>(existing)].debugger == debugger)
                {
                    mLayers[static_cast<unsigned int>(existing)].active = true;
                    mLayersDirty = true;
                    return;
                }
                DIA_ASSERT(false, "DebugLayerManager::Register — layer name already registered with different pointer");
                return;
            }

            DIA_ASSERT(!mFixedRegistry.HasLayer(debugger->GetLayerName()),
                       "DebugLayerManager::Register — layer name already registered in fixed registry");

            LayerEntry entry;
            entry.debugger  = debugger;
            entry.priority  = priority;
            entry.stageTag  = stageTag;
            entry.active    = true;
            mLayers.Add(entry);
            mSortDirty   = true;
            mLayersDirty = true;
        }

        void DebugLayerManager::RegisterWithoutDraw(IVisualDebugger* debugger, int priority,
                                                     const Dia::Core::StringCRC& stageTag)
        {
            Register(debugger, priority, stageTag);
            int index = FindLayerIndex(debugger->GetLayerName());
            if (index >= 0)
                mLayers[static_cast<unsigned int>(index)].skipDraw = true;
        }

        void DebugLayerManager::ClearDynamicLayers()
        {
            mLayers.RemoveAll();
            mSortDirty   = true;
            mLayersDirty = true;
        }

        void DebugLayerManager::Unregister(Dia::Core::StringCRC layerName)
        {
            int index = FindLayerIndex(layerName);
            if (index < 0)
                return;

            // Remove by index (swap-with-last pattern; order re-sorted on next Draw)
            unsigned int ui = static_cast<unsigned int>(index);
            mLayers.RemoveAt(ui);
            mSortDirty  = true;
            mLayersDirty = true;
        }

        // --------------------------------------------------------------------
        // Layer toggle
        // --------------------------------------------------------------------

        void DebugLayerManager::EnableLayer(Dia::Core::StringCRC layerName)
        {
            int index = FindLayerIndex(layerName);
            if (index >= 0)
            {
                mLayers[static_cast<unsigned int>(index)].debugger->SetEnabled(true);
                mLayersDirty = true;
                return;
            }
            if (mFixedRegistry.HasLayer(layerName))
            {
                mFixedRegistry.EnableLayer(layerName);
                mLayersDirty = true;
            }
        }

        void DebugLayerManager::DisableLayer(Dia::Core::StringCRC layerName)
        {
            int index = FindLayerIndex(layerName);
            if (index >= 0)
            {
                mLayers[static_cast<unsigned int>(index)].debugger->SetEnabled(false);
                mLayersDirty = true;
                return;
            }
            if (mFixedRegistry.HasLayer(layerName))
            {
                mFixedRegistry.DisableLayer(layerName);
                mLayersDirty = true;
            }
        }

        bool DebugLayerManager::IsLayerEnabled(Dia::Core::StringCRC layerName) const
        {
            int index = FindLayerIndex(layerName);
            if (index >= 0)
                return mLayers[static_cast<unsigned int>(index)].debugger->IsEnabled();
            return mFixedRegistry.IsLayerEnabled(layerName);
        }

        void DebugLayerManager::SetStageActive(const Dia::Core::StringCRC& stageTag, bool active)
        {
            if (stageTag == Dia::Core::StringCRC())
                return;  // empty tag = global layer, never deactivated
            for (unsigned int i = 0; i < mLayers.Size(); ++i)
            {
                if (mLayers[i].stageTag == stageTag)
                {
                    mLayers[i].active = active;
                    mLayersDirty = true;
                }
            }
        }

        // --------------------------------------------------------------------
        // Global debug scale
        // --------------------------------------------------------------------

        void DebugLayerManager::SetDebugScale(float scale)
        {
            mDebugScale = scale;
        }

        float DebugLayerManager::GetDebugScale() const
        {
            return mDebugScale;
        }

        // --------------------------------------------------------------------
        // Viewport / coordinate transform
        // --------------------------------------------------------------------

        void DebugLayerManager::SetViewport(const Dia::Graphics::Camera2D& camera,
                                             const Dia::Maths::Vector2D& windowSize)
        {
            mViewportCamera     = camera;
            mViewportWindowSize = windowSize;
        }

        Dia::Graphics::ViewportTransform DebugLayerManager::GetViewportTransform() const
        {
            return Dia::Graphics::ViewportTransform(mViewportCamera, mViewportWindowSize);
        }

        // --------------------------------------------------------------------
        // 3D camera
        // --------------------------------------------------------------------

        void DebugLayerManager::SetCamera3D(const Dia::Graphics3D::Camera3D& camera)
        {
            mCamera3D = camera;
        }

        const Dia::Graphics3D::Camera3D& DebugLayerManager::GetCamera3D() const
        {
            return mCamera3D;
        }

        // --------------------------------------------------------------------
        // Picking seam stubs
        // --------------------------------------------------------------------

        void DebugLayerManager::SetSelectedEntityId(uint32_t id)
        {
            mSelectedEntityId = id;
        }

        uint32_t DebugLayerManager::GetSelectedEntityId() const
        {
            return mSelectedEntityId;
        }

        // --------------------------------------------------------------------
        // Draw
        // --------------------------------------------------------------------

        void DebugLayerManager::Draw(Dia::Core::IDebugDraw& draw)
        {
            if (mSortDirty)
                SortByPriority();

            for (unsigned int i = 0; i < mLayers.Size(); ++i)
            {
                if (!mLayers[i].active)
                    continue;
                if (mLayers[i].skipDraw)
                    continue;
                IVisualDebugger* d = mLayers[i].debugger;
                if (d->IsEnabled())
                    d->Draw(draw);
            }

            // Cache dropped count — IDebugDraw exposes DroppedCount
            const uint32_t droppedNow = draw.DroppedCount();
            if (droppedNow != mLastDroppedCount)
            {
                mLastDroppedCount = droppedNow;
                mLayersDirty = true;
            }
        }

        // --------------------------------------------------------------------
        // DiaAPI command registration
        // LIFETIME: manager must outlive the application (lambdas capture this).
        // --------------------------------------------------------------------

        void DebugLayerManager::RegisterDiaAPICommands()
        {
            // debug.layer.enable <layer-name>
            Dia::API::RegisterCommand({
                Dia::Core::StringCRC("debug.layer.enable"),
                "Enable a named debug layer",
                Dia::Core::StringCRC("debug"),
                "DiaVisualDebugger", "1.0.0",
                "debug.layer.enable physics.shapes",
                [this](const Dia::API::CommandArgs& args) -> int {
                    if (args.positionalArgs.Size() < 1) return 2;
                    EnableLayer(Dia::Core::StringCRC(args.positionalArgs[0]));
                    return 0;
                }
            });

            // debug.layer.disable <layer-name>
            Dia::API::RegisterCommand({
                Dia::Core::StringCRC("debug.layer.disable"),
                "Disable a named debug layer",
                Dia::Core::StringCRC("debug"),
                "DiaVisualDebugger", "1.0.0",
                "debug.layer.disable physics.shapes",
                [this](const Dia::API::CommandArgs& args) -> int {
                    if (args.positionalArgs.Size() < 1) return 2;
                    DisableLayer(Dia::Core::StringCRC(args.positionalArgs[0]));
                    return 0;
                }
            });

            // debug.layer.list — returns 0; used by editor to trigger BroadcastLayerState
            Dia::API::RegisterCommand({
                Dia::Core::StringCRC("debug.layer.list"),
                "List all registered debug layers and their enabled state",
                Dia::Core::StringCRC("debug"),
                "DiaVisualDebugger", "1.0.0",
                "debug.layer.list",
                [this](const Dia::API::CommandArgs&) -> int {
                    (void)this;
                    return 0;
                }
            });

            // debug.scale <float>
            // std::atof used internally — acceptable per AI review Q3 (not in public API)
            Dia::API::RegisterCommand({
                Dia::Core::StringCRC("debug.scale"),
                "Set global debug draw scale factor",
                Dia::Core::StringCRC("debug"),
                "DiaVisualDebugger", "1.0.0",
                "debug.scale 2.0",
                [this](const Dia::API::CommandArgs& args) -> int {
                    if (args.positionalArgs.Size() < 1) return 2;
                    SetDebugScale(static_cast<float>(std::atof(args.positionalArgs[0])));
                    return 0;
                }
            });

            // debug.pick — no-op stub; picking seam for scene editor (SD-DBG-008)
            Dia::API::RegisterCommand({
                Dia::Core::StringCRC("debug.pick"),
                "Pick entity at screen coordinates (scene editor seam — not yet implemented)",
                Dia::Core::StringCRC("debug"),
                "DiaVisualDebugger", "1.0.0",
                "debug.pick 320 240",
                [](const Dia::API::CommandArgs&) -> int { return 0; }
            });
        }

        // --------------------------------------------------------------------
        // BroadcastLayerState
        // Serialises the current layer list + dropped count to a DATA_UPDATE
        // envelope and broadcasts to all subscribers of "debug.layer.state".
        // Only broadcasts when mLayersDirty is true; clears the flag after send.
        // --------------------------------------------------------------------

        void DebugLayerManager::BroadcastLayerState(
            Dia::DebugServer::DebugServer* debugServer)
        {
            if (debugServer == nullptr) return;
            if (!mLayersDirty) return;

            Json::Value payload;
            payload["droppedCount"] = mLastDroppedCount;

            Json::Value layers(Json::arrayValue);
            for (unsigned int i = 0; i < mLayers.Size(); ++i)
            {
                Json::Value entry;
                entry["name"]     = mLayers[i].debugger->GetLayerName().AsChar();
                entry["enabled"]  = mLayers[i].debugger->IsEnabled();
                entry["priority"] = mLayers[i].priority;
                entry["stage"]    = mLayers[i].stageTag.AsChar();   // may be "" for global layers
                entry["active"]   = mLayers[i].active;
                layers.append(entry);
            }
            payload["layers"] = layers;

            debugServer->NotifySubscribers(Dia::Core::StringCRC("debug.layer.state"), payload);
            mLayersDirty = false;
        }

        // --------------------------------------------------------------------
        // Fixed-layer registration
        // --------------------------------------------------------------------

        void DebugLayerManager::LockRegistration()
        {
            mRegistrationLocked = true;
        }

        void DebugLayerManager::RegisterFixed(
            Dia::Core::StringCRC name,
            const void*          sourceObject,
            IObjectRenderer*     renderer,
            unsigned int         capacity,
            int                  priority)
        {
            DIA_ASSERT(!mRegistrationLocked,
                       "DebugLayerManager::RegisterFixed — called after LockRegistration");
            DIA_ASSERT(FindLayerIndex(name) < 0,
                       "DebugLayerManager::RegisterFixed — name already registered in dynamic registry");
            mFixedRegistry.Register(name, sourceObject, renderer, capacity, priority);
        }

        void DebugLayerManager::UnregisterFixed(Dia::Core::StringCRC name)
        {
            mFixedRegistry.Unregister(name);
        }

        void DebugLayerManager::InvalidateFixed(Dia::Core::StringCRC name)
        {
            mFixedRegistry.Invalidate(name);
        }

        void DebugLayerManager::DrawFixed(const Dia::Graphics::DebugFrameDataVisitor& visitor)
        {
            mFixedRegistry.DrawFixed(visitor);
        }

        // --------------------------------------------------------------------
        // Query
        // --------------------------------------------------------------------

        int DebugLayerManager::GetLayerCount() const
        {
            return static_cast<int>(mLayers.Size()) + mFixedRegistry.GetCount();
        }

        bool DebugLayerManager::HasLayer(Dia::Core::StringCRC layerName) const
        {
            return FindLayerIndex(layerName) >= 0 || mFixedRegistry.HasLayer(layerName);
        }

        Dia::Core::StringCRC DebugLayerManager::GetLayerName(int index) const
        {
            if (index < 0 || static_cast<unsigned int>(index) >= mLayers.Size())
                return Dia::Core::StringCRC::kZero;
            return mLayers[static_cast<unsigned int>(index)].debugger->GetLayerName();
        }

        IVisualDebugger* DebugLayerManager::GetLayer(int index) const
        {
            if (index < 0 || static_cast<unsigned int>(index) >= mLayers.Size())
                return nullptr;
            return mLayers[static_cast<unsigned int>(index)].debugger;
        }

        Dia::Core::StringCRC DebugLayerManager::GetLayerStageTag(int index) const
        {
            if (index < 0 || static_cast<unsigned int>(index) >= mLayers.Size())
                return Dia::Core::StringCRC::kZero;
            return mLayers[static_cast<unsigned int>(index)].stageTag;
        }

        bool DebugLayerManager::IsStageActive(const Dia::Core::StringCRC& stageTag) const
        {
            if (stageTag == Dia::Core::StringCRC())
                return true;
            for (unsigned int i = 0; i < mLayers.Size(); ++i)
                if (mLayers[i].stageTag == stageTag && mLayers[i].active)
                    return true;
            return false;
        }

        void DebugLayerManager::GetStageTags(Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& out) const
        {
            out.RemoveAll();
            for (unsigned int i = 0; i < mLayers.Size(); ++i)
            {
                const auto& tag = mLayers[i].stageTag;
                if (tag == Dia::Core::StringCRC())
                    continue;
                bool found = false;
                for (unsigned int j = 0; j < out.Size(); ++j)
                    if (out[j] == tag) { found = true; break; }
                if (!found)
                    out.Add(tag);
            }
        }

        // --------------------------------------------------------------------
        // Private helpers
        // --------------------------------------------------------------------

        void DebugLayerManager::SortByPriority()
        {
            // Insertion sort — stable, O(N²) acceptable for kMaxLayers = 64
            for (unsigned int i = 1; i < mLayers.Size(); ++i)
            {
                LayerEntry key = mLayers[i];
                int j = static_cast<int>(i) - 1;
                while (j >= 0 && mLayers[static_cast<unsigned int>(j)].priority > key.priority)
                {
                    mLayers[static_cast<unsigned int>(j + 1)] = mLayers[static_cast<unsigned int>(j)];
                    --j;
                }
                mLayers[static_cast<unsigned int>(j + 1)] = key;
            }
            mSortDirty = false;
        }

        int DebugLayerManager::FindLayerIndex(Dia::Core::StringCRC layerName) const
        {
            for (unsigned int i = 0; i < mLayers.Size(); ++i)
            {
                if (mLayers[i].debugger && mLayers[i].debugger->GetLayerName() == layerName)
                    return static_cast<int>(i);
            }
            return -1;
        }

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
