#pragma once

#include <DiaObservation/Profile/JobChunk.h>
#include <DiaObservation/Profile/JobTimingTable.h>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// PU-side job chunk builder. Pure, no allocation. Lives in the ProcessingUnit.
		// Throttled to ~10 Hz and skipped entirely while the table has not changed.
		class JobChunkBuilder
		{
		public:
			static constexpr double kFlushIntervalMs = 100.0;

			JobChunkBuilder();

			void Begin(uint32_t unitCrc);

			// True when >= 100 ms since the last Take and the table saw new jobs or inbox drops.
			bool ShouldFlush(const JobTimingTable& table, double nowMs) const;

			// Packs tier 1 + the not-yet-sent aggregates/raw records (newest kept if more than fit).
			// The reference stays valid until the next Take.
			const JobChunk& Take(const JobTimingTable& table, double nowMs);

		private:
			JobChunk mOut;
			uint32_t mUnitCrc;
			uint32_t mSeq;
			uint64_t mNextAgg;
			uint64_t mNextRaw;
			uint64_t mLastSeen;
			uint32_t mLastDrops;
			double   mLastFlushMs;
		};
	}}
}
