#pragma once

#include <DiaObservation/Profile/StepSummaryRecord.h>

namespace Dia::DeferredStep
{
	// Receives one record per applied step, synchronously on SimPU, from inside DeferredStepScheduler::Tick.
	// Never called for a step rejected at Kick or discarded by Unregister / DrainAndDiscardAll.
	class IStepSummarySink
	{
	public:
		virtual ~IStepSummarySink() = default;
		virtual void OnStepSummary(const Dia::Observation::Profile::StepSummaryRecord& record) = 0;
	};
}
