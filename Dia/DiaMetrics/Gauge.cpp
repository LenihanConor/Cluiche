#include <DiaMetrics/Gauge.h>

namespace Dia
{
    namespace Metric
    {
        Gauge::Gauge()
            : mValue(0.0)
        {
        }

        void Gauge::Set(double value)
        {
            mValue.store(value, std::memory_order_relaxed);
        }

        double Gauge::Value() const
        {
            return mValue.load(std::memory_order_relaxed);
        }

    } // namespace Metric
} // namespace Dia
