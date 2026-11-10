#include "Modules/VisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaDebugServer/SimHelpInfo.h>
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
#include <DiaVisualDebugger/Console/ConsoleExecutor.h>
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
    mSnapshotPublisher = std::make_unique<Dia::VisualDebugger::Console::DomainStateSnapshotPublisher>(
        mDomainRegistry, mSnapshotSink);
    mLayerManager.SetDebugScale(50.0f);
    mLayerManager.RegisterDiaAPICommands();
    mCoord2DDomain = std::make_unique<Dia::Debug::Coord2DDebugDomain>();
    RegisterDomain(*mCoord2DDomain);
    mCoord3DDomain = std::make_unique<Dia::Debug::Coord3DDebugDomain>();
    RegisterDomain(*mCoord3DDomain);
    return Dia::ApplicationFlow::StartResult::kReady;
}

void VisualDebuggerModule::SnapshotSink::Write(const Dia::VisualDebugger::Console::DomainStateSnapshot& snapshot)
{
    mOut.Write(snapshot, Dia::Core::TimeAbsolute::Zero());
}

void VisualDebuggerModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
    DIA_TRACE_ZONE("VisualDebuggerModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    if (!mPanelCommandRegistered && !mPanelCommandAttempted)
        TryRegisterPanelCommand();

    DrainPanelCommandStream();
    DrainConsoleLines();
    DrainPendingCommands();

    if (mSnapshotPublisher)
    {
        Dia::Core::Containers::DynamicArrayC<
            Dia::ApplicationFlow::Event<DomainStateInterestEvent>, kCommandQueueCapacity> interest;
        mDomainInterest.Consume(interest);
        for (unsigned int i = 0; i < interest.Size(); ++i)
            mSnapshotPublisher->Arm(interest[i].payload.windowMs);
        // Packing every domain costs several ms of SimPU time per call (Debug). The panel does not need more than
        // 30/kSnapshotTickDivisor updates per second, so publish every kSnapshotTickDivisor-th Sim tick only.
        if (ctx.tick % kSnapshotTickDivisor == 0)
        {
            mSnapshotPublisher->Update(ctx.tick);
        }
    }

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
    if (mPanelCommandRegistered)
    {
        if (SimQueryHostModule* host = mSimQueryHost.Get())
            host->UnregisterQuery(kPanelCommandName);
        mPanelCommandRegistered = false;
    }
    mPanelCommandAttempted = false;
    mLayerManager.ClearDynamicLayers();
    if (mCoord2DDomain) { UnregisterDomain(*mCoord2DDomain); mCoord2DDomain.reset(); }
    if (mCoord3DDomain) { UnregisterDomain(*mCoord3DDomain); mCoord3DDomain.reset(); }
    mSnapshotPublisher.reset();
    mDomainRegistry.Clear();
    mLastKnownStage = Dia::Core::StringCRC();
    mFrame.Clear();
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
    mLayerManagerService.Deregister();
    return Dia::ApplicationFlow::StopResult::kDone;
}

void VisualDebuggerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRenderOutput.Connect(app);
    mLayerManagerService.Connect(app);
    mPanelCommands.Connect(app);
    mConsoleLines.Connect(app);
    mConsoleResults.Connect(app);
    mPanelToggle.Connect(app);
    mPanelSetVisibility.Connect(app);
    mDomainSnapshotOut.Connect(app);
    mDomainInterest.Connect(app);
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

bool VisualDebuggerModule::EnqueueCommand(Dia::Core::StringCRC domainId,
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
        return true;
    }
    return false;
}

namespace
{
    Json::Value PanelCommandError(const char* message)
    {
        Json::Value r(Json::objectValue);
        r["success"] = false;
        r["error"] = message;
        return r;
    }
}

Json::Value VisualDebuggerModule::HandlePanelCommand(const Json::Value& args)
{
    if (!args.isObject())
        return PanelCommandError("args must be an object {domain, cmd, args?}");
    if (!args.isMember("domain") || !args["domain"].isString() || args["domain"].asString().empty())
        return PanelCommandError("'domain' is required and must be a non-empty string");
    if (!args.isMember("cmd") || !args["cmd"].isString() || args["cmd"].asString().empty())
        return PanelCommandError("'cmd' is required and must be a non-empty string");
    if (args.isMember("args") && !args["args"].isNull() && !args["args"].isObject())
        return PanelCommandError("'args' must be an object when given");

    const std::string domainName = args["domain"].asString();
    const Dia::Core::StringCRC domainId(domainName.c_str());
    if (mDomainRegistry.FindDomain(domainId) == nullptr)
        return PanelCommandError("unknown domain");

    std::string argsJson = "{}";
    if (args.isMember("args") && args["args"].isObject())
    {
        Json::FastWriter writer;
        argsJson = writer.write(args["args"]);
        // FastWriter appends a trailing line break; an object always ends with '}'.
        argsJson.erase(argsJson.find_last_of('}') + 1);
    }
    if (argsJson.size() >= sizeof(PendingCommand::argsJson))
        return PanelCommandError("args too large for the panel command queue");

    const std::string cmdName = args["cmd"].asString();
    if (!EnqueueCommand(domainId, Dia::Core::StringCRC(cmdName.c_str()), argsJson.c_str()))
        return PanelCommandError("panel command queue is full");

    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["queued"] = true;
    return r;
}

void VisualDebuggerModule::TryRegisterPanelCommand()
{
    SimQueryHostModule* host = mSimQueryHost.Get();
    if (!host)
        return;   // optional: no SimQueryHostModule (yet) - retried each tick until the host is active

    mPanelCommandAttempted = true;   // one attempt per start; a rejection is logged once

    static const Dia::API::ValueParam kParams[] = {
        { "domain", "string", true,  "Debug domain id as shown in the panel (e.g. WorldGen)" },
        { "cmd",    "string", true,  "Domain command name (e.g. setScale)" },
        { "args",   "object", false, "Command arguments object (under 512 bytes serialized)" } };

    Dia::DebugServer::SimHelpInfo help;
    help.description = "Send a debug-panel command to a registered debug domain, exactly as the panel UI does (applied on the next sim tick)";
    help.category = "debug";
    help.owner = "VisualDebuggerModule";
    help.params = kParams;
    help.paramCount = 3;
    help.noArgs = false;

    mPanelCommandRegistered = host->RegisterCommand(kPanelCommandName, help,
        [this](const Json::Value& a) { return HandlePanelCommand(a); });
    if (!mPanelCommandRegistered)
        DIA_LOG_ERROR("Debug", "VisualDebuggerModule: %s registration rejected by SimQueryHost", kPanelCommandName);
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

    const Dia::VisualDebugger::Console::ConsoleResult result =
        Dia::VisualDebugger::Console::ExecuteConsoleCommand(cmdText);
    const int exitCode = result.exitCode;

    DIA_LOG_INFO("Debug", "VisualDebuggerModule: console '%s' -> %d", cmdText, exitCode);
}

void VisualDebuggerModule::DrainConsoleLines()
{
    Dia::Core::Containers::DynamicArrayC<
        Dia::ApplicationFlow::Event<DebugConsoleLineEvent>, kCommandQueueCapacity> events;
    mConsoleLines.Consume(events);

    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        const DebugConsoleLineEvent& line = events[i].payload;

        DebugConsoleResultEvent out{};
        out.seq = line.seq;
        strncpy_s(out.text, line.text, sizeof(out.text) - 1);
        out.text[sizeof(out.text) - 1] = '\0';

        const Dia::VisualDebugger::Console::ConsoleResult result =
            Dia::VisualDebugger::Console::ExecuteConsoleCommand(out.text);
        out.exitCode   = static_cast<int32_t>(result.exitCode);
        out.parseError = static_cast<uint8_t>(result.parseError);

        DIA_LOG_INFO("Debug", "VisualDebuggerModule: console '%s' -> %d", out.text, out.exitCode);
        mConsoleResults.Send(out);
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
