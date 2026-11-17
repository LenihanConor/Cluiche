#pragma once

#include <DiaObservation/Profile/JobTimingTable.h>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// POD payload for per-PU job timing chunks (<= ~8.7 KB). Sent via EventStream from each PU to
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
			static constexpr uint32_t kFlagPerJob    = 1;   // per-job recording was on

			static constexpr uint32_t kNoEpoch = 0;   // unknown epoch: assemblers treat it as a wildcard

			uint32_t unitCrc;
			uint32_t epoch;               // builder run identity; changes when the PU's builder restarts
			uint32_t seq;                 // increments per Take
			uint32_t flags;
			uint32_t ownerCount;
			uint32_t aggCount;
			uint32_t rawCount;
			uint64_t aggFirst;            // absolute index of aggs[0]
			uint64_t rawFirst;            // absolute index of raw[0]
			uint64_t recordsSeen;         // cumulative jobs finished
			uint64_t rawOverwritten;      // cumulative records the PU ring dropped (oldest first)
			uint64_t aggOverwritten;
			JobPoolCounters   pool;
			JobOwnerTier1     owners[kMaxOwners];
			JobFrameAggregate aggs[kMaxAggregates];
			JobRecordPacked   raw[kMaxRaw];
		};

		static_assert(sizeof(JobChunk) <= 9000, "JobChunk exceeds ~8.5 KB budget");
	}}
}
