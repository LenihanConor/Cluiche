////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx3D/Canvas3D.h"
#include "DiaBgfx3D/Resources/MaterialRegistry.h"
#include "DiaBgfx3D/Resources/MeshGpuCache.h"
#include "DiaBgfx3D/Renderers/MeshRenderer.h"
#include "DiaBgfx3D/Renderers/ShadowRenderer.h"

#include <DiaGraphics3D/FrameData3D.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace Dia
{
    namespace Bgfx3D
    {
        static constexpr unsigned short kShadowViewId = 1;
        static constexpr unsigned short kMeshViewId   = 2;

        Canvas3D::Canvas3D()
            : Dia::Bgfx::Canvas()
            , mMeshViewId(kMeshViewId)
            , mShadowViewId(kShadowViewId)
            , mMaterialRegistry(new MaterialRegistry())
            , mMeshGpuCache(new MeshGpuCache())
            , mMeshRenderer(nullptr)
            , mShadowRenderer(nullptr)
            , mMeshHandler(nullptr)
        {
            mShadowRenderer = new ShadowRenderer(mShadowViewId, mMeshGpuCache);
            // MeshRenderer created once a mesh handler is set
        }

        Canvas3D::~Canvas3D()
        {
            delete mMeshRenderer;
            delete mShadowRenderer;

            mMeshGpuCache->DestroyAll();
            delete mMeshGpuCache;
            delete mMaterialRegistry;
        }

        void Canvas3D::ProcessFrame(const Dia::Graphics3D::FrameData3D& frameData)
        {
            // FrameData3D inherits Mesh3DFrameData — cast to access 3D draw commands.
            const Dia::Graphics3D::Mesh3DFrameData& mesh3d =
                static_cast<const Dia::Graphics3D::Mesh3DFrameData&>(frameData);

            // 1. Shadow pass
            if (mShadowRenderer && mMeshHandler)
                mShadowRenderer->RenderShadowMap(mesh3d, mMeshHandler);

            // 2. Static mesh pass
            if (mMeshRenderer)
                mMeshRenderer->Draw(mesh3d);

            // 3. SkinnedMeshRenderer — wired when DiaSkinning3D ships

            // 4-7. Inherited 2D passes (sprite, debug, UI, ImGui).
            Dia::Bgfx::Canvas::ProcessFrame(static_cast<const Dia::Graphics::FrameData&>(frameData));
        }

        MaterialRegistry* Canvas3D::GetMaterialRegistry()
        {
            return mMaterialRegistry;
        }

        MeshGpuCache* Canvas3D::GetMeshGpuCache()
        {
            return mMeshGpuCache;
        }

        void Canvas3D::SetMeshHandler(Dia::Mesh3D::Mesh3DAssetHandler* handler)
        {
            mMeshHandler = handler;
            if (mMeshHandler && !mMeshRenderer)
            {
                mMeshRenderer = new MeshRenderer(mMeshViewId, mMeshGpuCache,
                                                 mMaterialRegistry, mMeshHandler);
            }
        }

    } // namespace Bgfx3D
} // namespace Dia
