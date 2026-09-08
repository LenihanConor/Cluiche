#include "Modules/VisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
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
    : SimModule(instanceId)
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

void VisualDebuggerModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
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
    mDomainRegistry.Clear();
    mLastKnownStage = Dia::Core::StringCRC();
    mFrame.Clear();
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
    mLayerManagerService.Deregister();
    mDomainRegistryService.Deregister();
    return Dia::ApplicationFlow::StopResult::kDone;
}

void VisualDebuggerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRenderOutput.Connect(app);
    mLayerManagerService.Connect(app);
    mDomainRegistryService.Connect(app);
    mPanelCommands.Connect(app);
    mPanelToggle.Connect(app);
    mPanelSetVisibility.Connect(app);
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
            strncpy_s(pending.argsJson, argsJson, sizeof(pending.argsJson) - 1);
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

    static const Dia::Core::StringCRC kGlobalDomain("__global__");
    static const Dia::Core::StringCRC kConsoleDomain("__console__");
    static const Dia::Core::StringCRC kPanelDomain("__panel__");
    static const Dia::Core::StringCRC kLogDomain("__log__");
    static const Dia::Core::StringCRC kSetScaleCmd("setScale");
    static const Dia::Core::StringCRC kExecCmd("exec");
    static const Dia::Core::StringCRC kLockEntityCmd("lockEntity");
    static const Dia::Core::StringCRC kInfoCmd("info");

    for (unsigned int i = 0; i < localQueue.Size(); ++i)
    {
        const PendingCommand& pending = localQueue[i];

        // JS log bridge: debug-panel.html calls sendCommand('__log__','info',{msg:'...'})
        if (pending.domainId == kLogDomain && pending.cmd == kInfoCmd)
        {
            Json::Value args;
            Json::Reader reader;
            reader.parse(std::string(pending.argsJson), args);
            const std::string msg = args.get("msg", "").asString();
            DIA_LOG_INFO("DebugPanelJS", "%s", msg.c_str());
            continue;
        }

        // Panel-wide: global debug scale slider
        if (pending.domainId == kGlobalDomain && pending.cmd == kSetScaleCmd)
        {
            Json::Value args;
            Json::Reader reader;
            reader.parse(std::string(pending.argsJson), args);
            if (args.isMember("value"))
                mLayerManager.SetDebugScale(args["value"].asFloat());
            continue;
        }

        // Command strip: execute a debug console command
        if (pending.domainId == kConsoleDomain && pending.cmd == kExecCmd)
        {
            Json::Value args;
            Json::Reader reader;
            reader.parse(std::string(pending.argsJson), args);
            const std::string text = args.get("text", "").asString();
            if (!text.empty())
                ExecuteConsoleCommand(text.c_str());
            continue;
        }

        // Entity-lock: store the locked entity ID for future domain filtering
        if (pending.domainId == kPanelDomain && pending.cmd == kLockEntityCmd)
        {
            Json::Value args;
            Json::Reader reader;
            reader.parse(std::string(pending.argsJson), args);
            const std::string id = args.get("id", "").asString();
            strncpy_s(mLockedEntityId, id.c_str(), sizeof(mLockedEntityId) - 1);
            mLockedEntityId[sizeof(mLockedEntityId) - 1] = '\0';
            DIA_LOG_INFO("Debug", "VisualDebuggerModule: entity lock -> '%s'", mLockedEntityId);
            continue;
        }

        // Domain command: dispatch to IDebugDomain
        Dia::VisualDebugger::IDebugDomain* domain = mDomainRegistry.FindDomain(pending.domainId);
        if (!domain)
            continue;

        Json::Value args;
        Json::Reader reader;
        reader.parse(std::string(pending.argsJson), args);
        domain->OnCommand(pending.cmd, args);
    }
}

void VisualDebuggerModule::ExecuteConsoleCommand(const char* cmdText)
{
    if (cmdText == nullptr || cmdText[0] == '\0')
        return;

    // Split on spaces: first token = command name, remaining = positional args.
    // Pointers into buf are valid for the duration of this call (synchronous).
    char buf[512] = {};
    strncpy_s(buf, cmdText, sizeof(buf) - 1);

    const char* commandName = nullptr;
    Dia::API::CommandArgs cmdArgs;

    char* p = buf;
    while (*p != '\0')
    {
        while (*p == ' ') ++p;
        if (*p == '\0') break;
        char* token = p;
        while (*p != '\0' && *p != ' ') ++p;
        if (*p != '\0') { *p = '\0'; ++p; }

        if (commandName == nullptr)
            commandName = token;
        else if (!cmdArgs.positionalArgs.IsFull())
            cmdArgs.positionalArgs.Add(token);
    }

    if (commandName == nullptr)
        return;

    const int exitCode = Dia::API::ExecuteCommand(
        Dia::Core::StringCRC(commandName), cmdArgs);

    DIA_LOG_INFO("Debug", "VisualDebuggerModule: console '%s' -> %d", cmdText, exitCode);
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
