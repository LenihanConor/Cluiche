#pragma once

#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaObservation/Health/IHealthReporter.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/StreamWriter.h>
#include <Modules/StageCatalogueState.h>

#include <memory>
#include <string>

namespace Dia { namespace Observation { namespace Metric { class Gauge; class Counter; } } }
namespace Dia { namespace DebugServer { class QueryRegistry; } }
namespace Dia { namespace ApplicationFlow { class IApplicationInspectable; } }

namespace Cluiche { namespace AppFlow {

// Owns the stage catalogue and the last result per stage (StageCatalogue F1, AC 11-14, 16).
// Config: diagame_path (required), results_path (optional; default <session>/../../stage_results.json).
// Reads the StageResult EventStream (SimPU -> MainPU), writes the results file at most once per frame and publishes
// StageCatalogueSnapshot (MainPU -> RenderPU) when the revision changes.
class StageCatalogueModule : public Dia::ApplicationFlow::MainModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Stage catalogue, last results file and catalogue snapshot";

    static constexpr const char* kResultStreamId = "StageResult";
    static constexpr const char* kSnapshotStreamId = "StageCatalogueSnapshot";

    explicit StageCatalogueModule(const Dia::Core::StringCRC& instanceId);
    ~StageCatalogueModule() override;

    const StageCatalogueState& State() const { return mState; }
    uint32_t PublishCount() const { return mPublishCount; }
    Dia::Observation::Health::Health HealthNow() const { return mHealth.Report(); }

    // Query registry for stages.list/get/results. Set before start (tests); default null resolves the DebugServer from the
    // "DebugServerService" ServiceStream (optional: absent in Release, the module then runs without queries). Not owned.
    void SetQueryRegistry(Dia::DebugServer::QueryRegistry* registry) { mQueryRegistryOverride = registry; }
    bool QueriesRegistered() const { return mQueriesOn != nullptr; }

protected:
    void OnConfigure(const char* configJson) override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

    // Observation session id and session directory (trailing separator). False when no session is available.
    // Seam for the ObservationSessionService (DiaViewLaunch F1, not built yet): until it exists the default returns false.
    virtual bool GetSessionInfo(std::string& sessionId, std::string& sessionDir) const;

    // Applies events (no write, no publish). Split out of DoUpdate so tests can inject events without streams.
    void ProcessEvents(const StageResultEvent* events, unsigned int count);
    // Once per frame: one results write for any number of events, then publish if the revision changed.
    void EndFrame();

private:
    class HealthReporter : public Dia::Observation::Health::IHealthReporter
    {
    public:
        explicit HealthReporter(const StageCatalogueModule& owner) : mOwner(owner) {}
        Dia::Core::StringCRC GetReporterName() const override;
        Dia::Observation::Health::Health Report() const override;
    private:
        const StageCatalogueModule& mOwner;
    };

    void Publish();
    void RegisterQueries();
    void UnregisterQueries();

    StageCatalogueState mState;
    std::string mDiagamePath;
    std::string mConfiguredResultsPath;
    std::unique_ptr<StageCatalogueSnapshot> mSnapshot;
    HealthReporter mHealth;
    bool mHealthRegistered = false;
    uint32_t mPublishCount = 0;
    Dia::ApplicationFlow::IApplicationInspectable* mAppInspect = nullptr;
    Dia::DebugServer::QueryRegistry* mQueryRegistryOverride = nullptr;
    Dia::DebugServer::QueryRegistry* mQueriesOn = nullptr;

    Dia::ApplicationFlow::EventStreamReader<StageResultEvent> mResultsIn;
    Dia::ApplicationFlow::StreamWriter<StageCatalogueSnapshot> mSnapshotOut;

    Dia::Observation::Metric::Gauge*   mMetricCount = nullptr;
    Dia::Observation::Metric::Counter* mMetricWriteFail = nullptr;
};

}} // namespace Cluiche::AppFlow
