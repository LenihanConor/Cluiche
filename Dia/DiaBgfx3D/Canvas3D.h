////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaBgfx/Canvas.h>

namespace Dia { namespace Graphics3D { class FrameData3D; } }

namespace Dia
{
    namespace Bgfx3D
    {
        class MaterialRegistry;

        // Canvas3D extends Dia::Bgfx::Canvas with a 3D-aware ProcessFrame overload.
        // The inherited 2D passes (sprite, debug, UI, ImGui) work without modification.
        // 3D renderer slots (mesh, skinned, shadow) are wired in as upstream modules ship.
        //
        // Call order inside ProcessFrame(FrameData3D):
        //   1. ShadowRenderer   (no-op until 3d-renderers feature ships)
        //   2. MeshRenderer     (no-op until 3d-renderers feature ships)
        //   3. SkinnedMeshRenderer (no-op until 3d-renderers feature ships)
        //   4-7. Inherited 2D passes via base Canvas::ProcessFrame
        class Canvas3D : public Dia::Bgfx::Canvas
        {
        public:
            Canvas3D();
            ~Canvas3D() override;

            // Extended entry point — dispatches 3D passes then inherited 2D passes.
            void ProcessFrame(const Dia::Graphics3D::FrameData3D& frameData);

            // App wire-up: register materials before rendering begins.
            MaterialRegistry* GetMaterialRegistry();

        private:
            MaterialRegistry* mMaterialRegistry;  // owned

            Canvas3D(const Canvas3D&) = delete;
            Canvas3D& operator=(const Canvas3D&) = delete;
        };

    } // namespace Bgfx3D
} // namespace Dia
