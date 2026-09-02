#pragma once

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaCore/SimTime/SimTimeDomainRegistry.h>
#include <DiaSimTime/SimTimeScheduler.h>
#include <DiaSimTime/SimTimeBudget.h>
#include <DiaSimTime/SimTimeRegistry.h>
#include <DiaSimTime/SimTimePolicy.h>
#include <DiaSimTime/SimTimeState.h>
#include <cstdint>

// Forward declarations — keep the metric headers out of every consumer TU.
namespace Dia { namespace Observation { namespace Metric {
    class Gauge;
    class Counter;
} } }

namespace Dia::SimTime {

    class ISimTimeBudgetedSystem;

    // DiaSimTimeModule
    // -------------------------------------------------------------------------
    // The umbrella SimPU module every game manifest places to get LOD
    // throttling, dormancy, CPU budgeting, and scheduled events. It assembles
    // the DiaSimTime siblings built in prior tasks:
    //   * SimTimeScheduler  (Task 3.x)  — fires scheduled events into the
    //                                     SimTimeSchedulerFire EventStream.
    //   * SimTimeBudget     (Task 4.2)  — partitions per-tick CPU budget by
    //                                     priority tier.
    //   * SimTimeRegistry   (Task 4.3)  — per-system sleep/LOD state + wake
    //                                     conditions.
    // and drives them through Pattern C's per-tick gate loop each SimPU tick,
    // publishing the SimTimeContext it was handed onto the "SimTime" FrameStream
    // (Pattern D) for DiaRenderTime and any sibling readers.
    //
    // It does NOT own a SimTimeDomainRegistry: the world clock-tree registry is
    // owned by the ProcessingUnit itself (bound to the PU's world domain).
    // GetDomainRegistry() is a pass-through to GetProcessingUnit()->GetDomainRegistry().
    class DiaSimTimeModule : public Dia::ApplicationFlow::SimModule
    {
    public:
        // Not literal constexpr — StringCRC's const char* ctor is a runtime CRC
        // table lookup. Declared here, defined in the .cpp (same convention as
        // DiaRenderTime / DiaMainTime / AIBudgetModule).
        static const Dia::Core::StringCRC kTypeId;

        // The umbrella SimModule lives on SimPU.
        static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs =
            Dia::ApplicationFlow::PUAffinity::kSim;

        explicit DiaSimTimeModule(const Dia::Core::StringCRC& instanceId);

        // --- Public interface (spec) -----------------------------------------
        // Registers a budgeted system with BOTH the registry (for sleep/LOD) and
        // the budget allocator (for CPU-budget gating) — the gate loop needs it
        // in both to run.
        void  Register(ISimTimeBudgetedSystem* system, const SimTimePolicy& policy);
        void  Unregister(ISimTimeBudgetedSystem* system);

        void          Sleep(Dia::Core::StringCRC systemId);
        void          Wake(Dia::Core::StringCRC systemId);
        SimTimeState  GetState(Dia::Core::StringCRC systemId) const;

        void  RegisterWakeOnTime(Dia::Core::StringCRC systemId, Dia::Core::TimeAbsolute at);
        void  RegisterWakeOnMessage(Dia::Core::StringCRC systemId, Dia::Core::StringCRC eventType);

        // Pass-through to the ProcessingUnit-owned world clock-tree registry.
        Dia::SimTime::SimTimeDomainRegistry&  GetDomainRegistry();
        SimTimeScheduler&                     GetScheduler();

    protected:
        void        OnConfigure(const char* configJson) override;
        Dia::ApplicationFlow::StartResult DoStart() override;
        void        DoUpdate(const SimTimeContext& ctx) override;
        Dia::ApplicationFlow::StopResult  DoStop() override;
        void        OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

    private:
        // Declaration order matters: mRegistry's ctor takes SimTimeScheduler&, so
        // mScheduler must be constructed first.
        SimTimeScheduler mScheduler;
        SimTimeBudget    mBudget;
        SimTimeRegistry  mRegistry;

        // Pattern D: publish SimTimeContext each tick on the "SimTime" FrameStream
        // (the same stream ID DiaRenderTime reads).
        Dia::ApplicationFlow::StreamWriter<SimTimeContext> mSimTimeWriter{
            this, Dia::Core::StringCRC("SimTime") };

        uint64_t mTickCounter = 0;

        // Metrics (registered in DoStart, nulled in DoStop).
        Dia::Observation::Metric::Gauge*   mMetricQueueDepth    = nullptr;
        Dia::Observation::Metric::Gauge*   mMetricSleepingCount = nullptr;
        Dia::Observation::Metric::Counter* mMetricTick          = nullptr;
    };

} // namespace Dia::SimTime
