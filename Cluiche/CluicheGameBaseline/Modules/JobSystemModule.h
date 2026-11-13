#pragma once
#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaStreams/ServiceStreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaThreading/JobSystem.h>
#include <DiaObservation/Metric/JobSystemMetricsAdapter.h>
#include <DiaObservation/Job/JobObserver.h>

namespace Cluiche { namespace AppFlow {

class JobSystemModule : public Dia::ApplicationFlow::MainModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Thread pool for parallel job dispatch";
    explicit JobSystemModule(const Dia::Core::StringCRC& instanceId);

    Dia::Threading::JobSystem&       GetJobSystem();
    const Dia::Threading::JobSystem& GetJobSystem() const;

protected:
    void                              OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart()         override;
    void                              DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult  DoStop()          override;

private:
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::Threading::JobSystem> mJobSystemService{this, "JobSystemService"};
    Dia::Observation::Metric::JobSystemMetricsAdapter mMetricsAdapter;
    // Declared before mJobSystem so it outlives it (members destruct in reverse order).
    // A PU installs its inbox for its thread via JobObserver::InstallThreadInbox().
    Dia::Observation::Job::JobObserver                mJobObserver;
    Dia::Threading::JobSystem                         mJobSystem;
};

} } // namespace Cluiche::AppFlow
