#include "Modules/StageCatalogueModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
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
        DIA_LOG_INFO("STAGES", "Results read '%s': %s (%u stages)", path.c_str(), StageResultsReadResultName(readResult), mState.Results().Count());
    }

    Publish();   // once at start
    return Dia::ApplicationFlow::StartResult::kReady;
}

void StageCatalogueModule::DoUpdate(const Dia::SimTime::MainTimeContext& /*ctx*/)
{
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
