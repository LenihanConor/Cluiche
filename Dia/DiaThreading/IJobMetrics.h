#pragma once

namespace Dia { namespace Threading {

// IJobMetrics
// ---------------------------------------------------------------------------
// Narrow instrumentation interface injected into JobSystem at Initialize()
// time. Keeps DiaThreading (foundation/core) free of a DiaObservation
// (foundation/services) dependency.
//
// Implementations live in higher-layer code (e.g. DiaObservation's
// JobSystemMetricsAdapter). Pass nullptr to Initialize() to skip metrics.
// ---------------------------------------------------------------------------
class IJobMetrics
{
public:
    virtual ~IJobMetrics() = default;

    virtual void OnJobSubmitted()                  = 0;
    virtual void OnJobCompleted()                  = 0;
    virtual void OnQueueDepthChanged(double depth) = 0;
    virtual void OnActiveWorkersChanged(double n)  = 0;
};

}} // namespace Dia::Threading
