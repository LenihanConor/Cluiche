#pragma once

#include <atomic>

namespace Dia
{
    namespace Metric
    {
        class Gauge
        {
        public:
            Gauge();

            void   Set(double value);
            double Value() const;

        private:
            std::atomic<double> mValue;
        };

    } // namespace Metric
} // namespace Dia
