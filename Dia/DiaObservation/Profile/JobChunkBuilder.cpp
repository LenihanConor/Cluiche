#include "JobChunkBuilder.h"
#include <cstring>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		JobChunkBuilder::JobChunkBuilder()
			: mUnitCrc(0), mEpoch(JobChunk::kNoEpoch), mSeq(0), mNextAgg(0), mNextRaw(0), mLastSeen(0), mLastDrops(0), mLastFlushMs(0.0)
		{
			std::memset(&mOut, 0, sizeof(mOut));
		}

		void JobChunkBuilder::Begin(uint32_t unitCrc, uint32_t epoch)
		{
			mUnitCrc = unitCrc;
			mEpoch = epoch;
			mSeq = 0;
			mNextAgg = 0;
			mNextRaw = 0;
			mLastSeen = 0;
			mLastDrops = 0;
			mLastFlushMs = 0.0;
		}

		bool JobChunkBuilder::ShouldFlush(const JobTimingTable& table, double nowMs) const
		{
			if (nowMs - mLastFlushMs < kFlushIntervalMs)
				return false;
			return table.GetRecordsSeen() != mLastSeen || table.GetPool().inboxDrops != mLastDrops;
		}

		const JobChunk& JobChunkBuilder::Take(const JobTimingTable& table, double nowMs)
		{
			JobChunk& c = mOut;
			c.unitCrc = mUnitCrc;
			c.epoch = mEpoch;
			c.seq = mSeq++;
			c.flags = table.GetPerJobRecording() ? JobChunk::kFlagPerJob : 0u;
			c.recordsSeen = table.GetRecordsSeen();
			c.pool = table.GetPool();

			c.ownerCount = table.GetOwnerCount();
			if (c.ownerCount > JobChunk::kMaxOwners) c.ownerCount = JobChunk::kMaxOwners;
			for (uint32_t i = 0; i < c.ownerCount; ++i)
				c.owners[i] = table.GetOwner(i);

			// Aggregates: everything not yet sent, newest kept.
			uint64_t from = mNextAgg;
			const uint64_t aggTotal = table.GetAggregateTotal();
			if (from < table.GetAggregateOldest()) from = table.GetAggregateOldest();
			if (aggTotal - from > JobChunk::kMaxAggregates) from = aggTotal - JobChunk::kMaxAggregates;
			c.aggFirst = from;
			c.aggCount = 0;
			for (uint64_t i = from; i < aggTotal; ++i)
				table.GetAggregate(i, c.aggs[c.aggCount++]);
			mNextAgg = aggTotal;
			c.aggOverwritten = table.GetAggregateOldest();

			// Raw records, same rule.
			from = mNextRaw;
			const uint64_t rawTotal = table.GetRawTotal();
			if (from < table.GetRawOldest()) from = table.GetRawOldest();
			if (rawTotal - from > JobChunk::kMaxRaw) from = rawTotal - JobChunk::kMaxRaw;
			c.rawFirst = from;
			c.rawCount = 0;
			for (uint64_t i = from; i < rawTotal; ++i)
				table.GetRaw(i, c.raw[c.rawCount++]);
			mNextRaw = rawTotal;
			c.rawOverwritten = table.GetRawOverwritten();

			mLastSeen = c.recordsSeen;
			mLastDrops = c.pool.inboxDrops;
			mLastFlushMs = nowMs;
			return mOut;
		}
	}}
}
