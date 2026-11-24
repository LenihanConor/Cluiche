#pragma once

#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseRing.h>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// PU-side chunk builder. Accumulates up to 64 entries and flushes at 64 or ~10 Hz.
		// Pure, no allocation. Lives in the ProcessingUnit.
		class CoarseChunkBuilder
		{
		public:
			CoarseChunkBuilder();

			// Initialize for a new chunk stream.
			void Begin(uint32_t unitCrc, float unitHz, uint32_t epoch = CoarseChunk::kNoEpoch);

			// Record one entry's ms for a module slot. Returns false if slot out of range.
			bool RecordModule(uint32_t slot, uint32_t moduleCrc, float ms);
			void RecordUnitTick(float ms);
			void CommitEntry(double nowMs);

			// Cumulative per-unit counters stamped into the NEXT taken chunk (task 3b). Default 0.
			void SetCumulativeCounters(uint32_t tripCount, uint32_t markerDrops) { mChunk.tripCount = tripCount; mChunk.markerDrops = markerDrops; }

			// True when count == 64 or (count > 0 and nowMs - lastFlushMs >= 100).
			bool ShouldFlush(double nowMs) const;

			// Finishes the pending chunk (seq, firstEntry), copies it to the output buffer, resets the
			// pending chunk and returns the output. The reference stays valid until the next Take.
			const CoarseChunk& Take(double nowMs);

		private:
			CoarseChunk mChunk;   // pending entries
			CoarseChunk mOut;     // last finished chunk
			uint32_t    mSeq;
			uint64_t    mEntryIndex;       // absolute entry count (firstEntry of next chunk)
			double      mLastFlushMs;
		};
	}}
}
