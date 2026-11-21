#pragma once

#include <stdint.h>
#include <DiaDeferredStep/DeferredStepTypes.h>
#include <DiaDeferredStep/RangeTiming.h>
#include <DiaObservation/Profile/StepSummaryRecord.h>

namespace Dia::DeferredStep
{
	struct StepSummaryInputs
	{
		bool ranInlineWholly  = false;   // serial launcher / no job system: SerialExecutor produces no RangeTimings
		bool restoredFromSave = false;   // restored step: range fields zeroed, stallNs forced to 0
		StepKey applyKey = 0;            // LaunchInfo carries no applyKey; the caller supplies it
	};

	// Pure: no I/O, no allocation. Every number is measured inside DiaDeferredStep.
	//  - capWaitNs   = sum of RangeTiming::capWaitNs (admit - enqueue for admitted ranges, start - enqueue for stolen, 0 otherwise)
	//  - workerWaitNs= sum of (start - admit) for admitted ranges only (stolen/failed-submit/coordinator range 0 have admit 0 -> 0)
	//  - cpuNs       = sum of (end - start)
	//  - inline-wholly: rangeCount 1, waits 0, cpuNs = readyNs - kickNs (the executor records no timing)
	//  - inlineFallbacks counts kRangeTrySubmitFailed; stolenRanges counts kRangeStolen; never merged.
	Dia::Observation::Profile::StepSummaryRecord BuildStepSummary(
		const LaunchInfo& info,
		const RangeTiming* timings,
		uint32_t count,
		const StepTimes& times,
		const StepSummaryInputs& inputs);
}
