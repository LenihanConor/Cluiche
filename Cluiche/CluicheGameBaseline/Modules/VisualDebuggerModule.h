#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Streams/StreamWriter.h>
#include <DiaApplicationFlow/Streams/ServiceStreamWriter.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include "Modules/InputStreamModule.h"
#include "Modules/CameraModule.h"
#include <memory>

namespace Dia::Debug
{
    class Coord2DOriginDrawer;
    class Coord2DAxesDrawer;
    class Coord2DGridDrawer;
    class Coord2DBoundsDrawer;
    class Coord2DCursorDrawer;
}

namespace Cluiche { namespace AppFlow {

class VisualDebuggerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Debug layer manager and SimToRender debug draw";
    explicit VisualDebuggerModule(const Dia::Core::StringCRC& instanceId);

    Dia::Debug::DebugLayerManager& GetLayerManager() { return mLayerManager; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void RegisterCoord2DDrawers();
    void UnregisterCoord2DDrawers();

    Dia::Debug::DebugLayerManager mLayerManager;
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData> mRenderOutput{this, "SimToRender"};
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::Debug::DebugLayerManager> mLayerManagerService{this, "DebugLayerManager"};
    Dia::Graphics::FrameData mFrame;
    Dia::Core::StringCRC mLastKnownStage;

    Dia::ApplicationFlow::ModuleRef<InputStreamModule> mInputRef{this};
    Dia::ApplicationFlow::ModuleRef<CameraModule>      mCameraRef{this};

    std::unique_ptr<Dia::Debug::Coord2DOriginDrawer> mCoord2DOriginDrawer;
    std::unique_ptr<Dia::Debug::Coord2DAxesDrawer>   mCoord2DAxesDrawer;
    std::unique_ptr<Dia::Debug::Coord2DGridDrawer>   mCoord2DGridDrawer;
    std::unique_ptr<Dia::Debug::Coord2DBoundsDrawer> mCoord2DBoundsDrawer;
    std::unique_ptr<Dia::Debug::Coord2DCursorDrawer> mCoord2DCursorDrawer;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
