#include "CoarseChunkBuilder.h"
#include <cstring>
#include <algorithm>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		CoarseChunkBuilder::CoarseChunkBuilder()
			: mSeq(0)
			, mEntryIndex(0)
			, mLastFlushMs(0.0)
		{
			std::memset(&mChunk, 0, sizeof(mChunk));
			std::memset(&mOut, 0, sizeof(mOut));
		}

		void CoarseChunkBuilder::Begin(uint32_t unitCrc, float unitHz)
		{
			mChunk.unitCrc = unitCrc;
			mChunk.unitHz  = unitHz;
			mChunk.count   = 0;
			mChunk.moduleCount = 0;
			mSeq = 0;
			mEntryIndex = 0;
			mLastFlushMs = 0.0;
			std::memset(mChunk.moduleCrcs, 0, sizeof(mChunk.moduleCrcs));
			std::memset(mChunk.ms, 0, sizeof(mChunk.ms));
			std::memset(mChunk.flags, 0, sizeof(mChunk.flags));
			mChunk.tripCount = 0;
			mChunk.markerDrops = 0;
		}

		bool CoarseChunkBuilder::RecordModule(uint32_t slot, uint32_t moduleCrc, float ms)
		{
			if (slot >= CoarseRing::kMaxModules)
				return false;

			if (mChunk.count >= CoarseChunk::kMaxEntries)
				return false;

			bool clamped = false;
			const uint16_t fixed = CoarseRing::ToFixed(ms, clamped);
			mChunk.ms[slot][mChunk.count] = fixed;
			if (clamped)
				mChunk.flags[mChunk.count] |= CoarseRing::kFlagClamped;

			mChunk.moduleCrcs[slot] = moduleCrc;
			mChunk.moduleCount = std::max(mChunk.moduleCount, slot + 1);
			return true;
		}

		void CoarseChunkBuilder::RecordUnitTick(float ms)
		{
			if (mChunk.count >= CoarseChunk::kMaxEntries)
				return;

			bool clamped = false;
			const uint16_t fixed = CoarseRing::ToFixed(ms, clamped);
			mChunk.ms[CoarseRing::kMaxModules][mChunk.count] = fixed;
			if (clamped)
				mChunk.flags[mChunk.count] |= CoarseRing::kFlagClamped;
		}

		void CoarseChunkBuilder::CommitEntry(double nowMs)
		{
			if (mChunk.count < CoarseChunk::kMaxEntries)
			{
				++mChunk.count;
				++mEntryIndex;
			}
		}

		bool CoarseChunkBuilder::ShouldFlush(double nowMs) const
		{
			if (mChunk.count == 0)
				return false;
			if (mChunk.count >= CoarseChunk::kMaxEntries)
				return true;
			if (nowMs - mLastFlushMs >= 100.0)
				return true;
			return false;
		}

		const CoarseChunk& CoarseChunkBuilder::Take(double nowMs)
		{
			mChunk.seq = mSeq;
			mChunk.firstEntry = mEntryIndex - mChunk.count;
			++mSeq;
			mLastFlushMs = nowMs;
			std::memcpy(&mOut, &mChunk, sizeof(CoarseChunk));

			// Reset the pending chunk; keep unit identity and the slot -> module CRC map.
			mChunk.count = 0;
			mChunk.moduleCount = 0;
			std::memset(mChunk.ms, 0, sizeof(mChunk.ms));
			std::memset(mChunk.flags, 0, sizeof(mChunk.flags));
			return mOut;
		}
	}}
}
