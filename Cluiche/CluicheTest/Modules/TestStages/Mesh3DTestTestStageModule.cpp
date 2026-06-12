#include "Modules/TestStages/Mesh3DTestTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaGraphics3D/Camera3D.h>
#include <DiaGraphics3D/Light.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Primitives/UnitCube.h>

namespace CluicheTest {

const Dia::Core::StringCRC Mesh3DTestTestStageModule::kTypeId("Mesh3DTestTestStageModule");

Mesh3DTestTestStageModule::Mesh3DTestTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

bool Mesh3DTestTestStageModule::AreDependenciesReady()
{
    return mMeshHandlerService.IsAvailable();
}

Dia::Core::StringCRC Mesh3DTestTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("Mesh3DTestTestStage");
}

const Dia::Core::StringCRC* Mesh3DTestTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.mesh3dtest.passed")
    };
    outCount = 1;
    return names;
}

void Mesh3DTestTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    // Create and register the procedural unit cube
    mUnitCubeAsset = Dia::Mesh3D::Primitives::CreateUnitCube(Dia::Core::StringCRC("unit_cube"));
    mMeshHandlerService.Get().RegisterMesh(mUnitCubeAsset);
    mUnitCubeAsset = nullptr; // handler owns it now

    DIA_LOG_INFO("Mesh3DTest", "Mesh3DTestTestStageModule: unit cube registered");

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.mesh3dtest.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            const bool passed = GetFrameCount() >= 30;
            return { passed, passed ? "rendered 30 frames" : "waiting for 30 frames", static_cast<float>(GetFrameCount()) };
        });
}

void Mesh3DTestTestStageModule::OnUpdate(float /*deltaTime*/)
{
    mFrame.Clear();

    // Camera: eye at (0,2,-5), target (0,0,0), up (0,1,0)
    Dia::Graphics3D::Camera3D camera;
    camera.SetView(
        Dia::Maths::Vector3D(0.0f, 2.0f, -5.0f),
        Dia::Maths::Vector3D(0.0f, 0.0f,  0.0f),
        Dia::Maths::Vector3D(0.0f, 1.0f,  0.0f));
    camera.SetPerspective(Dia::Maths::Angle::FromDegrees(60.0f), 1.4f, 0.1f, 1000.0f);
    static_cast<Dia::Graphics3D::Mesh3DFrameData&>(mFrame).SetCamera(camera);

    // Directional light: direction (0.5,-1,0.3) normalised, white, intensity 1
    Dia::Graphics3D::DirectionalLight light;
    {
        const float lx = 0.5f, ly = -1.0f, lz = 0.3f;
        const float len = Dia::Maths::Vector3D(lx, ly, lz).Magnitude();
        light.direction = Dia::Maths::Vector3D(lx / len, ly / len, lz / len);
    }
    light.colour    = Dia::Graphics::RGBA(255, 255, 255, 255);
    light.intensity = 1.0f;
    mFrame.AddDirectionalLight(light);

    // Draw the unit cube at identity transform
    Dia::Graphics3D::Mesh3DDrawCommand cmd;
    cmd.meshId               = Dia::Core::StringCRC("unit_cube");
    cmd.materialId           = Dia::Core::StringCRC("default_3d");
    cmd.transform            = Dia::Maths::Matrix44::Identity();
    cmd.skinningPaletteIndex = 0;
    cmd.layer                = 0;
    mFrame.RequestDrawMesh(cmd);

    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());

    // Pass after 30 frames
    if (GetFrameCount() == 30)
    {
        ReportPassed();
    }
}

void Mesh3DTestTestStageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    TestStageModuleBase::OnConnectStreams(app);
    mRenderOutput.Connect(app);
    mMeshHandlerService.Connect(app);
}

} // namespace CluicheTest

namespace { using Mesh3DTestTestStageModule_ = CluicheTest::Mesh3DTestTestStageModule; }
DIA_MODULE(Mesh3DTestTestStageModule_);
