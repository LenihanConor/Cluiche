#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaStreams/ServiceStreamWriter.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics3D/FrameData3D.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include "Modules/InputStreamModule.h"
#include "Modules/Camera2DModule.h"
#include "Types/DebugPanelCommandEvent.h"
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <memory>
#include <mutex>

namespace Dia::Debug
{
    class Coord2DOriginDrawer;
    class Coord2DAxesDrawer;
    class Coord2DGridDrawer;
    class Coord2DBoundsDrawer;
    class Coord2DCursorDrawer;
    class Coord3DOriginDrawer;
    class Coord3DAxesDrawer;
    class Coord3DGridDrawer;
    class Coord3DCameraDrawer;
}

namespace Dia::Graphics3D
{
    struct Camera3D;
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

    void RegisterDomain(Dia::VisualDebugger::IDebugDomain& domain);
    void UnregisterDomain(Dia::VisualDebugger::IDebugDomain& domain);
    const Dia::VisualDebugger::DiaDebugDomainRegistry& GetDomainRegistry() const { return mDomainRegistry; }

    // Called from Render PU (panel JS callback) — thread-safe
    void EnqueueCommand(Dia::Core::StringCRC domainId,
                        Dia::Core::StringCRC cmd,
                        const char* argsJson);

    void SetCamera3D(const Dia::Graphics3D::Camera3D& camera);
    void DrawCoord3D(Dia::Graphics3D::FrameData3D& frame);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    struct PendingCommand
    {
        Dia::Core::StringCRC domainId;
        Dia::Core::StringCRC cmd;
        // args serialized as a JSON string; parsed back on sim thread
        char argsJson[512];
    };
    static constexpr int kCommandQueueCapacity = 32;

    void RegisterCoord2DDrawers();
    void UnregisterCoord2DDrawers();
    void RegisterCoord3DDrawers();
    void UnregisterCoord3DDrawers();

    // Drain pending panel commands (enqueued from Render PU)
    void DrainPendingCommands();
    // Drain DiaDebugPanel commands arriving from the Main PU over the
    // DebugPanelCommand EventStream and funnel them into the command queue.
    void DrainPanelCommandStream();

    Dia::Debug::DebugLayerManager mLayerManager;
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData> mRenderOutput{this, "SimToRender"};
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::Debug::DebugLayerManager> mLayerManagerService{this, "DebugLayerManager"};
    Dia::VisualDebugger::DiaDebugDomainRegistry mDomainRegistry;
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::VisualDebugger::DiaDebugDomainRegistry> mDomainRegistryService{this, "DomainRegistry"};
    Dia::ApplicationFlow::EventStreamReader<DebugPanelCommandEvent> mPanelCommands{this, "DebugPanelCommand"};
    Dia::Core::Containers::DynamicArrayC<PendingCommand, kCommandQueueCapacity> mCommandQueue;
    std::mutex mCommandQueueMutex;
    Dia::Graphics::FrameData mFrame;
    Dia::Core::StringCRC mLastKnownStage;

    Dia::ApplicationFlow::ModuleRef<InputStreamModule> mInputRef{this};
    Dia::ApplicationFlow::ModuleRef<Camera2DModule>    mCameraRef{this};

    std::unique_ptr<Dia::Debug::Coord2DOriginDrawer> mCoord2DOriginDrawer;
    std::unique_ptr<Dia::Debug::Coord2DAxesDrawer>   mCoord2DAxesDrawer;
    std::unique_ptr<Dia::Debug::Coord2DGridDrawer>   mCoord2DGridDrawer;
    std::unique_ptr<Dia::Debug::Coord2DBoundsDrawer> mCoord2DBoundsDrawer;
    std::unique_ptr<Dia::Debug::Coord2DCursorDrawer> mCoord2DCursorDrawer;

    std::unique_ptr<Dia::Debug::Coord3DOriginDrawer> mCoord3DOriginDrawer;
    std::unique_ptr<Dia::Debug::Coord3DAxesDrawer>   mCoord3DAxesDrawer;
    std::unique_ptr<Dia::Debug::Coord3DGridDrawer>   mCoord3DGridDrawer;
    std::unique_ptr<Dia::Debug::Coord3DCameraDrawer> mCoord3DCameraDrawer;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
