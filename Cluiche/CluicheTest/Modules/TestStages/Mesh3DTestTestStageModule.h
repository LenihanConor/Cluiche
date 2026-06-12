#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaGraphics3D/FrameData3D.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>

namespace Dia { namespace Mesh3D { class Mesh3DAsset; } }

namespace CluicheTest {

class Mesh3DTestTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Renders a procedural unit cube with a directional light via Canvas3D";
    explicit Mesh3DTestTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    bool AreDependenciesReady() override;
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 300; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics3D::FrameData3D>                mRenderOutput{this, "SimToRender3D"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Mesh3D::Mesh3DAssetHandler>      mMeshHandlerService{this, "KernelMeshHandler"};

    Dia::Graphics3D::FrameData3D  mFrame;
    Dia::Mesh3D::Mesh3DAsset*     mUnitCubeAsset = nullptr;
};

} // namespace CluicheTest
