#include "Modules/TestStages/Mesh3DTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <cmath>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaGraphics3D/Camera3D.h>
#include <DiaGraphics3D/Light.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Primitives/UnitCube.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include <DiaGraphics/Assets/ITexture.h>
#include <DiaBgfx3D/Canvas3D.h>
#include <DiaBgfx3D/Resources/MaterialRegistry.h>
#include <DiaBgfx/Resources/BgfxTextureHandle.h>
#include <DiaGeometry3D/Shapes/Spline3D.h>

#ifdef DIA_DEBUG
#include <DiaMesh3DVisualDebugger/MeshBoundsDrawer.h>
#include <DiaMesh3DVisualDebugger/MeshOriginDrawer.h>
#include <DiaMesh3DVisualDebugger/MeshStatsDrawer.h>
#include <DiaLighting3DVisualDebugger/LightWidgetsDrawer.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include "Modules/VisualDebuggerModule.h"
#endif

namespace CluicheTest {

namespace {
    // No-op callback used when polling texture state rather than reacting to events.
    struct NullTextureCallback : public Dia::AssetRuntime::IAssetLoadCallback
    {
        void OnLoadComplete(const Dia::Core::StringCRC& /*assetId*/) override {}
        void OnLoadFailed(const Dia::Core::StringCRC& /*assetId*/, const char* /*reason*/) override {}
    };
    static NullTextureCallback sNullCallback;
} // anonymous namespace

const Dia::Core::StringCRC Mesh3DTestStageModule::kTypeId("Mesh3DTestStageModule");

Mesh3DTestStageModule::Mesh3DTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

#ifdef DIA_DEBUG
Mesh3DTestStageModule::~Mesh3DTestStageModule() = default;
#endif

bool Mesh3DTestStageModule::AreDependenciesReady()
{
    return mMeshHandlerService.IsAvailable()
        && mTextureHandlerService.IsAvailable();
}

Dia::Core::StringCRC Mesh3DTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("Mesh3DTestStage");
}

const Dia::Core::StringCRC* Mesh3DTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.mesh3dtest.passed")
    };
    outCount = 1;
    return names;
}

void Mesh3DTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    // Create and register the procedural unit cube
    mUnitCubeAsset = Dia::Mesh3D::Primitives::CreateUnitCube(Dia::Core::StringCRC("unit_cube"));
    mMeshHandlerService.Get().RegisterMesh(mUnitCubeAsset);
    mUnitCubeAsset = nullptr; // handler owns it now

    DIA_LOG_INFO("Mesh3DTest", "Mesh3DTestStageModule: unit cube registered");

    // Request async texture loads — state is polled in OnUpdate via LookupTexture/GetState.
    // Albedo is sRGB-encoded per glTF spec; normal map and ORM are linear data.
    auto& textureHandler = mTextureHandlerService.Get();
    textureHandler.Load(
        Dia::Core::StringCRC("texture.avocado_albedo"),
        Dia::Core::Containers::String512("Stages/Mesh3DTestStage/World/Textures/Avocado_baseColor.png"),
        &sNullCallback,
        Dia::AssetRuntime::TextureHandler::kFlagSRGB);
    textureHandler.Load(
        Dia::Core::StringCRC("texture.avocado_normal"),
        Dia::Core::Containers::String512("Stages/Mesh3DTestStage/World/Textures/Avocado_normal.png"),
        &sNullCallback,
        0);
    textureHandler.Load(
        Dia::Core::StringCRC("texture.avocado_orm"),
        Dia::Core::Containers::String512("Stages/Mesh3DTestStage/World/Textures/Avocado_roughnessMetallic.png"),
        &sNullCallback,
        0);
    DIA_LOG_INFO("Mesh3DTest", "Mesh3DTestStageModule: texture loads requested");

    // Red key: sweeps left↔right across the top (XY plane arc).
    // Lights the top face of the cube — should tint it red as it sweeps through.
    {
        const Dia::Maths::Vector3D sunPts[] = {
            Dia::Maths::Vector3D(-1.0f, -0.2f,  0.1f),
            Dia::Maths::Vector3D(-0.7f, -0.7f,  0.1f),
            Dia::Maths::Vector3D( 0.0f, -1.0f,  0.1f),
            Dia::Maths::Vector3D( 0.7f, -0.7f,  0.1f),
            Dia::Maths::Vector3D( 1.0f, -0.2f,  0.1f),
        };
        mSunSpline = Dia::Geometry3D::SplineFactory3D::MakeCatmullRom(sunPts, 5);
    }

    // Blue fill: sweeps up↔down from the left side (YZ plane on +X axis).
    // Lights the left face of the cube — should tint it blue as it sweeps.
    {
        const Dia::Maths::Vector3D fillPts[] = {
            Dia::Maths::Vector3D( 1.0f, -0.1f,  0.1f),
            Dia::Maths::Vector3D( 0.9f, -0.4f,  0.2f),
            Dia::Maths::Vector3D( 0.7f, -0.7f,  0.2f),
            Dia::Maths::Vector3D( 0.4f, -0.9f,  0.1f),
            Dia::Maths::Vector3D( 0.1f, -1.0f,  0.1f),
        };
        mFillSpline = Dia::Geometry3D::SplineFactory3D::MakeCatmullRom(fillPts, 5);
    }

    // Green rim: sweeps front↔back from the right side (XZ plane on -X axis).
    // Lights the front/right face of the cube — should tint it green.
    {
        const Dia::Maths::Vector3D rimPts[] = {
            Dia::Maths::Vector3D(-0.1f, -0.4f,  0.9f),
            Dia::Maths::Vector3D(-0.3f, -0.5f,  0.8f),
            Dia::Maths::Vector3D(-0.7f, -0.5f,  0.5f),
            Dia::Maths::Vector3D(-0.9f, -0.4f,  0.1f),
            Dia::Maths::Vector3D(-1.0f, -0.2f,  0.0f),
        };
        mRimSpline = Dia::Geometry3D::SplineFactory3D::MakeCatmullRom(rimPts, 5);
    }

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.mesh3dtest.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            const bool passed = GetFrameCount() >= 30;
            return { passed, passed ? "rendered 30 frames" : "waiting for 30 frames", static_cast<float>(GetFrameCount()) };
        });

#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebuggerRef.Get())
    {
        auto& lm = vd->GetLayerManager();
        mBoundsDrawer       = std::make_unique<Dia::Mesh3D::MeshBoundsDrawer>(mFrame, mMeshHandlerService.Get());
        mOriginsDrawer      = std::make_unique<Dia::Mesh3D::MeshOriginDrawer>(mFrame);
        mStatsDrawer        = std::make_unique<Dia::Mesh3D::MeshStatsDrawer>(mFrame, mMeshHandlerService.Get(), lm);
        mLightWidgetsDrawer = std::make_unique<Dia::Lighting3D::LightWidgetsDrawer>(mLightRegistry);

        static const Dia::Core::StringCRC kMesh3DStageTag("Mesh3DTestStage");
        lm.Register(mBoundsDrawer.get(),       10, kMesh3DStageTag);
        lm.Register(mOriginsDrawer.get(),      11, kMesh3DStageTag);
        lm.Register(mStatsDrawer.get(),        12, kMesh3DStageTag);
        lm.Register(mLightWidgetsDrawer.get(), 13, kMesh3DStageTag);
        mDebugDrawersRegistered = true;
        DIA_LOG_INFO("Mesh3DTest", "Mesh3DTestStageModule: debug drawers registered");
    }
#endif
}

void Mesh3DTestStageModule::OnUpdate(float /*deltaTime*/)
{
    mFrame.Clear();

    // Poll texture readiness and register the avocado material once both textures have settled.
    if (!mTexturesLoaded && mTextureHandlerService.IsAvailable() && mCanvasService.IsAvailable())
    {
        auto& textureHandler = mTextureHandlerService.Get();
        Dia::Graphics::ITexture* albedo = textureHandler.LookupTexture(Dia::Core::StringCRC("texture.avocado_albedo"));
        Dia::Graphics::ITexture* normal = textureHandler.LookupTexture(Dia::Core::StringCRC("texture.avocado_normal"));
        Dia::Graphics::ITexture* orm    = textureHandler.LookupTexture(Dia::Core::StringCRC("texture.avocado_orm"));

        const bool albedoReady  = albedo && albedo->GetState() == Dia::Graphics::ITexture::State::Ready;
        const bool normalReady  = normal && normal->GetState() == Dia::Graphics::ITexture::State::Ready;
        const bool ormReady     = orm    && orm->GetState()    == Dia::Graphics::ITexture::State::Ready;
        const bool albedoFailed = albedo && albedo->GetState() == Dia::Graphics::ITexture::State::Failed;
        const bool normalFailed = normal && normal->GetState() == Dia::Graphics::ITexture::State::Failed;
        const bool ormFailed    = orm    && orm->GetState()    == Dia::Graphics::ITexture::State::Failed;

        if (albedoFailed)
            DIA_LOG_WARNING("Mesh3DTest", "Avocado albedo texture failed to load");
        if (normalFailed)
            DIA_LOG_WARNING("Mesh3DTest", "Avocado normal map texture failed to load");
        if (ormFailed)
            DIA_LOG_WARNING("Mesh3DTest", "Avocado ORM texture failed to load");

        if ((albedoReady || albedoFailed) && (normalReady || normalFailed) && (ormReady || ormFailed))
        {
            auto* canvas3D = static_cast<Dia::Bgfx3D::Canvas3D*>(&mCanvasService.Get());
            Dia::Bgfx3D::MaterialRegistry* registry = canvas3D->GetMaterialRegistry();

            const Dia::Bgfx3D::MaterialDescriptor* default3d = registry->Resolve(Dia::Core::StringCRC("default_3d"));
            if (!default3d)
            {
                DIA_LOG_WARNING("Mesh3DTest", "Mesh3DTestStageModule: default_3d material not registered yet — deferring avocado_material");
                return;
            }

            Dia::Bgfx3D::MaterialDescriptor avocadoMat;
            avocadoMat.id               = Dia::Core::StringCRC("avocado_material");
            avocadoMat.program          = default3d->program;
            avocadoMat.baseColourRGBA   = 0xFFFFFFFFu;
            avocadoMat.albedoTexture    = albedoReady
                ? static_cast<Dia::Bgfx::BgfxTextureHandle*>(albedo)->GetBgfxHandleIdx()
                : 0xFFFFu;
            avocadoMat.normalMapTexture = normalReady
                ? static_cast<Dia::Bgfx::BgfxTextureHandle*>(normal)->GetBgfxHandleIdx()
                : 0xFFFFu;
            avocadoMat.ormTexture       = ormReady
                ? static_cast<Dia::Bgfx::BgfxTextureHandle*>(orm)->GetBgfxHandleIdx()
                : 0xFFFFu;
            avocadoMat.metallic         = 0.0f;
            avocadoMat.roughness        = 0.6f;

            registry->Register(avocadoMat);
            mTexturesLoaded = true;
            DIA_LOG_INFO("Mesh3DTest", "Mesh3DTestStageModule: avocado_material registered (albedo=%s normal=%s orm=%s)",
                albedoReady ? "ok" : "fallback",
                normalReady ? "ok" : "fallback",
                ormReady    ? "ok" : "fallback");
        }
    }

    // Camera: eye at (0,2,-5), target (0,0,0), up (0,1,0)
    Dia::Graphics3D::Camera3D camera;
    camera.SetView(
        Dia::Maths::Vector3D(0.0f, 2.0f, -5.0f),
        Dia::Maths::Vector3D(0.0f, 0.0f,  0.0f),
        Dia::Maths::Vector3D(0.0f, 1.0f,  0.0f));
    camera.SetPerspective(Dia::Maths::Angle::FromDegrees(60.0f), 1.4f, 0.1f, 1000.0f);
    static_cast<Dia::Graphics3D::Mesh3DFrameData&>(mFrame).SetCamera(camera);

    // Ambient: warm-white at 20%
    Dia::Graphics3D::AmbientLight ambient;
    ambient.colour    = Dia::Graphics::RGBA(230, 230, 217, 255);
    ambient.intensity = 0.2f;
    mFrame.SetAmbientLight(ambient);

    // Three directional lights on independent spline paths.
    // t is normalised to [0,1] over each light's period, then ping-ponged so motion reverses.
    const float seconds = static_cast<float>(GetFrameCount()) / 30.0f;

    // Sun: warm white, 8s period
    {
        const float period = 8.0f;
        const float phase  = std::fmod(seconds / period, 2.0f);
        const float t      = phase < 1.0f ? phase : 2.0f - phase; // ping-pong [0,1]
        Dia::Graphics3D::DirectionalLight light;
        light.direction = mSunSpline.Evaluate(t).AsNormal();
        light.colour    = Dia::Graphics::RGBA(255, 245, 210, 255); // warm white
        light.intensity = 0.9f;
        mFrame.AddDirectionalLight(light);

#ifdef DIA_DEBUG
        {
            static const Dia::Core::StringCRC kId("sun");
            Dia::Lighting3D::DirectionalLight3D dl;
            dl.direction = light.direction; dl.colour = light.colour; dl.intensity = light.intensity; dl.enabled = true;
            if (!mLightRegistry.Has(kId)) mLightRegistry.RegisterDirectional(kId, dl);
            else mLightRegistry.GetDirectional(kId) = dl;
        }
#endif
    }

    // Fill: cool blue, 12s period (offset by a third)
    {
        const float period = 12.0f;
        const float phase  = std::fmod((seconds + 4.0f) / period, 2.0f);
        const float t      = phase < 1.0f ? phase : 2.0f - phase;
        Dia::Graphics3D::DirectionalLight light;
        light.direction = mFillSpline.Evaluate(t).AsNormal();
        light.colour    = Dia::Graphics::RGBA(140, 190, 255, 255); // cool blue
        light.intensity = 0.7f;
        mFrame.AddDirectionalLight(light);

#ifdef DIA_DEBUG
        {
            static const Dia::Core::StringCRC kId("fill");
            Dia::Lighting3D::DirectionalLight3D dl;
            dl.direction = light.direction; dl.colour = light.colour; dl.intensity = light.intensity; dl.enabled = true;
            if (!mLightRegistry.Has(kId)) mLightRegistry.RegisterDirectional(kId, dl);
            else mLightRegistry.GetDirectional(kId) = dl;
        }
#endif
    }

    // Rim: amber/orange, 6s period (offset by two-thirds)
    {
        const float period = 6.0f;
        const float phase  = std::fmod((seconds + 2.0f) / period, 2.0f);
        const float t      = phase < 1.0f ? phase : 2.0f - phase;
        Dia::Graphics3D::DirectionalLight light;
        light.direction = mRimSpline.Evaluate(t).AsNormal();
        light.colour    = Dia::Graphics::RGBA(255, 170, 60, 255); // amber
        light.intensity = 0.7f;
        mFrame.AddDirectionalLight(light);

#ifdef DIA_DEBUG
        {
            static const Dia::Core::StringCRC kId("rim");
            Dia::Lighting3D::DirectionalLight3D dl;
            dl.direction = light.direction; dl.colour = light.colour; dl.intensity = light.intensity; dl.enabled = true;
            if (!mLightRegistry.Has(kId)) mLightRegistry.RegisterDirectional(kId, dl);
            else mLightRegistry.GetDirectional(kId) = dl;
        }
#endif
    }

    // Unit cube at (-1.5, 0, 0)
    Dia::Graphics3D::Mesh3DDrawCommand cmd;
    cmd.meshId               = Dia::Core::StringCRC("unit_cube");
    cmd.materialId           = Dia::Core::StringCRC("default_3d");
    cmd.transform            = Dia::Maths::Matrix44::FromTranslation(Dia::Maths::Vector3D(-1.5f, 0.0f, 0.0f));
    cmd.skinningPaletteIndex = 0;
    cmd.layer                = 0;
    mFrame.RequestDrawMesh(cmd);

    // Avocado at (1.5, 0, 0), scaled up — glTF source is ~0.08m, 15x makes it comparable to the cube.
    // DrawCommand is safe to submit every frame; MeshRenderer skips it until the asset is Ready.
    Dia::Graphics3D::Mesh3DDrawCommand avocadoCmd;
    avocadoCmd.meshId               = Dia::Core::StringCRC("mesh3d.avocado");
    avocadoCmd.materialId           = Dia::Core::StringCRC("avocado_material");
    avocadoCmd.transform            = Dia::Maths::Matrix44::FromTranslation(Dia::Maths::Vector3D(1.5f, 0.0f, 0.0f))
                                    * Dia::Maths::Matrix44::FromScale(15.0f);
    avocadoCmd.skinningPaletteIndex = 0;
    avocadoCmd.layer                = 0;
    mFrame.RequestDrawMesh(avocadoCmd);

#ifdef DIA_DEBUG
    if (mBoundsDrawer       && mBoundsDrawer->IsEnabled())       mBoundsDrawer->Draw(mFrame);
    if (mOriginsDrawer      && mOriginsDrawer->IsEnabled())      mOriginsDrawer->Draw(mFrame);
    if (mStatsDrawer        && mStatsDrawer->IsEnabled())        mStatsDrawer->Draw(mFrame);
    if (mLightWidgetsDrawer && mLightWidgetsDrawer->IsEnabled()) mLightWidgetsDrawer->Draw(mFrame);

    if (auto* vd = mVisualDebuggerRef.Get())
    {
        vd->SetCamera3D(camera);
        vd->DrawCoord3D(mFrame);
    }
#endif

    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());

    // Pass after 30 frames
    if (GetFrameCount() == 30)
    {
        ReportPassed();
    }
}

void Mesh3DTestStageModule::OnStop()
{
    // Remove the material we registered: it holds raw bgfx texture handle indices that
    // become invalid once TextureHandler::Unload destroys the textures on stage exit.
    // Leaving it in the registry causes a bgfx ASSERT when a subsequent stage renders
    // a mesh whose submesh materialId resolves to this stale entry.
    if (mTexturesLoaded && mCanvasService.IsAvailable())
    {
        auto* canvas3D = static_cast<Dia::Bgfx3D::Canvas3D*>(&mCanvasService.Get());
        canvas3D->GetMaterialRegistry()->Unregister(Dia::Core::StringCRC("avocado_material"));
        mTexturesLoaded = false;
    }

#ifdef DIA_DEBUG
    if (mDebugDrawersRegistered)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
        {
            auto& lm = vd->GetLayerManager();
            lm.Unregister(Dia::Debug::LayerNames::kMesh3DBounds);
            lm.Unregister(Dia::Debug::LayerNames::kMesh3DOrigins);
            lm.Unregister(Dia::Debug::LayerNames::kMesh3DStats);
            lm.Unregister(Dia::Debug::LayerNames::kLightWidgets);
        }
    }
    mBoundsDrawer.reset();
    mOriginsDrawer.reset();
    mStatsDrawer.reset();
    mLightWidgetsDrawer.reset();
    mDebugDrawersRegistered = false;
#endif
}

void Mesh3DTestStageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    TestStageModuleBase::OnConnectStreams(app);
    mRenderOutput.Connect(app);
    mMeshHandlerService.Connect(app);
    mTextureHandlerService.Connect(app);
    mCanvasService.Connect(app);
}

} // namespace CluicheTest

namespace { using Mesh3DTestStageModule_ = CluicheTest::Mesh3DTestStageModule; }
DIA_MODULE(Mesh3DTestStageModule_);
