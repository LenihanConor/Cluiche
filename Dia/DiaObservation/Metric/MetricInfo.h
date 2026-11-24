#pragma once

namespace Dia
{
    namespace Observation
    {
        namespace Metric
        {
            // Optional human-facing description of a metric. All strings must be literals
            // (or otherwise outlive the registration): registries store the pointers, not copies.
            struct MetricInfo
            {
                const char* label       = nullptr;
                const char* description = nullptr;
                const char* unit        = nullptr;
            };

        } // namespace Metric
    } // namespace Observation
} // namespace Dia
