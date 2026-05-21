#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaThreading/JobSystem.h>

namespace Dia { namespace Observation { namespace Metric {
    class Gauge;
    class Counter;
} } }

namespace Cluiche { namespace AppFlow {

class JobSystemModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit JobSystemModule(const Dia::Core::StringCRC& instanceId);

    static JobSystemModule*          GetStatic();
    Dia::Threading::JobSystem&       GetJobSystem();
    const Dia::Threading::JobSystem& GetJobSystem() const;

protected:
    Dia::ApplicationFlow::StartResult DoStart()         override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop()          override;

private:
    static JobSystemModule*      sInstance;
    Dia::Threading::JobSystem    mJobSystem;

    // Metric primitives — owned by MetricRegistry, pointers nulled on DoStop.
    Dia::Observation::Metric::Gauge*   mMetricQueueDepth    = nullptr;
    Dia::Observation::Metric::Gauge*   mMetricActiveWorkers = nullptr;
    Dia::Observation::Metric::Counter* mMetricSubmitted     = nullptr;
    Dia::Observation::Metric::Counter* mMetricCompleted     = nullptr;
    uint64_t                           mPrevSubmitted       = 0;
    uint64_t                           mPrevCompleted       = 0;
};

} } // namespace Cluiche::AppFlow
