#pragma once

#include <stdint.h>
#include <type_traits>

// Lives in DiaObservation so JobTimingTable can hold it without DiaObservation depending on DiaDeferredStep.
namespace Dia::Observation::Profile
{
	// StepSummaryRecord::flags bits.
	namespace StepSummaryFlags
	{
		static constexpr uint16_t kStalled          = 1;   // apply waited on the step (stallNs > 0)
		static constexpr uint16_t kRanInlineWholly  = 2;   // no job system / serial launcher: one inline range
		static constexpr uint16_t kRestoredFromSave = 4;   // step restored from a save; range fields are zero
	}

	struct StepSummaryRecord
	{
		uint32_t owner = 0;            // channel StringCRC value
		uint32_t kickKeyLow = 0;       // low 32 bits of the kick key, as in JobTag (LOSSY)
		uint64_t applyKey = 0;
		uint64_t kickNs = 0;
		uint64_t firstRangeStartNs = 0;
		uint64_t readyNs = 0;          // Run returned
		uint64_t applyNs = 0;
		uint64_t cpuNs = 0;            // summed across ranges
		uint64_t capWaitNs = 0;        // summed across ranges
		uint64_t workerWaitNs = 0;     // summed across ranges
		uint64_t stallNs = 0;
		uint16_t rangeCount = 0;
		uint16_t inlineFallbacks = 0;
		uint16_t stolenRanges = 0;
		uint16_t flags = 0;            // kStalled, kRanInlineWholly, kRestoredFromSave
	};

	static_assert(sizeof(StepSummaryRecord) == 88, "StepSummaryRecord wire size changed");
	static_assert(std::is_trivially_copyable<StepSummaryRecord>::value, "StepSummaryRecord must be trivially copyable");
}
