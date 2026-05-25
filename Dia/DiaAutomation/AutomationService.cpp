////////////////////////////////////////////////////////////////////////////////
// Filename: AutomationService.cpp
// DiaAutomation — AutomationService implementation
////////////////////////////////////////////////////////////////////////////////
#include "AutomationService.h"
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Histogram.h>
#include <DiaCore/Core/Assert.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace Automation {

    AutomationService::AutomationService(Dia::ApplicationFlow::Application& app)
        : mApp(app)
    {
    }

    AutomationService::~AutomationService()
    {
        // Detach lifecycle tap if attached
        if (mLifecycleTap.id != 0)
        {
            using namespace Dia::ApplicationFlow;
            IStreamStore* istore = mApp.FindStream(Dia::Core::StringCRC("$lifecycle"));
            if (istore)
            {
                auto* store = static_cast<EventStreamStore<LifecycleEvent>*>(istore);
                store->DetachTap(mLifecycleTap);
            }
            mLifecycleTap = TapHandle{0};
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Checkpoint Registry
    ////////////////////////////////////////////////////////////////////////////////

    void AutomationService::RegisterCheckpoint(Dia::ApplicationFlow::Module* owner,
                                                const Dia::Core::StringCRC& name,
                                                CheckpointFn fn)
    {
        if (HasCheckpoint(name))
        {
            DIA_LOG_ERROR("Automation", "RegisterCheckpoint: duplicate name '%s' — ignored", name.AsChar());
            return;
        }
        CheckpointEntry entry;
        entry.owner = owner;
        entry.name  = name;
        entry.fn    = fn;
        mCheckpoints.Add(entry);
        DIA_LOG_INFO("Automation", "checkpoint.registered name=%s", name.AsChar());
    }

    void AutomationService::UnregisterCheckpoints(Dia::ApplicationFlow::Module* owner)
    {
        for (unsigned int i = mCheckpoints.Size(); i > 0; --i)
        {
            if (mCheckpoints[i - 1].owner == owner)
            {
                DIA_LOG_INFO("Automation", "checkpoint.unregistered name=%s",
                             mCheckpoints[i - 1].name.AsChar());
                mCheckpoints.RemoveAt(i - 1);
            }
        }
    }

    CheckpointResult AutomationService::RunCheckpoint(const Dia::Core::StringCRC& name) const
    {
        for (unsigned int i = 0; i < mCheckpoints.Size(); ++i)
        {
            if (mCheckpoints[i].name == name)
                return mCheckpoints[i].fn();
        }
        CheckpointResult result;
        result.passed    = false;
        result.message   = "checkpoint not found";
        result.durationMs = 0.0f;
        return result;
    }

    bool AutomationService::HasCheckpoint(const Dia::Core::StringCRC& name) const
    {
        for (unsigned int i = 0; i < mCheckpoints.Size(); ++i)
        {
            if (mCheckpoints[i].name == name)
                return true;
        }
        return false;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Pause / Resume
    ////////////////////////////////////////////////////////////////////////////////

    void AutomationService::RegisterPauseCallback(Dia::ApplicationFlow::Module* owner,
                                                   PauseResumeFn pause,
                                                   PauseResumeFn resume)
    {
        PauseResumeEntry entry;
        entry.owner  = owner;
        entry.pause  = pause;
        entry.resume = resume;
        mPauseCallbacks.Add(entry);
    }

    void AutomationService::UnregisterPauseCallbacks(Dia::ApplicationFlow::Module* owner)
    {
        for (unsigned int i = mPauseCallbacks.Size(); i > 0; --i)
        {
            if (mPauseCallbacks[i - 1].owner == owner)
                mPauseCallbacks.RemoveAt(i - 1);
        }
    }

    void AutomationService::Pause()
    {
        if (mPaused)
            return;
        mPaused = true;
        for (unsigned int i = 0; i < mPauseCallbacks.Size(); ++i)
        {
            if (mPauseCallbacks[i].pause)
                mPauseCallbacks[i].pause();
        }
        Dia::ApplicationFlow::LifecycleEvent ev;
        ev.kind = Dia::ApplicationFlow::LifecycleEventKind::kAutomationPaused;
        mApp.EmitLifecycleEvent(ev);
        DIA_LOG_INFO("Automation", "automation.paused");
    }

    void AutomationService::Resume()
    {
        if (!mPaused)
            return;
        mPaused = false;
        for (unsigned int i = 0; i < mPauseCallbacks.Size(); ++i)
        {
            if (mPauseCallbacks[i].resume)
                mPauseCallbacks[i].resume();
        }
        Dia::ApplicationFlow::LifecycleEvent ev;
        ev.kind = Dia::ApplicationFlow::LifecycleEventKind::kAutomationResumed;
        mApp.EmitLifecycleEvent(ev);
        DIA_LOG_INFO("Automation", "automation.resumed");
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Navigation Hold
    ////////////////////////////////////////////////////////////////////////////////

    void AutomationService::EnableNavigationHold()
    {
        if (mHoldRegistered)
            return;
        mHolding = true;
        mHoldRegistered = true;

        // Register the guard — lambda captures this
        mApp.RegisterTransitionGuard(nullptr,
            [this]() -> Dia::ApplicationFlow::GuardResult {
                return mHolding
                    ? Dia::ApplicationFlow::GuardResult::Hold
                    : Dia::ApplicationFlow::GuardResult::Allow;
            });

        // Attach tap on $lifecycle to re-arm hold after each kStageTransitionCommitted
        using namespace Dia::ApplicationFlow;
        IStreamStore* istore = mApp.FindStream(Dia::Core::StringCRC("$lifecycle"));
        if (istore)
        {
            auto* store = static_cast<EventStreamStore<LifecycleEvent>*>(istore);
            mLifecycleTap = store->AttachTap(
                [this](const void* payload, unsigned int /*size*/, const Dia::Core::StringCRC& /*id*/)
                {
                    const auto* ev = static_cast<const LifecycleEvent*>(payload);
                    if (ev->kind == LifecycleEventKind::kStageTransitionCommitted)
                    {
                        mHolding = true;
                    }
                });
        }

        LifecycleEvent ev;
        ev.kind = LifecycleEventKind::kAutomationHoldEnabled;
        mApp.EmitLifecycleEvent(ev);
        DIA_LOG_INFO("Automation", "automation.hold.enabled");
    }

    void AutomationService::ReleaseNavigationHold(const Dia::Core::StringCRC& target,
                                                    bool* outSuccess,
                                                    const char** outError)
    {
        if (!mHolding)
        {
            DIA_LOG_WARNING("Automation", "ReleaseNavigationHold: not currently holding");
            if (outSuccess) *outSuccess = true;
            return;
        }

        // Validate target stage
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> stages;
        mApp.GetAllStages(stages);
        bool found = false;
        for (unsigned int i = 0; i < stages.Size(); ++i)
        {
            if (stages[i] == target)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            DIA_LOG_ERROR("Automation", "ReleaseNavigationHold: unknown stage '%s'", target.AsChar());
            if (outSuccess) *outSuccess = false;
            if (outError)   *outError   = "unknown stage";
            return;
        }

        mHolding = false;
        mApp.TransitionTo(target);

        Dia::ApplicationFlow::LifecycleEvent ev;
        ev.kind    = Dia::ApplicationFlow::LifecycleEventKind::kAutomationHoldReleased;
        ev.toStage = target;
        mApp.EmitLifecycleEvent(ev);
        DIA_LOG_INFO("Automation", "automation.hold.released target=%s", target.AsChar());

        if (outSuccess) *outSuccess = true;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CI Safety
    ////////////////////////////////////////////////////////////////////////////////

    void AutomationService::EnableHeartbeatMonitor(float timeoutSeconds)
    {
        mHeartbeatTimeout = timeoutSeconds;
        mHeartbeatElapsed = 0.0f;
        mHeartbeatEnabled = true;
        DIA_LOG_INFO("Automation", "automation.heartbeat.enabled timeout=%.1fs", timeoutSeconds);
    }

    void AutomationService::DisableHeartbeatMonitor()
    {
        mHeartbeatEnabled = false;
        DIA_LOG_INFO("Automation", "automation.heartbeat.disabled");
    }

    void AutomationService::ResetHeartbeat()
    {
        mHeartbeatElapsed = 0.0f;
    }

    void AutomationService::TickHeartbeat(float deltaTime)
    {
        if (!mHeartbeatEnabled)
            return;
        mHeartbeatElapsed += deltaTime;
        if (mHeartbeatElapsed >= mHeartbeatTimeout)
        {
            mHeartbeatEnabled = false;
            Dia::ApplicationFlow::LifecycleEvent ev;
            ev.kind = Dia::ApplicationFlow::LifecycleEventKind::kAutomationHeartbeatTimeout;
            mApp.EmitLifecycleEvent(ev);
            DIA_LOG_WARNING("Automation", "automation.heartbeat.timeout");
            OnDisconnect();
        }
    }

    void AutomationService::OnDisconnect()
    {
        if (mApp.IsShuttingDown())
            return;

        Dia::ApplicationFlow::LifecycleEvent ev;
        ev.kind = Dia::ApplicationFlow::LifecycleEventKind::kAutomationDisconnect;
        mApp.EmitLifecycleEvent(ev);
        DIA_LOG_WARNING("Automation", "automation.disconnect — releasing hold, requesting shutdown");

        mHolding = false;
        mApp.RequestShutdown();
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Command Registration
    ////////////////////////////////////////////////////////////////////////////////

    void AutomationService::RegisterCommands()
    {
        // dia.automation.navigate_to
        {
            Dia::API::CommandInfoJson cmd;
            cmd.name        = Dia::Core::StringCRC("dia.automation.navigate_to");
            cmd.description = "Release navigation hold and transition to target stage";
            cmd.category    = Dia::Core::StringCRC("dia.automation");
            cmd.owner       = "DiaAutomation";
            cmd.callback    = [this](const Json::Value& params) -> Json::Value {
                ResetHeartbeat();
                if (!params.isMember("target") || !params["target"].isString())
                {
                    Json::Value err;
                    err["error"] = "missing 'target' parameter";
                    return err;
                }
                Dia::Core::StringCRC target(params["target"].asCString());
                bool success = false;
                const char* errMsg = nullptr;
                ReleaseNavigationHold(target, &success, &errMsg);
                if (!success)
                {
                    Json::Value err;
                    char buf[128];
                    snprintf(buf, sizeof(buf), "unknown stage: '%s'", target.AsChar());
                    err["error"] = buf;
                    return err;
                }
                return Json::Value(Json::objectValue);
            };
            Dia::API::RegisterCommandJson(cmd);
        }
        // dia.automation.pause
        {
            Dia::API::CommandInfoJson cmd;
            cmd.name        = Dia::Core::StringCRC("dia.automation.pause");
            cmd.description = "Pause all registered pause callbacks";
            cmd.category    = Dia::Core::StringCRC("dia.automation");
            cmd.owner       = "DiaAutomation";
            cmd.callback    = [this](const Json::Value&) -> Json::Value {
                ResetHeartbeat();
                Pause();
                Json::Value data;
                data["paused"] = true;
                return data;
            };
            Dia::API::RegisterCommandJson(cmd);
        }
        // dia.automation.resume
        {
            Dia::API::CommandInfoJson cmd;
            cmd.name        = Dia::Core::StringCRC("dia.automation.resume");
            cmd.description = "Resume all registered resume callbacks";
            cmd.category    = Dia::Core::StringCRC("dia.automation");
            cmd.owner       = "DiaAutomation";
            cmd.callback    = [this](const Json::Value&) -> Json::Value {
                ResetHeartbeat();
                Resume();
                Json::Value data;
                data["resumed"] = true;
                return data;
            };
            Dia::API::RegisterCommandJson(cmd);
        }
        // dia.automation.validate
        {
            Dia::API::CommandInfoJson cmd;
            cmd.name        = Dia::Core::StringCRC("dia.automation.validate");
            cmd.description = "Run a named checkpoint and return pass/fail result";
            cmd.category    = Dia::Core::StringCRC("dia.automation");
            cmd.owner       = "DiaAutomation";
            cmd.callback    = [this](const Json::Value& params) -> Json::Value {
                ResetHeartbeat();
                if (!params.isMember("checkpoint") || !params["checkpoint"].isString())
                {
                    Json::Value err;
                    err["error"] = "missing 'checkpoint' parameter";
                    return err;
                }
                Dia::Core::StringCRC name(params["checkpoint"].asCString());
                if (!HasCheckpoint(name))
                {
                    Json::Value err;
                    char buf[128];
                    snprintf(buf, sizeof(buf), "checkpoint not found: '%s'", name.AsChar());
                    err["error"] = buf;
                    return err;
                }
                CheckpointResult result = RunCheckpoint(name);
                Json::Value data;
                data["passed"]      = result.passed;
                data["message"]     = result.message ? result.message : "";
                data["duration_ms"] = result.durationMs;
                return data;
            };
            Dia::API::RegisterCommandJson(cmd);
        }
        // dia.automation.get_metric
        {
            Dia::API::CommandInfoJson cmd;
            cmd.name        = Dia::Core::StringCRC("dia.automation.get_metric");
            cmd.description = "Query a live metric value by name";
            cmd.category    = Dia::Core::StringCRC("dia.automation");
            cmd.owner       = "DiaAutomation";
            cmd.callback    = [this](const Json::Value& params) -> Json::Value {
                ResetHeartbeat();
                if (!params.isMember("name") || !params["name"].isString())
                {
                    Json::Value err;
                    err["error"] = "missing 'name' parameter";
                    return err;
                }
                const char* name = params["name"].asCString();
                Dia::Core::StringCRC metricId(name);

                using namespace Dia::Observation::Metric;
                MetricRegistry& registry = MetricRegistry::Instance();

                Counter*   counter   = registry.FindCounter(metricId);
                Gauge*     gauge     = registry.FindGauge(metricId);
                Histogram* histogram = registry.FindHistogram(metricId);

                if (!counter && !gauge && !histogram)
                {
                    Json::Value err;
                    char buf[256];
                    snprintf(buf, sizeof(buf), "metric not found: '%s'", name);
                    err["error"] = buf;
                    return err;
                }

                Json::Value data;
                if (counter)
                {
                    data["type"]  = "counter";
                    data["value"] = static_cast<Json::UInt64>(counter->Value());
                }
                else if (gauge)
                {
                    data["type"]  = "gauge";
                    data["value"] = gauge->Value();
                }
                else
                {
                    data["type"] = "histogram";

                    Histogram::Data hdata;
                    histogram->ReadData(hdata);

                    double mean = (hdata.total > 0)
                        ? hdata.sum / static_cast<double>(hdata.total)
                        : 0.0;

                    // Approximate min/max from bucket bounds
                    double minVal = 0.0;
                    double maxVal = 0.0;
                    if (hdata.total > 0 && hdata.bucketCount > 0)
                    {
                        for (unsigned int i = 0; i < hdata.bucketCount; ++i)
                        {
                            if (hdata.counts[i] > 0)
                            {
                                minVal = (i == 0) ? 0.0 : static_cast<double>(hdata.bounds[i - 1]);
                                break;
                            }
                        }
                        for (unsigned int i = hdata.bucketCount; i > 0; --i)
                        {
                            if (hdata.counts[i - 1] > 0)
                            {
                                maxVal = static_cast<double>(hdata.bounds[i - 1]);
                                break;
                            }
                        }
                        if (hdata.counts[hdata.bucketCount] > 0)
                            maxVal = maxVal > 0.0 ? maxVal * 2.0 : static_cast<double>(hdata.bounds[hdata.bucketCount - 1]) * 2.0;
                    }

                    Json::Value value;
                    value["count"] = static_cast<Json::UInt64>(hdata.total);
                    value["sum"]   = hdata.sum;
                    value["min"]   = minVal;
                    value["max"]   = maxVal;
                    value["mean"]  = mean;
                    data["value"]  = value;
                }
                return data;
            };
            Dia::API::RegisterCommandJson(cmd);
        }
    }

}} // namespace Dia::Automation
