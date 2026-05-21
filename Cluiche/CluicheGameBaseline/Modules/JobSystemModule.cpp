#include "Modules/JobSystemModule.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaThreading/JobSystem.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC JobSystemModule::kTypeId("JobSystemModule");

JobSystemModule* JobSystemModule::sInstance = nullptr;

JobSystemModule::JobSystemModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
}

Dia::ApplicationFlow::StartResult JobSystemModule::DoStart()
{
    DIA_LOG_INFO("Application", "JobSystemModule DoStart entry");
    sInstance = this;
    mJobSystem.Initialize(0);

    // Register job metrics with the global MetricRegistry.
    {
        auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
        mMetricQueueDepth    = reg.RegisterGauge(Dia::Core::StringCRC("dia.jobs.queue_depth"));
        mMetricActiveWorkers = reg.RegisterGauge(Dia::Core::StringCRC("dia.jobs.active_workers"));
        mMetricSubmitted     = reg.RegisterCounter(Dia::Core::StringCRC("dia.jobs.submitted"));
        mMetricCompleted     = reg.RegisterCounter(Dia::Core::StringCRC("dia.jobs.completed"));
    }

    DIA_LOG_INFO("Application", "JobSystemModule DoStart ready");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void JobSystemModule::DoUpdate(float /*dt*/)
{
    if (mMetricQueueDepth)
        mMetricQueueDepth->Set(static_cast<double>(mJobSystem.GetQueueDepth()));
    if (mMetricActiveWorkers)
        mMetricActiveWorkers->Set(static_cast<double>(mJobSystem.GetActiveJobCount()));

    uint64_t submitted = mJobSystem.GetSubmittedCount();
    if (mMetricSubmitted && submitted > mPrevSubmitted)
    {
        mMetricSubmitted->Inc(submitted - mPrevSubmitted);
        mPrevSubmitted = submitted;
    }

    uint64_t completed = mJobSystem.GetCompletedCount();
    if (mMetricCompleted && completed > mPrevCompleted)
    {
        mMetricCompleted->Inc(completed - mPrevCompleted);
        mPrevCompleted = completed;
    }
}

Dia::ApplicationFlow::StopResult JobSystemModule::DoStop()
{
    DIA_LOG_INFO("Application", "JobSystemModule DoStop entry");
    mJobSystem.Shutdown();
    sInstance = nullptr;

    // Null metric pointers — MetricRegistry owns the objects.
    mMetricQueueDepth    = nullptr;
    mMetricActiveWorkers = nullptr;
    mMetricSubmitted     = nullptr;
    mMetricCompleted     = nullptr;

    return Dia::ApplicationFlow::StopResult::kDone;
}

JobSystemModule* JobSystemModule::GetStatic()
{
    return sInstance;
}

Dia::Threading::JobSystem& JobSystemModule::GetJobSystem()
{
    return mJobSystem;
}

const Dia::Threading::JobSystem& JobSystemModule::GetJobSystem() const
{
    return mJobSystem;
}

} } // namespace Cluiche::AppFlow

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using JobSystemModule_ = Cluiche::AppFlow::JobSystemModule; }
DIA_MODULE(JobSystemModule_);
