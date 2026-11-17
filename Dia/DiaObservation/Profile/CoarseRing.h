#pragma once

#include <cstdint>
#include <vector>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// Per-ProcessingUnit long coarse timing ring. Stores per-module milliseconds
		// as fixed-point 0.01 ms (uint16_t) for efficient streaming. One entry = one
		// forward pass of the unit (on Sim, one fixed step). 60 s retention at unit Hz.
		class CoarseRing
		{
		public:
			static constexpr uint32_t kMaxModules = 64;
			static constexpr uint32_t kUnitSlot   = kMaxModules;
			static constexpr uint16_t kClampedValue = 65535;	// stored for values over 655.35 ms
			static constexpr uint8_t  kFlagClamped  = 0x01;	// entry flag: some value was clamped
			static constexpr uint8_t  kFlagGap     = 0x02;	// entry flag: gap filler (chunk assembler only)

			CoarseRing();

			// Allocates all storage once. moduleCount is capped at kMaxModules. A capacity of 0
			// makes the ring inert (CommitEntry does nothing).
			void Init(uint32_t entryCapacity, uint32_t moduleCount);

			// Fixed-point conversion (0.01 ms units). ToFixed clamps > 655.35 ms to 65535.
			// Sentinel: 65535 = clamped/over-range.
			static uint16_t ToFixed(float ms, bool& clamped);
			static float FromFixed(uint16_t fixed);

			// Returns entry capacity for the given unit Hz and retention seconds.
			static uint32_t CapacityFor(float unitHz, float seconds);

			// Returns total memory bytes allocated.
			uint32_t GetMemoryBytes() const;

			// Record one entry's ms for a module slot. Returns false if slot out of range.
			bool RecordModule(uint32_t slot, uint32_t moduleCrc, float ms);
			void RecordUnitTick(float ms);
			void CommitEntry();

			// Raw fixed-point variants (chunk assembler): store the value as is, no float round trip.
			bool RecordModuleFixed(uint32_t slot, uint32_t moduleCrc, uint16_t fixed);
			void RecordUnitTickFixed(uint16_t fixed);

			// OR extra flags into the entry being staged (kFlagClamped from a chunk, kFlagGap filler).
			void SetStagingFlags(uint8_t flags) { mStagingFlags |= flags; }

			uint32_t GetCapacity() const { return mCapacity; }
			uint32_t GetModuleCount() const { return mModuleCount; }

			// Read side.
			uint64_t GetEntryCount() const { return mEntryCount; }
			uint64_t GetOldestAvailable() const;
			uint32_t GetModuleCrc(uint32_t slot) const;

			// Copy entries [firstEntry, firstEntry + count) of a series (module slot or kUnitSlot).
			// Returns the number copied: the end is clipped to the newest entry; if firstEntry is
			// already overwritten (< GetOldestAvailable()) or not yet written, returns 0, so the
			// caller restarts from GetOldestAvailable() and records the gap.
			uint32_t CopyRange(uint32_t series, uint64_t firstEntry, uint32_t count, uint16_t* out) const;
			uint32_t CopyFlags(uint64_t firstEntry, uint32_t count, uint8_t* out) const;

			// Reset the ring to start at a new absolute entry index (used by assembler when
			// a gap exceeds capacity). Clears all data and sets the base to firstEntry.
			void ResetAt(uint64_t firstEntry);

		private:
			uint32_t SeriesCount() const { return mModuleCount + 1; }	// modules + unit tick
			uint32_t ClipRange(uint64_t firstEntry, uint32_t count) const;

			uint32_t mCapacity;
			uint32_t mModuleCount;
			std::vector<uint16_t> mValues;      // capacity x SeriesCount(), entry-major
			std::vector<uint8_t>  mFlags;       // capacity
			std::vector<uint32_t> mModuleCrcs;  // length = moduleCount
			uint64_t mEntryCount;
			uint64_t mBaseEntry;   // first entry index since Init/ResetAt (nothing older is held)
			uint32_t mWriteIndex;
			std::vector<uint16_t> mStagingMs;   // moduleCount + 1
			uint8_t mStagingFlags;
		};
	}}
}
