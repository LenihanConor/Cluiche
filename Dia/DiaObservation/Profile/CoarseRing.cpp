#include "CoarseRing.h"
#include <cmath>
#include <algorithm>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		CoarseRing::CoarseRing()
			: mCapacity(0)
			, mModuleCount(0)
			, mEntryCount(0)
			, mBaseEntry(0)
			, mWriteIndex(0)
			, mStagingFlags(0)
		{
		}

		void CoarseRing::Init(uint32_t entryCapacity, uint32_t moduleCount)
		{
			mCapacity = entryCapacity;
			mModuleCount = std::min(moduleCount, kMaxModules);
			mEntryCount = 0;
			mBaseEntry = 0;
			mWriteIndex = 0;
			mStagingFlags = 0;

			const uint32_t seriesCount = SeriesCount();
			mValues.assign(static_cast<size_t>(mCapacity) * seriesCount, 0);
			mFlags.assign(mCapacity, 0);
			mStagingMs.assign(seriesCount, 0);
			mModuleCrcs.assign(mModuleCount, 0);
		}

		uint16_t CoarseRing::ToFixed(float ms, bool& clamped)
		{
			clamped = false;
			if (std::isnan(ms) || ms < 0.0f)
				return 0;
			const float scaled = ms * 100.0f;	// 0.01 ms units
			if (scaled > 65535.0f)
			{
				clamped = true;
				return kClampedValue;
			}
			return static_cast<uint16_t>(scaled + 0.5f);	// round half up
		}

		float CoarseRing::FromFixed(uint16_t fixed)
		{
			return static_cast<float>(fixed) * 0.01f;
		}

		uint32_t CoarseRing::CapacityFor(float unitHz, float seconds)
		{
			if (!(unitHz > 0.0f) || !(seconds > 0.0f))
				return 0;
			return static_cast<uint32_t>(unitHz * seconds + 0.5f);
		}

		uint32_t CoarseRing::GetMemoryBytes() const
		{
			return static_cast<uint32_t>(mValues.size() * sizeof(uint16_t) + mFlags.size());
		}

		bool CoarseRing::RecordModule(uint32_t slot, uint32_t moduleCrc, float ms)
		{
			if (slot >= mModuleCount)
				return false;

			bool clamped = false;
			mStagingMs[slot] = ToFixed(ms, clamped);
			if (clamped)
				mStagingFlags |= kFlagClamped;
			mModuleCrcs[slot] = moduleCrc;
			return true;
		}

		void CoarseRing::RecordUnitTick(float ms)
		{
			if (mStagingMs.empty())
				return;
			bool clamped = false;
			mStagingMs[mModuleCount] = ToFixed(ms, clamped);	// unit series is stored after the modules
			if (clamped)
				mStagingFlags |= kFlagClamped;
		}

		void CoarseRing::CommitEntry()
		{
			if (mCapacity == 0)
				return;

			const uint32_t seriesCount = SeriesCount();
			std::copy(mStagingMs.begin(), mStagingMs.end(), mValues.begin() + static_cast<size_t>(mWriteIndex) * seriesCount);
			mFlags[mWriteIndex] = mStagingFlags;

			++mEntryCount;
			mWriteIndex = (mWriteIndex + 1) % mCapacity;

			std::fill(mStagingMs.begin(), mStagingMs.end(), static_cast<uint16_t>(0));
			mStagingFlags = 0;
		}

		uint64_t CoarseRing::GetOldestAvailable() const
		{
			if (mEntryCount - mBaseEntry <= mCapacity)
				return mBaseEntry;
			return mEntryCount - mCapacity;
		}

		uint32_t CoarseRing::GetModuleCrc(uint32_t slot) const
		{
			if (slot >= mModuleCount)
				return 0;
			return mModuleCrcs[slot];
		}

		uint32_t CoarseRing::ClipRange(uint64_t firstEntry, uint32_t count) const
		{
			if (count == 0 || firstEntry < GetOldestAvailable() || firstEntry >= mEntryCount)
				return 0;
			const uint64_t available = mEntryCount - firstEntry;
			return static_cast<uint32_t>(std::min<uint64_t>(count, available));
		}

		uint32_t CoarseRing::CopyRange(uint32_t series, uint64_t firstEntry, uint32_t count, uint16_t* out) const
		{
			uint32_t storageSeries = series;
			if (series == kUnitSlot)
				storageSeries = mModuleCount;
			else if (series >= mModuleCount)
				return 0;

			const uint32_t actualCount = ClipRange(firstEntry, count);
			const uint32_t seriesCount = SeriesCount();
			for (uint32_t i = 0; i < actualCount; ++i)
			{
				const uint32_t ringIndex = static_cast<uint32_t>((firstEntry + i) % mCapacity);
				out[i] = mValues[static_cast<size_t>(ringIndex) * seriesCount + storageSeries];
			}
			return actualCount;
		}

		uint32_t CoarseRing::CopyFlags(uint64_t firstEntry, uint32_t count, uint8_t* out) const
		{
			const uint32_t actualCount = ClipRange(firstEntry, count);
			for (uint32_t i = 0; i < actualCount; ++i)
				out[i] = mFlags[static_cast<uint32_t>((firstEntry + i) % mCapacity)];
			return actualCount;
		}

		bool CoarseRing::RecordModuleFixed(uint32_t slot, uint32_t moduleCrc, uint16_t fixed)
		{
			if (slot >= mModuleCount)
				return false;
			mStagingMs[slot] = fixed;
			mModuleCrcs[slot] = moduleCrc;
			return true;
		}

		void CoarseRing::RecordUnitTickFixed(uint16_t fixed)
		{
			if (mStagingMs.empty())
				return;
			mStagingMs[mModuleCount] = fixed;
		}

		void CoarseRing::ResetAt(uint64_t firstEntry)
		{
			mEntryCount = firstEntry;
			mBaseEntry = firstEntry;
			mWriteIndex = (mCapacity > 0) ? static_cast<uint32_t>(firstEntry % mCapacity) : 0;
			std::fill(mValues.begin(), mValues.end(), static_cast<uint16_t>(0));
			std::fill(mFlags.begin(), mFlags.end(), static_cast<uint8_t>(0));
			std::fill(mStagingMs.begin(), mStagingMs.end(), static_cast<uint16_t>(0));
			mStagingFlags = 0;
		}
	}}
}
