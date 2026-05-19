#pragma once

#include <DiaObservation/Metric/MetricSnapshot.h>

namespace Dia
{
    namespace Observation
    {
        namespace Metric
        {
            class IMetricSink
            {
            public:
                virtual ~IMetricSink() = default;
                virtual void OnSnapshot(const MetricSnapshot& snapshot) = 0;
                virtual void OnFinal(const MetricSnapshot& snapshot)    = 0;
            };

        } // namespace Metric
    } // namespace Observation
} // namespace Dia
