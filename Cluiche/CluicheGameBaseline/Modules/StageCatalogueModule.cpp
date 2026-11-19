#include "Modules/StageCatalogueModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/Streams/ServiceStreamStore.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <Modules/DebugServerHostModule.h>
#include <Modules/StageCatalogueQueries.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaStreams/Event.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC StageCatalogueModule::kTypeId("StageCatalogueModule");

namespace {

std::string ResolvePath(const char* raw)
{
#ifdef _WIN32
    char absPath[512] = {};
    if (GetFullPathNameA(raw, sizeof(absPath), absPath, nullptr) > 0) { return std::string(absPath); }
#endif
    return std::string(raw);
}

Dia::DebugServer::QueryRegistry* ResolveQueryRegistry(Dia::ApplicationFlow::IApplicationInspectable* app)
{
    if (app == nullptr) { return nullptr; }
    auto* store = app->FindStream(Dia::Core::StringCRC(DebugServerHostModule::kServiceStreamId));
    // Get() asserts until the framework commits the stream (after every provider's DoStart): DoUpdate retries.
    if (store == nullptr || !store->IsCommitted()) { return nullptr; }
    auto* typed = static_cast<Dia::ApplicationFlow::ServiceStreamStore<Dia::DebugServer::DebugServer*>*>(store);
    Dia::DebugServer::DebugServer* server = typed->Get();
    return server ? &server->GetQueryRegistry() : nullptr;
}

} // namespace

StageCatalogueModule::StageCatalogueModule(const Dia::Core::StringCRC& instanceId)
    : MainModule(instanceId)
    , mSnapshot(new StageCatalogueSnapshot())
    , mHealth(*this)
    , mResultsIn(this, Dia::Core::StringCRC(kResultStreamId))
    , mSnapshotOut(this, Dia::Core::StringCRC(kSnapshotStreamId))
{
}

StageCatalogueModule::~StageCatalogueModule() = default;

void StageCatalogueModule::OnConfigure(const char* configJson)
{
    if (configJson == nullptr || configJson[0] == '\0') { return; }
    Json::Value config;
    Json::Reader reader;
    if (!reader.parse(configJson, config))
    {
        DIA_LOG_WARNING("STAGES", "StageCatalogueModule: OnConfigure JSON parse failed");
        return;
    }
    if (config.isMember("diagame_path") && config["diagame_path"].isString())
        mDiagamePath = ResolvePath(config["diagame_path"].asCString());
    if (config.isMember("results_path") && config["results_path"].isString())
        mConfiguredResultsPath = ResolvePath(config["results_path"].asCString());
}

void StageCatalogueModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mResultsIn.Connect(app);
    mSnapshotOut.Connect(app);
    mAppInspect = app.GetInspectable();
}

void StageCatalogueModule::RegisterQueries()
{
    if (mQueriesOn != nullptr) { return; }
    Dia::DebugServer::QueryRegistry* registry = mQueryRegistryOverride ? mQueryRegistryOverride : ResolveQueryRegistry(mAppInspect);
    if (registry == nullptr) { return; }
    // Help text lives in StageCatalogueQueries::k*Help (main-hosted queries have no registry help channel).
    const StageCatalogueState* state = &mState;
    registry->Register(Dia::Core::StringCRC(StageCatalogueQueries::kListName),
        [state](const Json::Value& args) { return StageCatalogueQueries::List(*state, args); });
    registry->Register(Dia::Core::StringCRC(StageCatalogueQueries::kGetName),
        [state](const Json::Value& args) { return StageCatalogueQueries::Get(*state, args); });
    registry->Register(Dia::Core::StringCRC(StageCatalogueQueries::kResultsName),
        [state](const Json::Value& args) { return StageCatalogueQueries::Results(*state, args); });
    mQueriesOn = registry;
    DIA_LOG_INFO("STAGES", "Registered queries stages.list, stages.get, stages.results");
}

void StageCatalogueModule::UnregisterQueries()
{
    if (mQueriesOn == nullptr) { return; }
    mQueriesOn->Unregister(Dia::Core::StringCRC(StageCatalogueQueries::kListName));
    mQueriesOn->Unregister(Dia::Core::StringCRC(StageCatalogueQueries::kGetName));
    mQueriesOn->Unregister(Dia::Core::StringCRC(StageCatalogueQueries::kResultsName));
    mQueriesOn = nullptr;
}

bool StageCatalogueModule::GetSessionInfo(std::string&, std::string&) const
{
    return false;
}

Dia::ApplicationFlow::StartResult StageCatalogueModule::DoStart()
{
    auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
    mMetricCount = metrics.RegisterGauge(Dia::Core::StringCRC("stages.catalogue.count"));
    mMetricWriteFail = metrics.RegisterCounter(Dia::Core::StringCRC("stages.results.write_fail"));
    Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
    mHealthRegistered = true;

    // Catalogue: errors are logged and the module runs with an empty catalogue.
    const StageCatalogueResult catResult = mState.LoadCatalogue(mDiagamePath.c_str());
    if (catResult == StageCatalogueResult::kOk)
    {
        DIA_LOG_INFO("STAGES", "Catalogue loaded: %u stages, omitted %u, dropped %u",
            mState.Catalogue().Size(), mState.Catalogue().OmittedCount(), mState.Catalogue().DroppedCount());
    }
    else
    {
        DIA_LOG_ERROR("STAGES", "Catalogue load failed (%s) for '%s'; running with an empty catalogue",
            StageCatalogueResultName(catResult), mDiagamePath.c_str());
    }
    if (mMetricCount) { mMetricCount->Set(static_cast<double>(mState.Catalogue().Size())); }

    // Results path: config, else derived from the observation session, else no persistence (one Warning).
    std::string path = mConfiguredResultsPath;
    if (path.empty())
    {
        std::string id, dir;
        if (GetSessionInfo(id, dir)) { path = DeriveResultsPathFromSessionDir(dir.c_str()); }
    }
    mState.SetResultsPath(path.c_str());
    if (path.empty())
    {
        DIA_LOG_WARNING("STAGES", "No results_path and no observation session: stage results run without persistence");
    }
    else
    {
        const StageResultsReadResult readResult = mState.LoadResults(path.c_str());
        if (readResult == StageResultsReadResult::kOk || readResult == StageResultsReadResult::kMissing)
        {
            DIA_LOG_INFO("STAGES", "Results read '%s': %s (%u stages)", path.c_str(), StageResultsReadResultName(readResult), mState.Results().Count());
        }
        else
        {
            DIA_LOG_WARNING("STAGES", "Results read '%s' failed: %s; starting with an empty table", path.c_str(), StageResultsReadResultName(readResult));
        }
    }

    Publish();   // once at start
    RegisterQueries();   // no-op until the DebugServer stream is committed; DoUpdate retries
    return Dia::ApplicationFlow::StartResult::kReady;
}

void StageCatalogueModule::DoUpdate(const Dia::SimTime::MainTimeContext& /*ctx*/)
{
    if (mQueriesOn == nullptr) { RegisterQueries(); }
    Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<StageResultEvent>, 16> events;
    mResultsIn.Consume(events);
    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        ProcessEvents(&events[i].payload, 1);
    }
    EndFrame();
}

void StageCatalogueModule::ProcessEvents(const StageResultEvent* events, unsigned int count)
{
    std::string sessionId, sessionDir;
    const bool haveSession = GetSessionInfo(sessionId, sessionDir);
    const std::string logPath = haveSession ? sessionDir + "log.jsonl" : std::string();
    for (unsigned int i = 0; i < count; ++i)
    {
        DIA_LOG_INFO("STAGES", "Result received: stage=%s severity=%s", events[i].stage.AsChar(), StageResultSeverityName(events[i].result.severity));
        if (!mState.ApplyEvent(events[i], sessionId.c_str(), logPath.c_str()))
        {
            DIA_LOG_WARNING("STAGES", "Result for stage '%s' rejected (table full or empty name)", events[i].stage.AsChar());
        }
    }
}

void StageCatalogueModule::EndFrame()
{
    if (mState.FlushFrame() == StageCatalogueState::FlushResult::kFailed)
    {
        DIA_LOG_ERROR("STAGES", "Results write failed: '%s'", mState.ResultsPath());
        if (mMetricWriteFail) { mMetricWriteFail->Inc(); }
    }
    if (mState.NeedsPublish()) { Publish(); }
}

void StageCatalogueModule::Publish()
{
    mState.BuildSnapshot(*mSnapshot);
    mSnapshotOut.Write(*mSnapshot, Dia::Core::TimeAbsolute::Zero());
    mState.MarkPublished();
    ++mPublishCount;
}

Dia::ApplicationFlow::StopResult StageCatalogueModule::DoStop()
{
    UnregisterQueries();
    if (mHealthRegistered)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        mHealthRegistered = false;
    }
    mMetricCount = nullptr;
    mMetricWriteFail = nullptr;
    return Dia::ApplicationFlow::StopResult::kDone;
}

Dia::Core::StringCRC StageCatalogueModule::HealthReporter::GetReporterName() const
{
    return Dia::Core::StringCRC("StageCatalogue");
}

Dia::Observation::Health::Health StageCatalogueModule::HealthReporter::Report() const
{
    Dia::Observation::Health::Health h;
    h.errors = 0;
    h.warnings = 0;
    if (mOwner.mState.LastWriteFailed())
    {
        h.status = Dia::Observation::Health::HealthStatus::kDegraded;
        h.warnings = 1;
        h.reason = Dia::Core::StringCRC("results write failed");
    }
    else
    {
        h.status = Dia::Observation::Health::HealthStatus::kOK;
    }
    return h;
}

}} // namespace Cluiche::AppFlow

namespace { using StageCatalogueModule_ = Cluiche::AppFlow::StageCatalogueModule; }
DIA_MODULE(StageCatalogueModule_);
DIA_DESCRIBE(StageCatalogueModule_::kTypeId, "Owns the stage catalogue and last stage results; persists results and publishes a catalogue snapshot.");
