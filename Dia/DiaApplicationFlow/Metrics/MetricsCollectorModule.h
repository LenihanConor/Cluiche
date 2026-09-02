#pragma once

#include <DiaApplicationFlow/MainModule.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Observation { namespace Metric { class Gauge; } } }

namespace Dia
{
    namespace ApplicationFlow
    {
        class MetricsCollectorModule : public MainModule
        {
        public:
            static const Dia::Core::StringCRC kInstanceId;

            MetricsCollectorModule();

        protected:
            StartResult DoStart() override;
            void        DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
            StopResult  DoStop() override;

        private:
            void QueryMemory();

            Dia::Observation::Metric::Gauge* mFpsGauge;
            Dia::Observation::Metric::Gauge* mFrameTimeGauge;
            Dia::Observation::Metric::Gauge* mMemoryGauge;
            Dia::Observation::Metric::Gauge* mUptimeGauge;

            double mUptimeAccumulator;
        };

    } // namespace ApplicationFlow
} // namespace Dia
