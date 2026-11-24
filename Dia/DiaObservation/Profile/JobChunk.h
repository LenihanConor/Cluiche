#pragma once

#include <DiaObservation/Profile/JobTimingTable.h>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// POD payload for per-PU job timing chunks (<= ~8.95 KB, 9000 B budget). Sent via EventStream from each PU to
		// Main at <= 10 Hz, and only when the PU's table changed. Trivially copyable.
		//
		// Tier 1 (owners, pool) is cumulative, so the newest chunk alone is the current state (late attach
		// needs nothing else). Aggregates and raw records carry absolute indexes so the assembler can count
		// what was lost.
		struct JobChunk
		{
			static constexpr uint32_t kMaxOwners     = JobTimingTable::kMaxOwners;
			static constexpr uint32_t kMaxAggregates = 48;
			static constexpr uint32_t kMaxRaw        = 224;
			static constexpr uint32_t kMaxSteps      = 3;   // deferred-step summaries per chunk; the excess is carried to the next chunk
			static constexpr uint32_t kFlagPerJob    = 1;   // per-job recording was on

			static constexpr uint32_t kNoEpoch = 0;   // unknown epoch: assemblers treat it as a wildcard

			uint32_t unitCrc;
			uint32_t epoch;               // builder run identity; changes when the PU's builder restarts
			uint32_t seq;                 // increments per Take
			uint32_t flags;
			uint32_t ownerCount;
			uint32_t aggCount;
			uint32_t rawCount;
			uint32_t stepCount;           // step summaries in steps[] (sits in the former header padding)
			uint64_t aggFirst;           // absolute index of aggs[0]
			uint64_t rawFirst;            // absolute index of raw[0]
			uint64_t recordsSeen;         // cumulative jobs finished
			uint64_t rawOverwritten;      // cumulative records the PU ring dropped (oldest first)
			uint64_t aggOverwritten;
			uint64_t stepFirst;           // absolute index of steps[0] (the next one to ship when stepCount is 0)
			uint64_t stepsDropped;        // cumulative summaries the PU ring overwrote before they were shipped
			JobPoolCounters   pool;
			JobOwnerTier1     owners[kMaxOwners];
			JobFrameAggregate aggs[kMaxAggregates];
			JobRecordPacked   raw[kMaxRaw];
			StepSummary       steps[kMaxSteps];   // oldest first; unlike aggs/raw, excess is carried, not dropped
		};

		static_assert(sizeof(JobChunk) <= 9000, "JobChunk exceeds ~8.5 KB budget");
	}}
}
