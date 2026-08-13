#include "Modules/VisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
// Full domain definitions needed here so unique_ptr can destroy complete types.
#include <DiaVisualDebugger/Coord2D/Coord2DDebugDomain.h>
#include <DiaVisualDebugger/Coord3D/Coord3DDebugDomain.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <mutex>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC VisualDebuggerModule::kTypeId("VisualDebuggerModule");

VisualDebuggerModule::VisualDebuggerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

// Destructor defined here (not in header) so unique_ptr can destroy the
// forward-declared Coord2DDebugDomain / Coord3DDebugDomain with their complete
// drawer types visible.
VisualDebuggerModule::~VisualDebuggerModule() = default;

Dia::ApplicationFlow::StartResult VisualDebuggerModule::DoStart()
{
    mLayerManagerService.Register(mLayerManager);
    mDomainRegistryService.Register(mDomainRegistry);
    mLayerManager.SetDebugScale(50.0f);
    mLayerManager.RegisterDiaAPICommands();
    mCoord2DDomain = std::make_unique<Dia::Debug::Coord2DDebugDomain>();
    RegisterDomain(*mCoord2DDomain);
    mCoord3DDomain = std::make_unique<Dia::Debug::Coord3DDebugDomain>();
    RegisterDomain(*mCoord3DDomain);
    return Dia::ApplicationFlow::StartResult::kReady;
}

void VisualDebuggerModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("VisualDebuggerModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    DrainPanelCommandStream();
    DrainPendingCommands();

    auto* app = GetApplication();
    if (app)
    {
        Dia::Core::StringCRC currentStage = app->GetCurrentStage();
        if (currentStage != mLastKnownStage)
        {
            if (!(mLastKnownStage == Dia::Core::StringCRC()))
                mLayerManager.SetStageActive(mLastKnownStage, false);
            mLayerManager.SetStageActive(currentStage, true);
            mLastKnownStage = currentStage;
        }
    }

    const bool hasCam = mCameraRef && mCameraRef->HasActiveCamera();
    const Dia::Camera2D::Camera2D&  camera     = hasCam ? mCameraRef->GetActiveCamera() : Dia::Camera2D::Camera2D{};
    const Dia::Maths::Vector2D&     windowSize = hasCam ? mCameraRef->GetWindowSize()   : Dia::Maths::Vector2D{1400.0f, 1000.0f};

    mLayerManager.SetViewport(camera, windowSize);

    mFrame.Clear();
    mFrame.SetCamera(camera);
    mFrame.SetWindowSize(windowSize);

    if (mInputRef.Get())
    {
        mFrame.SetMousePixel(Dia::Maths::Vector2D(
            static_cast<float>(mInputRef->GetMouseX()),
            static_cast<float>(mInputRef->GetMouseY())));

        // Tilde key: toggle DiaDebugPanel visibility
        if (mInputRef->WasKeyPressed(Dia::Input::EKey::Tilde))
        {
            mPanelToggle.Send(DebugPanelToggleEvent{});
            DIA_LOG_INFO("Debug", "VisualDebuggerModule: DiaDebugPanel toggle fired");
        }
    }

    mLayerManager.Draw(mFrame);
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult VisualDebuggerModule::DoStop()
{
    mLayerManager.ClearDynamicLayers();
    if (mCoord2DDomain) { UnregisterDomain(*mCoord2DDomain); mCoord2DDomain.reset(); }
    if (mCoord3DDomain) { UnregisterDomain(*mCoord3DDomain); mCoord3DDomain.reset(); }
    mLastKnownStage = Dia::Core::StringCRC();
    mFrame.Clear();
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
    return Dia::ApplicationFlow::StopResult::kDone;
}

void VisualDebuggerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRenderOutput.Connect(app);
    mLayerManagerService.Connect(app);
    mDomainRegistryService.Connect(app);
    mPanelCommands.Connect(app);
    mPanelToggle.Connect(app);
}

void VisualDebuggerModule::RegisterDomain(Dia::VisualDebugger::IDebugDomain& domain)
{
    mDomainRegistry.Register(domain);
    if (domain.HasWorldDrawers())
        domain.Register(mLayerManager);
}

void VisualDebuggerModule::UnregisterDomain(Dia::VisualDebugger::IDebugDomain& domain)
{
    if (domain.HasWorldDrawers())
        domain.Unregister(mLayerManager);
    mDomainRegistry.Unregister(domain);
}

void VisualDebuggerModule::EnqueueCommand(Dia::Core::StringCRC domainId,
                                          Dia::Core::StringCRC cmd,
                                          const char* argsJson)
{
    std::lock_guard<std::mutex> lock(mCommandQueueMutex);
    if (!mCommandQueue.IsFull())
    {
        PendingCommand pending{};
        pending.domainId = domainId;
        pending.cmd      = cmd;
        if (argsJson)
        {
            strncpy(pending.argsJson, argsJson, sizeof(pending.argsJson) - 1);
            pending.argsJson[sizeof(pending.argsJson) - 1] = '\0';
        }
        mCommandQueue.Add(pending);
    }
}

void VisualDebuggerModule::DrainPendingCommands()
{
    Dia::Core::Containers::DynamicArrayC<PendingCommand, kCommandQueueCapacity> localQueue;
    {
        std::lock_guard<std::mutex> lock(mCommandQueueMutex);
        for (unsigned int i = 0; i < mCommandQueue.Size(); ++i)
            localQueue.Add(mCommandQueue[i]);
        mCommandQueue.RemoveAll();
    }

    for (unsigned int i = 0; i < localQueue.Size(); ++i)
    {
        const PendingCommand& pending = localQueue[i];
        Dia::VisualDebugger::IDebugDomain* domain = mDomainRegistry.FindDomain(pending.domainId);
        if (!domain)
            continue;

        Json::Value args;
        Json::Reader reader;
        reader.parse(std::string(pending.argsJson), args);
        domain->OnCommand(pending.cmd, args);
    }
}

void VisualDebuggerModule::DrainPanelCommandStream()
{
    // DiaDebugPanel runs on the Main PU (Ultralight). ModuleRef cannot cross a
    // PU boundary, so its commands arrive here as events and are funnelled
    // through the same mutex-protected queue used by in-PU callers.
    Dia::Core::Containers::DynamicArrayC<
        Dia::ApplicationFlow::Event<DebugPanelCommandEvent>, kCommandQueueCapacity> events;
    mPanelCommands.Consume(events);

    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        const DebugPanelCommandEvent& payload = events[i].payload;
        EnqueueCommand(payload.domainId, payload.cmd, payload.argsJson);
    }
}

void VisualDebuggerModule::SetCamera3D(const Dia::Graphics3D::Camera3D& camera)
{
    mLayerManager.SetCamera3D(camera);
}

void VisualDebuggerModule::DrawCoord3D(Dia::Graphics3D::FrameData3D& frame)
{
    if (mCoord3DDomain)
        mCoord3DDomain->DrawCoord3D(frame);
}

} } // namespace Cluiche::AppFlow

namespace { using VisualDebuggerModule_ = Cluiche::AppFlow::VisualDebuggerModule; }
DIA_MODULE(VisualDebuggerModule_);
DIA_DESCRIBE(VisualDebuggerModule_::kTypeId, "Renders visual debug overlays: coordinate axes, grids, physics shapes, and velocity arrows.");

#endif // DIA_DEBUG
