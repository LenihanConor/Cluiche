////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx3D/Canvas3D.h"
#include "DiaBgfx3D/Resources/MaterialRegistry.h"
#include "DiaBgfx3D/Resources/MeshGpuCache.h"
#include "DiaBgfx3D/Renderers/MeshRenderer.h"
#include "DiaBgfx3D/Renderers/ShadowRenderer.h"

#include <DiaBgfx/Resources/ShaderProgram.h>
#include <DiaGraphics3D/FrameData3D.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaObservation/Log/DiaLog.h>

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
            , mMeshProgram(nullptr)
            , mShadowProgram(nullptr)
            , m3DInitialised(false)
        {
            mShadowRenderer = new ShadowRenderer(mShadowViewId, mMeshGpuCache);
            // MeshRenderer created once a mesh handler is set
        }

        Canvas3D::~Canvas3D()
        {
            delete mMeshRenderer;
            delete mShadowRenderer;

            delete mMeshProgram;   mMeshProgram   = nullptr;
            delete mShadowProgram; mShadowProgram = nullptr;

            mMeshGpuCache->DestroyAll();
            delete mMeshGpuCache;
            delete mMaterialRegistry;
        }

        void Canvas3D::StartFrame(const Dia::Graphics::FrameData& frame)
        {
            Dia::Bgfx::Canvas::StartFrame(frame);
            if (!m3DInitialised && IsInitialised())
                Init3DPrograms();
        }

        void Canvas3D::Init3DPrograms()
        {
            const char* root    = GetShaderRoot();
            const char* backend = nullptr;
            switch (GetRendererType())
            {
                case Dia::Bgfx::RendererType::Direct3D11: backend = "dx11";   break;
                case Dia::Bgfx::RendererType::Direct3D12: backend = "dx12";   break;
                case Dia::Bgfx::RendererType::Vulkan:     backend = "vulkan"; break;
                default:                                  backend = "dx11";   break;
            }

            mMeshProgram = new Dia::Bgfx::ShaderProgram();
            if (!mMeshProgram->LoadFromPath(root, backend, "3d/vs_mesh.bin", "3d/fs_mesh.bin"))
                DIA_LOG_WARNING("DiaBgfx3D", "Canvas3D::Init3DPrograms — failed to load mesh shader");

            mShadowProgram = new Dia::Bgfx::ShaderProgram();
            if (!mShadowProgram->LoadFromPath(root, backend, "3d/vs_shadow_caster.bin", "3d/fs_shadow_caster.bin"))
                DIA_LOG_WARNING("DiaBgfx3D", "Canvas3D::Init3DPrograms — failed to load shadow_caster shader");

            // Register default material
            Dia::Bgfx3D::MaterialDescriptor def;
            def.id             = Dia::Core::StringCRC("default_3d");
            def.program        = mMeshProgram;
            def.baseColourRGBA = 0xCCCCCCFFu;
            mMaterialRegistry->Register(def);

            // Wire shadow renderer with the shadow program
            if (mShadowRenderer)
                mShadowRenderer->SetProgram(mShadowProgram);

            m3DInitialised = true;
            DIA_LOG_INFO("DiaBgfx3D", "Canvas3D::Init3DPrograms complete (backend=%s)", backend);
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
