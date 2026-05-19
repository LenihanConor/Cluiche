#pragma once

#include <DiaMetrics/MetricRegistry.h>

namespace Dia
{
    namespace Metric
    {
        // RAII test helper. Resets the MetricRegistry before and after each test
        // to ensure isolation between test cases.
        struct MetricFixture
        {
            MetricFixture()  { MetricRegistry::Instance().Reset(); }
            ~MetricFixture() { MetricRegistry::Instance().Reset(); }
        };

    } // namespace Metric
} // namespace Dia
