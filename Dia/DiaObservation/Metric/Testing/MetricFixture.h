#pragma once

#include <DiaObservation/Metric/MetricRegistry.h>

namespace Dia
{
    namespace Observation
    {
        namespace Metric
        {
            struct MetricFixture
            {
                MetricFixture()  { MetricRegistry::Instance().Reset(); }
                ~MetricFixture() { MetricRegistry::Instance().Reset(); }
            };

        } // namespace Metric
    } // namespace Observation
} // namespace Dia
