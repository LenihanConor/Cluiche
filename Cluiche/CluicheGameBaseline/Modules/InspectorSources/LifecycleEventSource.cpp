#include "Modules/InspectorSources/LifecycleEventSource.h"
#include <DiaApplicationFlow/LifecycleEvent.h>
#include <DiaObservation/Log/DiaLog.h>

namespace {
    const char* ModuleStateName(Dia::ApplicationFlow::ModuleState s)
    {
        using Dia::ApplicationFlow::ModuleState;
        switch (s)
        {
            case ModuleState::kInactive: return "inactive";
            case ModuleState::kStarting: return "starting";
            case ModuleState::kActive:   return "active";
            case ModuleState::kStopping: return "stopping";
            case ModuleState::kFailed:   return "failed";
        }
        return "unknown";
    }
}

namespace Cluiche { namespace AppFlow {

LifecycleEventSource::LifecycleEventSource(
    Dia::ApplicationFlow::IApplicationInspectable* app,
    const double* uptimeSecs)
    : mApp(app)
    , mUptimeSecs(uptimeSecs)
{
}

Dia::Core::StringCRC LifecycleEventSource::GetTopic() const
{
    static const Dia::Core::StringCRC kTopic("app.event");
    return kTopic;
}

Dia::DebugServer::SourcePolicy LifecycleEventSource::GetPolicy() const
{
    return { Dia::DebugServer::SourceStrategy::kEventDriven, 0.0f, 0, 0.0f };
}

void LifecycleEventSource::Activate(Dia::DebugServer::DebugServer* server)
{
    EventDrivenSourceBase::Activate(server);

    if (!mApp) return;
    auto* lifecycleStore = mApp->FindStream(Dia::Core::StringCRC("$lifecycle"));
    if (!lifecycleStore)
    {
        DIA_LOG_WARNING("DebugServer", "LifecycleEventSource::Activate - $lifecycle stream not found");
        return;
    }

    auto handle = lifecycleStore->AttachTap(
        [this](const void* bytes, unsigned int size, const Dia::Core::StringCRC& /*streamId*/)
        {
            if (!bytes || size < sizeof(Dia::ApplicationFlow::LifecycleEvent)) return;
            const auto* evt = static_cast<const Dia::ApplicationFlow::LifecycleEvent*>(bytes);

            Json::Value eventPayload;
            double uptimeMs = mUptimeSecs ? (*mUptimeSecs * 1000.0) : 0.0;

            switch (evt->kind)
            {
            case Dia::ApplicationFlow::LifecycleEventKind::kStageTransitionCommitted:
                if (auto* s = GetServer())
                    s->BroadcastStageTransition(evt->fromStage, evt->toStage);
                eventPayload["severity"] = "transition";
                eventPayload["message"]  = std::string("Stage: ") + evt->fromStage.AsChar()
                                         + " -> " + evt->toStage.AsChar();
                break;
            case Dia::ApplicationFlow::LifecycleEventKind::kStageTransitionStarted:
                eventPayload["severity"] = "info";
                eventPayload["message"]  = std::string("Transition starting: ") + evt->fromStage.AsChar()
                                         + " -> " + evt->toStage.AsChar();
                break;
            case Dia::ApplicationFlow::LifecycleEventKind::kModuleStateChanged:
                eventPayload["severity"] = (evt->newState == Dia::ApplicationFlow::ModuleState::kFailed)
                                         ? "error" : "info";
                eventPayload["message"]  = std::string("Module ") + evt->moduleInstanceId.AsChar()
                                         + ": " + ModuleStateName(evt->newState);
                break;
            case Dia::ApplicationFlow::LifecycleEventKind::kShutdownRequested:
                eventPayload["severity"] = "warn";
                eventPayload["message"]  = "Shutdown requested";
                break;
            default:
                return;
            }

            eventPayload["timestampMs"] = static_cast<Json::UInt64>(uptimeMs);
            Push(eventPayload);
        });

    mTapId = handle.id;
    if (mTapId == 0)
        DIA_LOG_WARNING("DebugServer", "LifecycleEventSource::Activate - $lifecycle tap attach failed");
}

void LifecycleEventSource::Deactivate()
{
    if (mTapId != 0 && mApp)
    {
        auto* lifecycleStore = mApp->FindStream(Dia::Core::StringCRC("$lifecycle"));
        if (lifecycleStore)
            lifecycleStore->DetachTap(Dia::ApplicationFlow::TapHandle{mTapId});
        mTapId = 0;
    }
    EventDrivenSourceBase::Deactivate();
}

}} // namespace Cluiche::AppFlow
