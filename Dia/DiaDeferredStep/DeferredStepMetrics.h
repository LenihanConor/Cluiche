#pragma once

#include <stdint.h>
#include <DiaDeferredStep/CapGate.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>

namespace Dia::DeferredStep
{
	// Owns the nine global dia.deferredstep.* registrations. SimPU only.
	// Counters are advanced by the delta between the stats' running totals and the last published value, so nothing
	// is counted twice. Unregister() retires the nine metrics in MetricRegistry (absent from snapshots); Register() revives them zeroed.
	class DeferredStepMetrics
	{
	public:
		void Register();     // resets the published baselines (a new scheduler starts its stats at zero)
		void Unregister();
		bool IsRegistered() const { return mInFlight != nullptr; }

		void Update(const SchedulerStats& stats, const CapStats& cap, bool hasCap);

	private:
		Dia::Observation::Metric::Gauge*   mInFlight = nullptr;
		Dia::Observation::Metric::Counter* mKicks = nullptr;
		Dia::Observation::Metric::Counter* mKicksRejected = nullptr;
		Dia::Observation::Metric::Counter* mApplies = nullptr;
		Dia::Observation::Metric::Counter* mStalls = nullptr;
		Dia::Observation::Metric::Counter* mStallMs = nullptr;
		Dia::Observation::Metric::Gauge*   mSlackKeys = nullptr;
		Dia::Observation::Metric::Gauge*   mCapUtilisation = nullptr;
		Dia::Observation::Metric::Counter* mInlineFallbacks = nullptr;

		uint64_t mPubKicks = 0;
		uint64_t mPubKicksRejected = 0;
		uint64_t mPubApplies = 0;
		uint64_t mPubStalls = 0;
		uint64_t mPubStallMs = 0;
		uint64_t mPubInline = 0;
	};
}
