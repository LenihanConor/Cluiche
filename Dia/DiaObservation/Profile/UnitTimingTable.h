#pragma once

#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// Per-ProcessingUnit timing table. Pure data: callers supply millisecond values,
		// the table never reads a clock. One entry = one forward pass of the unit
		// (on Sim, one fixed step). Slots 0..kMaxModules-1 are modules; the unit's own
		// tick is a separate series (kUnitSlot).
		class UnitTimingTable
		{
		public:
			static constexpr uint32_t kMaxModules = 64;          // matches ProcessingUnit::kMaxModules
			static constexpr uint32_t kRingSize   = 128;
			static constexpr uint32_t kRecentMax  = 60;          // window used by trip records
			static constexpr uint32_t kUnitSlot   = kMaxModules; // series index of the unit tick
			static constexpr float    kEwmaAlpha  = 0.2f;

			UnitTimingTable();

			void Reset();

			// Record one entry's ms for a module slot. Returns false if slot out of range.
			bool RecordModule(uint32_t slot, uint32_t moduleCrc, float ms);
			void RecordUnitTick(float ms);
			// Marks the end of one forward pass.
			void CommitEntry();

			uint32_t GetEntryCount() const { return mEntryCount; }
			uint32_t GetModuleCrc(uint32_t slot) const;

			// Module accessors (slot < kMaxModules; kUnitSlot addresses the unit tick; else returns 0).
			float    GetLast(uint32_t slot) const;
			float    GetEwma(uint32_t slot) const;
			float    GetP50(uint32_t slot) const;
			float    GetP95(uint32_t slot) const;
			float    GetMax(uint32_t slot) const;
			uint32_t GetSampleCount(uint32_t slot) const;   // total recorded (not capped by ring)

			// Unit tick equivalents.
			float    GetUnitLast() const        { return GetLast(kUnitSlot); }
			float    GetUnitEwma() const        { return GetEwma(kUnitSlot); }
			float    GetUnitP50() const         { return GetP50(kUnitSlot); }
			float    GetUnitP95() const         { return GetP95(kUnitSlot); }
			float    GetUnitMax() const         { return GetMax(kUnitSlot); }
			uint32_t GetUnitSampleCount() const { return GetSampleCount(kUnitSlot); }

			// Copies up to maxOut most recent samples (also capped at kRingSize), newest LAST.
			// Returns number copied. slot may be kUnitSlot.
			uint32_t CopyRecent(uint32_t slot, float* out, uint32_t maxOut) const;
			uint32_t CopyRecentUnit(float* out, uint32_t maxOut) const { return CopyRecent(kUnitSlot, out, maxOut); }

		private:
			struct Series
			{
				float    mRing[kRingSize];
				float    mLast;
				float    mEwma;
				uint32_t mCount;     // total samples recorded
				uint32_t mCrc;
			};

			static void Push(Series& s, float ms);
			float       Percentile(uint32_t slot, float p) const;
			bool        Valid(uint32_t slot) const { return slot <= kUnitSlot; }

			Series   mSeries[kMaxModules + 1];
			uint32_t mEntryCount;
			// Future (not implemented): per-module "starting" flags, Sim step/catch-up counter.
		};
	}}
}
