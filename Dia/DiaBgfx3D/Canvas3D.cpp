////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx3D/Canvas3D.h"
#include "DiaBgfx3D/Resources/MaterialRegistry.h"
#include "DiaBgfx3D/Resources/MeshGpuCache.h"
#include "DiaBgfx3D/Renderers/MeshRenderer.h"
#include "DiaBgfx3D/Renderers/ShadowRenderer.h"
#include "DiaBgfx3D/MeshPassLighting.h"

#include <DiaBgfx/Resources/ShaderProgram.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaGraphics3D/FrameData3D.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaGraphics3D/Light.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Metric/Gauge.h>
#include <bgfx/bgfx.h>

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
            // GPU resources (ShadowRenderer's depth texture/framebuffer, shader
            // programs) are deferred to Init3DPrograms(), which runs from the
            // first StartFrame() once bgfx is initialised. Creating them here
            // would call bgfx::createTexture2D before bgfx::init (maxTextureSize
            // reports 0 -> assert). MeshRenderer is created when a handler is set.
        }

        Canvas3D::~Canvas3D()
        {
            // Shutdown() must have been called before the destructor (it destroys
            // bgfx handles while the context is still live). If somehow it wasn't,
            // nil out the pointers rather than calling bgfx::destroy on a dead context.
            mMeshRenderer  = nullptr;
            mShadowRenderer = nullptr;
            mMeshProgram   = nullptr;
            mShadowProgram = nullptr;

            delete mMeshGpuCache;
            delete mMaterialRegistry;
        }

        void Canvas3D::Shutdown()
        {
            // Destroy 3D bgfx resources BEFORE the base class calls bgfx::shutdown().
            delete mMeshRenderer;   mMeshRenderer   = nullptr;
            delete mShadowRenderer; mShadowRenderer = nullptr;

            delete mMeshProgram;   mMeshProgram   = nullptr;
            delete mShadowProgram; mShadowProgram = nullptr;

            if (mMeshGpuCache)
                mMeshGpuCache->DestroyAll();

            m3DInitialised = false;

            Dia::Bgfx::Canvas::Shutdown();
        }

        void Canvas3D::StartFrame(const Dia::Graphics::FrameData& frame)
        {
            Dia::Bgfx::Canvas::StartFrame(frame);
            if (!m3DInitialised && IsInitialised())
                Init3DPrograms();
        }

        void Canvas3D::SetMetrics(Dia::Observation::Metric::Gauge* meshDrawCalls,
                                   Dia::Observation::Metric::Gauge* gpuMeshCount)
        {
            mMetricMeshDrawCalls = meshDrawCalls;
            mMetricGpuMeshCount  = gpuMeshCount;
        }

        Dia::Observation::Health::HealthReporterBase& Canvas3D::GetHealthReporter()
        {
            return mHealthReporter;
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
            {
                DIA_LOG_WARNING("DiaBgfx3D", "Canvas3D::Init3DPrograms — failed to load mesh shader");
                mHealthReporter.SetDegraded(Dia::Core::StringCRC("mesh_shader_load_failed"));
            }

            mShadowProgram = new Dia::Bgfx::ShaderProgram();
            if (!mShadowProgram->LoadFromPath(root, backend, "3d/vs_shadow_caster.bin", "3d/fs_shadow_caster.bin"))
            {
                DIA_LOG_WARNING("DiaBgfx3D", "Canvas3D::Init3DPrograms — failed to load shadow_caster shader");
                mHealthReporter.SetDegraded(Dia::Core::StringCRC("shadow_shader_load_failed"));
            }

            // Register default material
            Dia::Bgfx3D::MaterialDescriptor def;
            def.id               = Dia::Core::StringCRC("default_3d");
            def.program          = mMeshProgram;
            def.baseColourRGBA   = 0xCCCCCCFFu;
            def.albedoTexture    = 0xFFFFu;
            def.normalMapTexture = 0xFFFFu;
            mMaterialRegistry->Register(def);

            // Create renderers now that bgfx is initialised — their ctors call
            // bgfx::createUniform / bgfx::createTexture2D, which require a live context.
            if (!mShadowRenderer)
                mShadowRenderer = new ShadowRenderer(mShadowViewId, mMeshGpuCache);
            mShadowRenderer->SetProgram(mShadowProgram);

            if (!mMeshRenderer && mMeshHandler)
            {
                mMeshRenderer = new MeshRenderer(mMeshViewId, mMeshGpuCache,
                                                 mMaterialRegistry, mMeshHandler);
                mMeshRenderer->InitUniforms();
            }

            m3DInitialised = true;
            mHealthReporter.SetOK();
            DIA_LOG_INFO("DiaBgfx3D", "Canvas3D::Init3DPrograms complete (backend=%s)", backend);
        }

        void Canvas3D::ProcessFrame(const Dia::Graphics3D::FrameData3D& frameData)
        {
            DIA_TRACE_ZONE("canvas3d.process_frame", ::Dia::Observation::Trace::Category::kDiaGraphics);

            if (!mMeshRenderer)
            {
                DIA_LOG_DEBUG("DiaBgfx3D", "Canvas3D::ProcessFrame — no MeshRenderer (handler not yet set)");
                Dia::Bgfx::Canvas::ProcessFrame(static_cast<const Dia::Graphics::FrameData&>(frameData));
                return;
            }

            // FrameData3D inherits Mesh3DFrameData — cast to access 3D draw commands.
            const Dia::Graphics3D::Mesh3DFrameData& mesh3d =
                static_cast<const Dia::Graphics3D::Mesh3DFrameData&>(frameData);

            // Upload camera to mesh view. Must happen before any draw submissions
            // on this view; bgfx latches the transform at submit time.
            {
                const Dia::Graphics3D::Camera3D& cam = mesh3d.GetCamera();
                float viewMtx[16], projMtx[16];
                cam.view.GetColumnMajor(viewMtx);
                cam.projection.GetColumnMajor(projMtx);
                bgfx::setViewTransform(mMeshViewId, viewMtx, projMtx);

                const Dia::Maths::Vector2D& sz = GetCanvasSize();
                uint16_t w = static_cast<uint16_t>(sz.X());
                uint16_t h = static_cast<uint16_t>(sz.Y());
                if (w == 0) w = 1280;
                if (h == 0) h = 720;
                bgfx::setViewRect(mMeshViewId, 0, 0, w, h);
                bgfx::setViewClear(mMeshViewId,
                                   BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                                   0x303030FF, 1.0f, 0);
            }

            // 1. Shadow pass
            if (mShadowRenderer && mMeshHandler)
                mShadowRenderer->RenderShadowMap(mesh3d, mMeshHandler);

            // 2. Static mesh pass
            if (mMeshRenderer)
            {
                MeshPassLighting lighting{};

                // Ambient from frame data (game-controlled).
                {
                    const auto& amb = mesh3d.GetAmbientLight();
                    lighting.ambient[0] = static_cast<float>(amb.colour.R()) / 255.0f;
                    lighting.ambient[1] = static_cast<float>(amb.colour.G()) / 255.0f;
                    lighting.ambient[2] = static_cast<float>(amb.colour.B()) / 255.0f;
                    lighting.ambient[3] = amb.intensity;
                }

                const auto& dirLights = mesh3d.GetDirectionalLights();
                if (dirLights.Size() > 0)
                {
                    const auto& dl = dirLights[0];
                    lighting.dirLightDir[0] = dl.direction.X();
                    lighting.dirLightDir[1] = dl.direction.Y();
                    lighting.dirLightDir[2] = dl.direction.Z();
                    lighting.dirLightDir[3] = 0.0f;

                    lighting.dirLightColour[0] = static_cast<float>(dl.colour.R()) / 255.0f;
                    lighting.dirLightColour[1] = static_cast<float>(dl.colour.G()) / 255.0f;
                    lighting.dirLightColour[2] = static_cast<float>(dl.colour.B()) / 255.0f;
                    lighting.dirLightColour[3] = dl.intensity;
                }
                else
                {
                    // No light in scene — use a default overhead sun.
                    lighting.dirLightDir[0]    = 0.0f;
                    lighting.dirLightDir[1]    = 1.0f; // pointing up == coming from above
                    lighting.dirLightDir[2]    = 0.0f;
                    lighting.dirLightDir[3]    = 0.0f;
                    lighting.dirLightColour[0] = 1.0f;
                    lighting.dirLightColour[1] = 1.0f;
                    lighting.dirLightColour[2] = 1.0f;
                    lighting.dirLightColour[3] = 1.0f;
                }

                if (mShadowRenderer)
                {
                    lighting.shadowTexture = mShadowRenderer->GetShadowTexture();
                    mShadowRenderer->GetLightViewProj(lighting.lightViewProj);
                }
                else
                {
                    lighting.shadowTexture = bgfx::kInvalidHandle;
                }

                mMeshRenderer->Draw(mesh3d, lighting);

                if (mMetricMeshDrawCalls)
                    mMetricMeshDrawCalls->Set(static_cast<double>(mesh3d.GetMeshDraws().Size()));
                if (mMetricGpuMeshCount)
                    mMetricGpuMeshCount->Set(static_cast<double>(mMeshGpuCache->GetResidentCount()));
            }

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
            // Must be called from RenderPU before the first StartFrame().
            // Init3DPrograms() (called on first StartFrame) creates MeshRenderer
            // using this pointer — if called after bgfx init it would be a data race.
            mMeshHandler = handler;

            if (handler)
            {
                MeshGpuCache* cache = mMeshGpuCache;
                handler->SetEvictCallback([cache](uint32_t assetId)
                {
                    cache->Evict(assetId);
                });
            }
        }

    } // namespace Bgfx3D
} // namespace Dia
