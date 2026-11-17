#include "JobChunkAssembler.h"

namespace Dia
{
	namespace Observation { namespace Profile
	{
		JobChunkAssembler::JobChunkAssembler() : mUnitCount(0), mDroppedUnknownUnit(0) {}

		JobChunkAssembler::UnitEntry* JobChunkAssembler::FindUnit(uint32_t unitCrc)
		{
			for (uint32_t i = 0; i < mUnitCount; ++i)
				if (mUnits[i].active && mUnits[i].unitCrc == unitCrc) return &mUnits[i];
			return nullptr;
		}

		const JobChunkAssembler::UnitEntry* JobChunkAssembler::FindUnit(uint32_t unitCrc) const
		{
			for (uint32_t i = 0; i < mUnitCount; ++i)
				if (mUnits[i].active && mUnits[i].unitCrc == unitCrc) return &mUnits[i];
			return nullptr;
		}

		void JobChunkAssembler::Apply(const JobChunk& chunk)
		{
			UnitEntry* e = FindUnit(chunk.unitCrc);
			if (e == nullptr)
			{
				if (mUnitCount >= kMaxUnits)
				{
					++mDroppedUnknownUnit;
					return;
				}
				e = &mUnits[mUnitCount++];
				e->unitCrc = chunk.unitCrc;
				e->active = true;
				e->raw.assign(kRawRingCapacity, JobRecordPacked{});
				e->agg.assign(kAggHistoryCapacity, JobFrameAggregate{});
				// Late attach: start at this chunk's seq and indexes; nothing before it counts as lost.
				e->nextSeq = chunk.seq;
				e->nextRaw = chunk.rawFirst;
				e->nextAgg = chunk.aggFirst;
				e->epoch = chunk.epoch;
			}
			else if (chunk.epoch != JobChunk::kNoEpoch)
			{
				// Epoch rule (same as CoarseChunkAssembler): kNoEpoch is a wildcard; a stored kNoEpoch adopts the
				// first real epoch; two different real epochs reset the unit's rings and cursors to this chunk.
				// gapCount/rawLost/aggLost are cumulative and kept.
				if (e->epoch == JobChunk::kNoEpoch)
					e->epoch = chunk.epoch;
				else if (e->epoch != chunk.epoch)
				{
					e->epoch = chunk.epoch;
					++e->epochResets;
					e->rawTotal = 0;
					e->aggTotal = 0;
					e->hasLatest = false;
					e->nextSeq = chunk.seq;
					e->nextRaw = chunk.rawFirst;
					e->nextAgg = chunk.aggFirst;
				}
			}

			if (chunk.seq < e->nextSeq)
				return;   // stale / duplicate
			if (chunk.seq != e->nextSeq)
				++e->gapCount;
			e->nextSeq = chunk.seq + 1;

			e->latest = chunk;
			e->hasLatest = true;

			const uint32_t rawCount = chunk.rawCount < JobChunk::kMaxRaw ? chunk.rawCount : JobChunk::kMaxRaw;
			if (chunk.rawFirst > e->nextRaw)
			{
				e->rawLost += chunk.rawFirst - e->nextRaw;
				e->nextRaw = chunk.rawFirst;
			}
			for (uint32_t i = 0; i < rawCount; ++i)
			{
				const uint64_t idx = chunk.rawFirst + i;
				if (idx < e->nextRaw)
					continue;   // overlap
				e->raw[e->rawTotal % e->raw.size()] = chunk.raw[i];
				++e->rawTotal;
				e->nextRaw = idx + 1;
			}

			const uint32_t aggCount = chunk.aggCount < JobChunk::kMaxAggregates ? chunk.aggCount : JobChunk::kMaxAggregates;
			if (chunk.aggFirst > e->nextAgg)
			{
				e->aggLost += chunk.aggFirst - e->nextAgg;
				e->nextAgg = chunk.aggFirst;
			}
			for (uint32_t i = 0; i < aggCount; ++i)
			{
				const uint64_t idx = chunk.aggFirst + i;
				if (idx < e->nextAgg)
					continue;
				e->agg[e->aggTotal % e->agg.size()] = chunk.aggs[i];
				++e->aggTotal;
				e->nextAgg = idx + 1;
			}
		}

		uint32_t JobChunkAssembler::GetUnitCrcAt(uint32_t index) const
		{
			return index < mUnitCount ? mUnits[index].unitCrc : 0u;
		}

		const JobChunk* JobChunkAssembler::GetLatest(uint32_t unitCrc) const
		{
			const UnitEntry* e = FindUnit(unitCrc);
			return (e && e->hasLatest) ? &e->latest : nullptr;
		}

		uint32_t JobChunkAssembler::GetEpochResets(uint32_t unitCrc) const
		{
			const UnitEntry* e = FindUnit(unitCrc);
			return e ? e->epochResets : 0;
		}

		uint32_t JobChunkAssembler::GetGapCount(uint32_t unitCrc) const
		{
			const UnitEntry* e = FindUnit(unitCrc);
			return e ? e->gapCount : 0;
		}

		uint64_t JobChunkAssembler::GetRawLost(uint32_t unitCrc) const
		{
			const UnitEntry* e = FindUnit(unitCrc);
			return e ? e->rawLost : 0;
		}

		uint64_t JobChunkAssembler::GetAggLost(uint32_t unitCrc) const
		{
			const UnitEntry* e = FindUnit(unitCrc);
			return e ? e->aggLost : 0;
		}

		uint32_t JobChunkAssembler::GetRawCount(uint32_t unitCrc) const
		{
			const UnitEntry* e = FindUnit(unitCrc);
			if (!e) return 0;
			return e->rawTotal < e->raw.size() ? static_cast<uint32_t>(e->rawTotal) : static_cast<uint32_t>(e->raw.size());
		}

		uint32_t JobChunkAssembler::CopyRaw(uint32_t unitCrc, uint64_t fromNs, uint64_t toNs, JobRecordPacked* out, uint32_t max, bool* truncated) const
		{
			if (truncated) *truncated = false;
			const UnitEntry* e = FindUnit(unitCrc);
			if (!e) return 0;
			const uint64_t cap = e->raw.size();
			const uint64_t oldest = e->rawTotal > cap ? e->rawTotal - cap : 0;
			uint32_t n = 0;
			for (uint64_t i = oldest; i < e->rawTotal; ++i)
			{
				const JobRecordPacked& p = e->raw[i % cap];
				if (p.startNs > toNs || p.startNs + p.durNs < fromNs)
					continue;
				if (n >= max)
				{
					if (truncated) *truncated = true;
					break;
				}
				out[n++] = p;
			}
			return n;
		}

		uint32_t JobChunkAssembler::GetAggCount(uint32_t unitCrc) const
		{
			const UnitEntry* e = FindUnit(unitCrc);
			if (!e) return 0;
			return e->aggTotal < e->agg.size() ? static_cast<uint32_t>(e->aggTotal) : static_cast<uint32_t>(e->agg.size());
		}

		uint32_t JobChunkAssembler::CopyAggregates(uint32_t unitCrc, JobFrameAggregate* out, uint32_t max) const
		{
			const UnitEntry* e = FindUnit(unitCrc);
			if (!e) return 0;
			const uint64_t cap = e->agg.size();
			const uint64_t oldest = e->aggTotal > cap ? e->aggTotal - cap : 0;
			uint64_t start = oldest;
			if (e->aggTotal - start > max) start = e->aggTotal - max;
			uint32_t n = 0;
			for (uint64_t i = start; i < e->aggTotal; ++i)
				out[n++] = e->agg[i % cap];
			return n;
		}
	}}
}
