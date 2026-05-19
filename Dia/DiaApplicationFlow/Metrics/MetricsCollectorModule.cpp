#include "DiaApplicationFlow/Metrics/MetricsCollectorModule.h"

#include <DiaMetrics/MetricRegistry.h>
#include <DiaMetrics/Gauge.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace Dia
{
    namespace ApplicationFlow
    {
        const Dia::Core::StringCRC MetricsCollectorModule::kInstanceId("MetricsCollectorModule");

        MetricsCollectorModule::MetricsCollectorModule()
            : Module(kInstanceId)
            , mFpsGauge(nullptr)
            , mFrameTimeGauge(nullptr)
            , mMemoryGauge(nullptr)
            , mUptimeGauge(nullptr)
            , mUptimeAccumulator(0.0)
        {
        }

        StartResult MetricsCollectorModule::DoStart()
        {
            mUptimeAccumulator = 0.0;

            auto& registry = Dia::Metric::MetricRegistry::Instance();
            mFpsGauge       = registry.RegisterGauge(Dia::Core::StringCRC("dia.fps"));
            mFrameTimeGauge = registry.RegisterGauge(Dia::Core::StringCRC("dia.frame_time_ms"));
            mMemoryGauge    = registry.RegisterGauge(Dia::Core::StringCRC("dia.memory_bytes"));
            mUptimeGauge    = registry.RegisterGauge(Dia::Core::StringCRC("dia.uptime_s"));

            return StartResult::kReady;
        }

        void MetricsCollectorModule::DoUpdate(float deltaTime)
        {
            if (deltaTime > 0.0f)
            {
                if (mFpsGauge)
                    mFpsGauge->Set(static_cast<double>(1.0f / deltaTime));
                if (mFrameTimeGauge)
                    mFrameTimeGauge->Set(static_cast<double>(deltaTime * 1000.0f));
            }

            mUptimeAccumulator += static_cast<double>(deltaTime);
            if (mUptimeGauge)
                mUptimeGauge->Set(mUptimeAccumulator);

            QueryMemory();
        }

        StopResult MetricsCollectorModule::DoStop()
        {
            mFpsGauge       = nullptr;
            mFrameTimeGauge = nullptr;
            mMemoryGauge    = nullptr;
            mUptimeGauge    = nullptr;
            return StopResult::kDone;
        }

        void MetricsCollectorModule::QueryMemory()
        {
#ifdef _WIN32
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
            {
                if (mMemoryGauge)
                    mMemoryGauge->Set(static_cast<double>(pmc.WorkingSetSize));
            }
#endif
        }

    } // namespace ApplicationFlow
} // namespace Dia
