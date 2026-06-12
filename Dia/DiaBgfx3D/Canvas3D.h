////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaBgfx/Canvas.h>

namespace Dia { namespace Graphics3D { class FrameData3D; } }
namespace Dia { namespace Mesh3D { class Mesh3DAssetHandler; } }
namespace Dia { namespace Bgfx { class ShaderProgram; } }

namespace Dia
{
    namespace Bgfx3D
    {
        class MaterialRegistry;
        class MeshGpuCache;
        class MeshRenderer;
        class ShadowRenderer;

        class Canvas3D : public Dia::Bgfx::Canvas
        {
        public:
            Canvas3D();
            ~Canvas3D() override;

            void StartFrame(const Dia::Graphics::FrameData& frame) override;
            void ProcessFrame(const Dia::Graphics3D::FrameData3D& frameData);

            MaterialRegistry* GetMaterialRegistry();
            MeshGpuCache*     GetMeshGpuCache();

            void SetMeshHandler(Dia::Mesh3D::Mesh3DAssetHandler* handler);

        private:
            void Init3DPrograms();

            unsigned short    mMeshViewId;
            unsigned short    mShadowViewId;
            MaterialRegistry* mMaterialRegistry;  // owned
            MeshGpuCache*     mMeshGpuCache;      // owned
            MeshRenderer*     mMeshRenderer;      // owned
            ShadowRenderer*   mShadowRenderer;    // owned
            Dia::Mesh3D::Mesh3DAssetHandler* mMeshHandler; // not owned

            Dia::Bgfx::ShaderProgram* mMeshProgram;    // owned
            Dia::Bgfx::ShaderProgram* mShadowProgram;  // owned
            bool                      m3DInitialised;

            Canvas3D(const Canvas3D&) = delete;
            Canvas3D& operator=(const Canvas3D&) = delete;
        };

    } // namespace Bgfx3D
} // namespace Dia
