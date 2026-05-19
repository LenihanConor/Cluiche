#pragma once

#include <DiaMetrics/MetricSnapshot.h>

namespace Dia
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
} // namespace Dia
