#pragma once

#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseRing.h>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// Main-side chunk assembler. Rebuilds long CoarseRing per unit from incoming chunks.
		// Handles seq gaps, entry gaps (fills with kFlagGap), and ring resets when gaps exceed capacity.
		class CoarseChunkAssembler
		{
		public:
			static constexpr uint32_t kMaxUnits = 8;

			CoarseChunkAssembler();

			// Apply an incoming chunk. If unitCrc is unknown and capacity is not exceeded,
			// creates a new ring. Otherwise updates the existing ring. Handles gaps.
			void Apply(const CoarseChunk& chunk);

			// Accessors
			uint32_t GetUnitCount() const { return mUnitCount; }
			const CoarseRing* GetRing(uint32_t unitCrc) const;
			uint32_t GetGapCount(uint32_t unitCrc) const;          // seq gaps (chunks lost on the stream)
			uint32_t GetDroppedUnknownUnitCount() const { return mDroppedUnknownUnit; }   // chunks from units beyond kMaxUnits

		private:
			struct UnitEntry
			{
				uint32_t    unitCrc;
				CoarseRing  ring;
				uint32_t    nextExpectedSeq;
				uint32_t    gapCount;
				bool        active;

				UnitEntry();
			};

			UnitEntry* FindUnit(uint32_t unitCrc);
			const UnitEntry* FindUnit(uint32_t unitCrc) const;
			UnitEntry* AllocateUnit(uint32_t unitCrc, float unitHz);
			void FillGap(CoarseRing& ring, uint64_t gapStart, uint64_t gapEnd);

			UnitEntry mUnits[kMaxUnits];
			uint32_t  mUnitCount;
			uint32_t  mDroppedUnknownUnit;
		};
	}}
}
