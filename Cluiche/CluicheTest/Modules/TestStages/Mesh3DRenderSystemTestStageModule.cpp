#include "Modules/TestStages/Mesh3DRenderSystemTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaGraphics3D/Camera3D.h>
#include <DiaGraphics3D/Light.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Primitives/UnitCube.h>
#include <cmath>

namespace CluicheTest {

const Dia::Core::StringCRC Mesh3DRenderSystemTestStageModule::kTypeId("Mesh3DRenderSystemTestStageModule");

Mesh3DRenderSystemTestStageModule::Mesh3DRenderSystemTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

bool Mesh3DRenderSystemTestStageModule::AreDependenciesReady()
{
    return mMeshHandlerService.IsAvailable();
}

Dia::Core::StringCRC Mesh3DRenderSystemTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("Mesh3DRenderSystemTestStage");
}

const Dia::Core::StringCRC* Mesh3DRenderSystemTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.mesh3drendersystem.passed")
    };
    outCount = 1;
    return names;
}

void Mesh3DRenderSystemTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    DIA_LOG_INFO("Mesh3DRenderSystem", "Mesh3DRenderSystemTestStageModule: OnStart");

    Dia::Mesh3D::Mesh3DAsset* unitCube = Dia::Mesh3D::Primitives::CreateUnitCube(Dia::Core::StringCRC("unit_cube"));
    mMeshHandlerService.Get().RegisterMesh(unitCube);
    DIA_LOG_INFO("Mesh3DRenderSystem", "Mesh3DRenderSystemTestStageModule: unit cube registered");

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.mesh3drendersystem.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            const bool passed = GetFrameCount() >= 60;
            return { passed, passed ? "rendered 60 frames" : "waiting", static_cast<float>(GetFrameCount()) };
        });
}

void Mesh3DRenderSystemTestStageModule::OnUpdate(float /*deltaTime*/)
{
    mFrame.Clear();

    // Camera: eye at (0,3,-8), target (0,0,0), up (0,1,0), FOV 60 degrees
    Dia::Graphics3D::Camera3D camera;
    camera.SetView(
        Dia::Maths::Vector3D(0.0f,  3.0f, -8.0f),
        Dia::Maths::Vector3D(0.0f,  0.0f,  0.0f),
        Dia::Maths::Vector3D(0.0f,  1.0f,  0.0f));
    camera.SetPerspective(Dia::Maths::Angle::FromDegrees(60.0f), 1.4f, 0.1f, 1000.0f);
    static_cast<Dia::Graphics3D::Mesh3DFrameData&>(mFrame).SetCamera(camera);

    // Ambient light
    Dia::Graphics3D::AmbientLight ambient;
    ambient.colour    = Dia::Graphics::RGBA(230, 230, 217, 255);
    ambient.intensity = 0.2f;
    mFrame.SetAmbientLight(ambient);

    // Directional light: sweeps left-to-right over 4s
    Dia::Graphics3D::DirectionalLight light;
    {
        const float t = static_cast<float>(GetFrameCount()) / 30.0f;
        const float sweepX = std::sin(t * Dia::Maths::PI / 2.0f);
        const float lx = sweepX, ly = -1.0f, lz = 0.3f;
        const float len = Dia::Maths::Vector3D(lx, ly, lz).Magnitude();
        light.direction = Dia::Maths::Vector3D(lx / len, ly / len, lz / len);
    }
    light.colour    = Dia::Graphics::RGBA(255, 255, 255, 255);
    light.intensity = 1.0f;
    mFrame.AddDirectionalLight(light);

    // 3 valid unit cubes at distinct positions (-3, 0, 3) on X axis
    for (int i = -1; i <= 1; ++i)
    {
        Dia::Graphics3D::Mesh3DDrawCommand cmd;
        cmd.meshId               = Dia::Core::StringCRC("unit_cube");
        cmd.materialId           = Dia::Core::StringCRC("default_3d");
        cmd.transform            = Dia::Maths::Matrix44::FromTranslation(Dia::Maths::Vector3D(3.0f * static_cast<float>(i), 0.0f, 0.0f));
        cmd.skinningPaletteIndex = 0;
        cmd.layer                = 0;
        mFrame.RequestDrawMesh(cmd);
    }

    // 1 glTF mesh (avocado) — silently skipped until asset is ready
    Dia::Graphics3D::Mesh3DDrawCommand avocado;
    avocado.meshId               = Dia::Core::StringCRC("mesh3d.avocado");
    avocado.materialId           = Dia::Core::StringCRC("default_3d");
    avocado.transform            = Dia::Maths::Matrix44::FromTranslation(Dia::Maths::Vector3D(0.0f, 2.0f, 0.0f))
                                 * Dia::Maths::Matrix44::FromScale(15.0f);
    avocado.skinningPaletteIndex = 0;
    avocado.layer                = 0;
    mFrame.RequestDrawMesh(avocado);

    // 1 unknown mesh ID — must be silently skipped every frame without crash
    Dia::Graphics3D::Mesh3DDrawCommand ghost;
    ghost.meshId               = Dia::Core::StringCRC("mesh3d.does_not_exist");
    ghost.materialId           = Dia::Core::StringCRC("default_3d");
    ghost.transform            = Dia::Maths::Matrix44::Identity();
    ghost.skinningPaletteIndex = 0;
    ghost.layer                = 0;
    mFrame.RequestDrawMesh(ghost);

    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());

    if (GetFrameCount() == 60)
        ReportPassed();
}

void Mesh3DRenderSystemTestStageModule::OnStop()
{
    DIA_LOG_INFO("Mesh3DRenderSystem", "Mesh3DRenderSystemTestStageModule: OnStop");

    // Flush stale draw commands so the RenderPU does not render meshes after assets unload.
    mFrame.Clear();
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
}

void Mesh3DRenderSystemTestStageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    TestStageModuleBase::OnConnectStreams(app);
    mRenderOutput.Connect(app);
    mMeshHandlerService.Connect(app);
}

} // namespace CluicheTest

namespace { using Mesh3DRenderSystemTestStageModule_ = CluicheTest::Mesh3DRenderSystemTestStageModule; }
DIA_MODULE(Mesh3DRenderSystemTestStageModule_);
