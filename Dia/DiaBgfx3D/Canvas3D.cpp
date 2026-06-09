////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx3D/Canvas3D.h"
#include "DiaBgfx3D/Resources/MaterialRegistry.h"

#include <DiaGraphics3D/FrameData3D.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia
{
    namespace Bgfx3D
    {
        Canvas3D::Canvas3D()
            : Dia::Bgfx::Canvas()
            , mMaterialRegistry(new MaterialRegistry())
        {}

        Canvas3D::~Canvas3D()
        {
            delete mMaterialRegistry;
            mMaterialRegistry = nullptr;
        }

        void Canvas3D::ProcessFrame(const Dia::Graphics3D::FrameData3D& frameData)
        {
            // 3D passes — wired in as upstream modules ship:
            //   ShadowRenderer::RenderShadowMap   (3d-renderers feature)
            //   MeshRenderer::Draw                (3d-renderers feature)
            //   SkinnedMeshRenderer::Draw         (3d-renderers feature)

            // Inherited 2D passes (sprite, debug, UI, ImGui).
            // FrameData3D inherits FrameData so this cast is safe.
            Dia::Bgfx::Canvas::ProcessFrame(static_cast<const Dia::Graphics::FrameData&>(frameData));
        }

        MaterialRegistry* Canvas3D::GetMaterialRegistry()
        {
            DIA_ASSERT(mMaterialRegistry != nullptr, "Canvas3D::GetMaterialRegistry — registry is null");
            return mMaterialRegistry;
        }

    } // namespace Bgfx3D
} // namespace Dia
