#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Metric { class Gauge; } }

namespace Dia
{
    namespace ApplicationFlow
    {
        class MetricsCollectorModule : public Module
        {
        public:
            static const Dia::Core::StringCRC kInstanceId;

            MetricsCollectorModule();

        protected:
            StartResult DoStart() override;
            void        DoUpdate(float deltaTime) override;
            StopResult  DoStop() override;

        private:
            void QueryMemory();

            Dia::Metric::Gauge* mFpsGauge;
            Dia::Metric::Gauge* mFrameTimeGauge;
            Dia::Metric::Gauge* mMemoryGauge;
            Dia::Metric::Gauge* mUptimeGauge;

            double mUptimeAccumulator;
        };

    } // namespace ApplicationFlow
} // namespace Dia
