#pragma once

#include <DiaObservation/Profile/JobChunk.h>

#include <cstdint>
#include <vector>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// Main-side job chunk assembler. Pure. Per unit it keeps the latest tier 1 state (the newest
		// chunk), a raw job ring (~5 s) and a history of per-owner per-frame aggregates. Counts seq
		// gaps and the raw/aggregate records known to be lost. Storage is allocated when a unit first
		// appears (not on later Apply calls).
		class JobChunkAssembler
		{
		public:
			static constexpr uint32_t kMaxUnits         = 8;
			static constexpr uint32_t kRawRingCapacity  = 8192;
			static constexpr uint32_t kAggHistoryCapacity = 16384;   // 60 s at 30 Hz with ~9 busy owners (tier 2 window)

			JobChunkAssembler();

			void Apply(const JobChunk& chunk);

			uint32_t GetUnitCount() const { return mUnitCount; }
			uint32_t GetUnitCrcAt(uint32_t index) const;                 // arrival order; 0 if out of range
			uint32_t GetDroppedUnknownUnitCount() const { return mDroppedUnknownUnit; }

			const JobChunk* GetLatest(uint32_t unitCrc) const;           // tier 1 current state; null if unknown
			uint32_t GetEpochResets(uint32_t unitCrc) const;             // times the unit restarted under a new epoch
			uint32_t GetGapCount(uint32_t unitCrc) const;                // chunks lost on the stream
			uint64_t GetRawLost(uint32_t unitCrc) const;                 // raw records known lost (stream gap or burst)
			uint64_t GetAggLost(uint32_t unitCrc) const;

			uint32_t GetRawCount(uint32_t unitCrc) const;
			// Raw records overlapping [fromNs, toNs], oldest first; truncated set when more than max matched.
			uint32_t CopyRaw(uint32_t unitCrc, uint64_t fromNs, uint64_t toNs, JobRecordPacked* out, uint32_t max, bool* truncated) const;

			uint32_t GetAggCount(uint32_t unitCrc) const;
			// Newest min(max, count) aggregates, oldest first.
			uint32_t CopyAggregates(uint32_t unitCrc, JobFrameAggregate* out, uint32_t max) const;

		private:
			struct UnitEntry
			{
				uint32_t unitCrc = 0;
				bool     active = false;
				bool     hasLatest = false;
				JobChunk latest;
				uint32_t nextSeq = 0;
				uint32_t gapCount = 0;
				uint32_t epoch = 0;
				uint32_t epochResets = 0;
				uint64_t nextRaw = 0;
				uint64_t nextAgg = 0;
				uint64_t rawLost = 0;
				uint64_t aggLost = 0;
				std::vector<JobRecordPacked>   raw;
				uint64_t rawTotal = 0;
				std::vector<JobFrameAggregate> agg;
				uint64_t aggTotal = 0;
			};

			UnitEntry*       FindUnit(uint32_t unitCrc);
			const UnitEntry* FindUnit(uint32_t unitCrc) const;

			UnitEntry mUnits[kMaxUnits];
			uint32_t  mUnitCount;
			uint32_t  mDroppedUnknownUnit;
		};
	}}
}
