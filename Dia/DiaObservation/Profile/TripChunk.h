#pragma once

#include <DiaObservation/Profile/TripRecord.h>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// POD payload of the framework "$perftrips" EventStream (Perf Inspector task 3b): one emitted
		// over-budget trip of one processing unit. Sent from the PU thread next to TripReporter::Report;
		// trips are rate limited (>= 5 s apart per unit), so this is never a per-frame message.
		// The whole TripRecord (fixed size, ~4 KB) is reused so Main serialises the exact keys of trips.jsonl.
		struct TripChunk
		{
			uint32_t   unitCrc;   // ProcessingUnit instanceId CRC
			uint32_t   seq;       // 1-based cumulative trip number of that unit (== CoarseChunk::tripCount after this trip)
			TripRecord record;
		};

		static_assert(sizeof(TripChunk) <= 9000, "TripChunk exceeds the EventStream payload budget");
	}}
}
