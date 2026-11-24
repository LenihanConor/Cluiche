#include <DiaObservation/Profile/UnitTimingTable.h>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		UnitTimingTable::UnitTimingTable() { Reset(); }

		void UnitTimingTable::Reset()
		{
			for (uint32_t i = 0; i <= kMaxModules; ++i)
			{
				Series& s = mSeries[i];
				for (uint32_t r = 0; r < kRingSize; ++r) s.mRing[r] = 0.0f;
				s.mLast = 0.0f; s.mEwma = 0.0f; s.mCount = 0; s.mCrc = 0;
			}
			mEntryCount = 0;
		}

		void UnitTimingTable::Push(Series& s, float ms)
		{
			s.mRing[s.mCount % kRingSize] = ms;
			s.mEwma = (s.mCount == 0) ? ms : s.mEwma + kEwmaAlpha * (ms - s.mEwma);
			s.mLast = ms;
			++s.mCount;
		}

		bool UnitTimingTable::RecordModule(uint32_t slot, uint32_t moduleCrc, float ms)
		{
			if (slot >= kMaxModules) return false;
			mSeries[slot].mCrc = moduleCrc;
			Push(mSeries[slot], ms);
			return true;
		}

		void UnitTimingTable::RecordUnitTick(float ms) { Push(mSeries[kUnitSlot], ms); }

		void UnitTimingTable::CommitEntry() { ++mEntryCount; }

		uint32_t UnitTimingTable::GetModuleCrc(uint32_t slot) const
		{
			return slot < kMaxModules ? mSeries[slot].mCrc : 0;
		}

		float UnitTimingTable::GetLast(uint32_t slot) const { return Valid(slot) ? mSeries[slot].mLast : 0.0f; }
		float UnitTimingTable::GetEwma(uint32_t slot) const { return Valid(slot) ? mSeries[slot].mEwma : 0.0f; }
		uint32_t UnitTimingTable::GetSampleCount(uint32_t slot) const { return Valid(slot) ? mSeries[slot].mCount : 0; }

		float UnitTimingTable::GetMax(uint32_t slot) const
		{
			if (!Valid(slot) || mSeries[slot].mCount == 0) return 0.0f;
			const Series& s = mSeries[slot];
			const uint32_t n = s.mCount < kRingSize ? s.mCount : kRingSize;
			float m = s.mRing[0];
			for (uint32_t i = 1; i < n; ++i) if (s.mRing[i] > m) m = s.mRing[i];
			return m;
		}

		// Nearest-rank percentile over the ring contents.
		float UnitTimingTable::Percentile(uint32_t slot, float p) const
		{
			if (!Valid(slot) || mSeries[slot].mCount == 0) return 0.0f;
			const Series& s = mSeries[slot];
			const uint32_t n = s.mCount < kRingSize ? s.mCount : kRingSize;
			float tmp[kRingSize];
			for (uint32_t i = 0; i < n; ++i) tmp[i] = s.mRing[i];
			for (uint32_t i = 1; i < n; ++i)   // insertion sort, n <= 128
			{
				const float v = tmp[i];
				uint32_t j = i;
				while (j > 0 && tmp[j - 1] > v) { tmp[j] = tmp[j - 1]; --j; }
				tmp[j] = v;
			}
			const float exact = p * static_cast<float>(n);
			uint32_t rank = static_cast<uint32_t>(exact);
			if (static_cast<float>(rank) < exact) ++rank; // ceil
			if (rank == 0) rank = 1;
			if (rank > n) rank = n;
			return tmp[rank - 1];
		}

		float UnitTimingTable::GetP50(uint32_t slot) const { return Percentile(slot, 0.50f); }
		float UnitTimingTable::GetP95(uint32_t slot) const { return Percentile(slot, 0.95f); }

		uint32_t UnitTimingTable::CopyRecent(uint32_t slot, float* out, uint32_t maxOut) const
		{
			if (!Valid(slot) || out == nullptr || maxOut == 0) return 0;
			const Series& s = mSeries[slot];
			uint32_t n = s.mCount < kRingSize ? s.mCount : kRingSize;
			if (n > maxOut) n = maxOut;
			const uint32_t start = s.mCount - n;   // index of oldest copied sample
			for (uint32_t i = 0; i < n; ++i) out[i] = s.mRing[(start + i) % kRingSize];
			return n;
		}
	}}
}
