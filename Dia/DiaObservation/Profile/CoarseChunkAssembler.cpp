#include "CoarseChunkAssembler.h"
#include <algorithm>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		CoarseChunkAssembler::UnitEntry::UnitEntry()
			: unitCrc(0)
			, unitHz(0.0f)
			, nextExpectedSeq(0)
			, gapCount(0)
			, active(false)
		{
		}

		CoarseChunkAssembler::CoarseChunkAssembler()
			: mUnitCount(0)
			, mDroppedUnknownUnit(0)
		{
		}

		void CoarseChunkAssembler::Apply(const CoarseChunk& chunk)
		{
			UnitEntry* entry = FindUnit(chunk.unitCrc);
			if (entry == nullptr)
			{
				entry = AllocateUnit(chunk.unitCrc, chunk.unitHz);
				if (entry == nullptr)
				{
					++mDroppedUnknownUnit;
					return;  // capacity exceeded, ignore
				}
				// First chunk seen for this unit (a reader can attach late): start the ring at the
				// chunk's entry index and its seq, so nothing before it counts as a gap.
				entry->ring.ResetAt(chunk.firstEntry);
				entry->nextExpectedSeq = chunk.seq;
			}

			// Check for seq gap
			if (chunk.seq != entry->nextExpectedSeq)
			{
				++entry->gapCount;
			}
			entry->nextExpectedSeq = chunk.seq + 1;

			// Check for entry gap
			const uint64_t nextEntryIndex = entry->ring.GetEntryCount();
			if (chunk.firstEntry < nextEntryIndex)
			{
				// Older/overlapping chunk, ignore
				return;
			}

			if (chunk.firstEntry > nextEntryIndex)
			{
				// Entry gap detected
				const uint64_t gapSize = chunk.firstEntry - nextEntryIndex;
				if (gapSize >= entry->ring.GetCapacity())
				{
					// Gap exceeds capacity, reset ring
					entry->ring.ResetAt(chunk.firstEntry);
				}
				else
				{
					// Fill gap with zeros and kFlagGap
					FillGap(entry->ring, nextEntryIndex, chunk.firstEntry);
				}
			}

			// Apply chunk entries
			const uint32_t count       = (chunk.count < CoarseChunk::kMaxEntries) ? chunk.count : CoarseChunk::kMaxEntries;
			const uint32_t moduleCount = (chunk.moduleCount < CoarseRing::kMaxModules) ? chunk.moduleCount : CoarseRing::kMaxModules;
			for (uint32_t i = 0; i < count; ++i)
			{
				for (uint32_t slot = 0; slot < moduleCount; ++slot)
					entry->ring.RecordModuleFixed(slot, chunk.moduleCrcs[slot], chunk.ms[slot][i]);
				entry->ring.RecordUnitTickFixed(chunk.ms[CoarseRing::kMaxModules][i]);
				entry->ring.SetStagingFlags(chunk.flags[i]);	// keeps kFlagClamped from the PU
				entry->ring.CommitEntry();
			}
		}

		const CoarseRing* CoarseChunkAssembler::GetRing(uint32_t unitCrc) const
		{
			const UnitEntry* entry = FindUnit(unitCrc);
			return entry ? &entry->ring : nullptr;
		}

		float CoarseChunkAssembler::GetUnitHz(uint32_t unitCrc) const
		{
			const UnitEntry* entry = FindUnit(unitCrc);
			return entry ? entry->unitHz : 0.0f;
		}

		uint32_t CoarseChunkAssembler::GetUnitCrcAt(uint32_t index) const
		{
			return index < mUnitCount ? mUnits[index].unitCrc : 0u;
		}

		uint32_t CoarseChunkAssembler::GetGapCount(uint32_t unitCrc) const
		{
			const UnitEntry* entry = FindUnit(unitCrc);
			return entry ? entry->gapCount : 0;
		}

		CoarseChunkAssembler::UnitEntry* CoarseChunkAssembler::FindUnit(uint32_t unitCrc)
		{
			for (uint32_t i = 0; i < mUnitCount; ++i)
			{
				if (mUnits[i].active && mUnits[i].unitCrc == unitCrc)
					return &mUnits[i];
			}
			return nullptr;
		}

		const CoarseChunkAssembler::UnitEntry* CoarseChunkAssembler::FindUnit(uint32_t unitCrc) const
		{
			for (uint32_t i = 0; i < mUnitCount; ++i)
			{
				if (mUnits[i].active && mUnits[i].unitCrc == unitCrc)
					return &mUnits[i];
			}
			return nullptr;
		}

		CoarseChunkAssembler::UnitEntry* CoarseChunkAssembler::AllocateUnit(uint32_t unitCrc, float unitHz)
		{
			if (mUnitCount >= kMaxUnits)
				return nullptr;

			UnitEntry* entry = &mUnits[mUnitCount];
			++mUnitCount;

			entry->unitCrc = unitCrc;
		entry->unitHz = unitHz;
			entry->active = true;
			entry->nextExpectedSeq = 0;
			entry->gapCount = 0;

			// Init ring with 60s capacity
			float hz = unitHz;
			if (!(hz > 0.0f))
				hz = 60.0f;  // fallback
			const uint32_t capacity = CoarseRing::CapacityFor(hz, 60.0f);
			entry->ring.Init(capacity, CoarseRing::kMaxModules);

			return entry;
		}

		void CoarseChunkAssembler::FillGap(CoarseRing& ring, uint64_t gapStart, uint64_t gapEnd)
		{
			// Caller guarantees gapEnd - gapStart < ring capacity, so indexes stay aligned.
			for (uint64_t i = gapStart; i < gapEnd; ++i)
			{
				// Record zeros with kFlagGap
				ring.RecordUnitTick(0.0f);
				ring.SetStagingFlags(CoarseRing::kFlagGap);
				ring.CommitEntry();
			}
		}
	}}
}
