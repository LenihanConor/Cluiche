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
    auto& textureHandler = mTextureHandlerService.Get();
    textureHandler.Load(
        Dia::Core::StringCRC("texture.avocado_albedo"),
        Dia::Core::Containers::String512("Stages/Mesh3DTestStage/World/Textures/Avocado_baseColor.png"),
        &sNullCallback);
    textureHandler.Load(
        Dia::Core::StringCRC("texture.avocado_normal"),
        Dia::Core::Containers::String512("Stages/Mesh3DTestStage/World/Textures/Avocado_normal.png"),
        &sNullCallback);
    DIA_LOG_INFO("Mesh3DTest", "Mesh3DTestStageModule: texture loads requested");

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.mesh3dtest.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            const bool passed = GetFrameCount() >= 30;
            return { passed, passed ? "rendered 30 frames" : "waiting for 30 frames", static_cast<float>(GetFrameCount()) };
        });
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

        const bool albedoReady  = albedo && albedo->GetState() == Dia::Graphics::ITexture::State::Ready;
        const bool normalReady  = normal && normal->GetState() == Dia::Graphics::ITexture::State::Ready;
        const bool albedoFailed = albedo && albedo->GetState() == Dia::Graphics::ITexture::State::Failed;
        const bool normalFailed = normal && normal->GetState() == Dia::Graphics::ITexture::State::Failed;

        if (albedoFailed)
            DIA_LOG_WARNING("Mesh3DTest", "Avocado albedo texture failed to load");
        if (normalFailed)
            DIA_LOG_WARNING("Mesh3DTest", "Avocado normal map texture failed to load");

        if ((albedoReady || albedoFailed) && (normalReady || normalFailed))
        {
            auto* canvas3D = static_cast<Dia::Bgfx3D::Canvas3D*>(&mCanvasService.Get());
            Dia::Bgfx3D::MaterialRegistry* registry = canvas3D->GetMaterialRegistry();

            const Dia::Bgfx3D::MaterialDescriptor& defaultMat = registry->GetDefault();

            Dia::Bgfx3D::MaterialDescriptor avocadoMat;
            avocadoMat.id               = Dia::Core::StringCRC("avocado_material");
            avocadoMat.program          = defaultMat.program;
            avocadoMat.baseColourRGBA   = 0xFFFFFFFFu;
            avocadoMat.albedoTexture    = albedoReady
                ? static_cast<Dia::Bgfx::BgfxTextureHandle*>(albedo)->GetBgfxHandleIdx()
                : 0xFFFFu;
            avocadoMat.normalMapTexture = normalReady
                ? static_cast<Dia::Bgfx::BgfxTextureHandle*>(normal)->GetBgfxHandleIdx()
                : 0xFFFFu;

            registry->Register(avocadoMat);
            mTexturesLoaded = true;
            DIA_LOG_INFO("Mesh3DTest", "Mesh3DTestStageModule: avocado_material registered (albedo=%s normal=%s)",
                albedoReady ? "ok" : "fallback",
                normalReady ? "ok" : "fallback");
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

    // Directional light: sweeps left-to-right over 4s (sin wave on X axis).
    Dia::Graphics3D::DirectionalLight light;
    {
        const float t = static_cast<float>(GetFrameCount()) / 30.0f; // seconds at 30Hz
        const float sweepX = std::sin(t * Dia::Maths::PI / 2.0f);  // ±1 over 4s half-period
        const float lx = sweepX, ly = -1.0f, lz = 0.3f;
        const float len = Dia::Maths::Vector3D(lx, ly, lz).Magnitude();
        light.direction = Dia::Maths::Vector3D(lx / len, ly / len, lz / len);
    }
    light.colour    = Dia::Graphics::RGBA(255, 255, 255, 255);
    light.intensity = 1.0f;
    mFrame.AddDirectionalLight(light);

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

    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());

    // Pass after 30 frames
    if (GetFrameCount() == 30)
    {
        ReportPassed();
    }
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
