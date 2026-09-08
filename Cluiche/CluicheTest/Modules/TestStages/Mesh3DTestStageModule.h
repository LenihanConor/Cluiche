#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaGraphics3D/FrameData3D.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaGeometry3D/Shapes/Spline3D.h>

#ifdef DIA_DEBUG
#include <DiaApplicationFlow/ModuleRefV2.h>
#include "Modules/VisualDebuggerModule.h"
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <memory>
#endif

namespace Dia { namespace Mesh3D { class Mesh3DAsset; } }
#ifdef DIA_DEBUG
namespace Dia { namespace Mesh3D      { class MeshBoundsDrawer; class MeshOriginDrawer; class MeshStatsDrawer; } }
namespace Dia { namespace Lighting3D  { class LightWidgetsDrawer; } }
#endif

namespace CluicheTest {

class Mesh3DTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Renders a procedural unit cube with a directional light via Canvas3D";
    explicit Mesh3DTestStageModule(const Dia::Core::StringCRC& instanceId);
#ifdef DIA_DEBUG
    ~Mesh3DTestStageModule() override;
#endif

protected:
    bool AreDependenciesReady() override;
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 300; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics3D::FrameData3D>                mRenderOutput{this, "SimToRender3D"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Mesh3D::Mesh3DAssetHandler>      mMeshHandlerService{this, "KernelMeshHandler"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::AssetRuntime::TextureHandler>    mTextureHandlerService{this, "KernelTextureHandler"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Graphics::ICanvas>               mCanvasService{this, "KernelCanvas"};

    Dia::Graphics3D::FrameData3D  mFrame;
    Dia::Mesh3D::Mesh3DAsset*     mUnitCubeAsset = nullptr;
    bool                          mTexturesLoaded = false;

    Dia::Geometry3D::Spline3D     mSunSpline;
    Dia::Geometry3D::Spline3D     mFillSpline;
    Dia::Geometry3D::Spline3D     mRimSpline;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};

    Dia::Lighting3D::LightRegistry3D                        mLightRegistry;
    std::unique_ptr<Dia::Mesh3D::MeshBoundsDrawer>          mBoundsDrawer;
    std::unique_ptr<Dia::Mesh3D::MeshOriginDrawer>          mOriginsDrawer;
    std::unique_ptr<Dia::Mesh3D::MeshStatsDrawer>           mStatsDrawer;
    std::unique_ptr<Dia::Lighting3D::LightWidgetsDrawer>    mLightWidgetsDrawer;
    bool mDebugDrawersRegistered = false;
#endif
};

} // namespace CluicheTest
