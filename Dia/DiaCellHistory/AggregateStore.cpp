#include <DiaCellHistory/AggregateStore.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cmath>
#include <cstring>

namespace Dia::CellHistory
{
	using Dia::Core::StringCRC;

	namespace
	{
		constexpr size_t kFloatRowBytes = 12;	// min, mean, max as float32
		constexpr size_t kSizeBytes = 2;		// group size (uint16) per group
	}

	AggregateStore::AggregateStore(uint64_t budgetBytes) : mBudget(budgetBytes)
	{
		mGroups[0] = WorldGroup();
		mGroupCount = 1;
	}

	AggregateStore::~AggregateStore()
	{
		delete[] mData;
		delete[] mFlags;
		delete[] mIds;
	}

	StringCRC AggregateStore::WorldGroup() { return StringCRC("world"); }

	int AggregateStore::FindLayer(StringCRC id) const
	{
		for (uint32_t i = 0; i < mLayerCount; ++i) { if (mLayers[i].mId == id) { return int(i); } }
		return -1;
	}

	int AggregateStore::FindGroup(StringCRC id) const
	{
		for (uint32_t i = 0; i < mGroupCount; ++i) { if (mGroups[i] == id) { return int(i); } }
		return -1;
	}

	size_t AggregateStore::LayerBytes(const Layer& l) const
	{
		return size_t(mGroupCount) * (l.mKind == AggregateKind::Float ? kFloatRowBytes : size_t(l.mEnumCount) * 2);
	}

	bool AggregateStore::AddLayer(StringCRC id, AggregateKind kind, uint32_t enumCount)
	{
		if (mFrozen) { DIA_LOG_ERROR("HIS", "aggregate store: cannot add layer '%s' after sampling began", id.AsChar()); return false; }
		if (FindLayer(id) >= 0 || mLayerCount >= kMaxLayers) { return false; }
		if (kind == AggregateKind::Category && (enumCount == 0 || enumCount > kMaxEnumCount)) { return false; }
		Layer& l = mLayers[mLayerCount++];
		l.mId = id;
		l.mKind = kind;
		l.mEnumCount = kind == AggregateKind::Category ? enumCount : 0;
		l.mOffset = 0;
		return true;
	}

	bool AggregateStore::AddGroup(StringCRC groupId)
	{
		if (mFrozen) { DIA_LOG_ERROR("HIS", "aggregate store: cannot add group '%s' after sampling began", groupId.AsChar()); return false; }
		if (FindGroup(groupId) >= 0 || mGroupCount >= kMaxGroups) { return false; }
		mGroups[mGroupCount++] = groupId;
		return true;
	}

	uint64_t AggregateStore::BytesPerSample() const
	{
		uint64_t sum = uint64_t(mGroupCount) * kSizeBytes;
		for (uint32_t i = 0; i < mLayerCount; ++i) { sum += LayerBytes(mLayers[i]); }
		return sum;
	}

	void AggregateStore::Freeze()
	{
		size_t off = size_t(mGroupCount) * kSizeBytes;
		for (uint32_t i = 0; i < mLayerCount; ++i)
		{
			mLayers[i].mOffset = off;
			off += LayerBytes(mLayers[i]);
		}
		mSlotBytes = off;
		mFlagBytes = size_t(mLayerCount + 1) * mGroupCount;
		mFrozen = true;
	}

	void AggregateStore::RecordDrop(uint32_t sample)
	{
		if (!mInEpisode)
		{
			mInEpisode = true;
			++mEpisodeCount;
			DIA_LOG_WARNING("HIS", "aggregate budget exceeded: dropping oldest samples (window %u, first dropped %u)", mWindow, sample);
			if (mGapCount == kMaxGaps)
			{
				for (uint32_t i = 1; i < kMaxGaps; ++i) { mGaps[i - 1] = mGaps[i]; }
				--mGapCount;
			}
			mGaps[mGapCount].mFirstDropped = sample;
			mGaps[mGapCount].mLastDropped = sample;
			++mGapCount;
		}
		else
		{
			mGaps[mGapCount - 1].mLastDropped = sample;
		}
	}

	void AggregateStore::DropOldest(uint32_t n)
	{
		for (uint32_t i = 0; i < n && mCount > 0; ++i)
		{
			RecordDrop(mIds[mHead]);
			mHead = (mHead + 1) % mWindow;
			--mCount;
		}
	}

	bool AggregateStore::BeginSample(uint32_t sampleIndex)
	{
		if (!mFrozen)
		{
			if (mLayerCount == 0) { return false; }
			const uint64_t bps = BytesPerSample();
			const uint64_t w = mBudget / bps;
			if (w == 0)
			{
				DIA_LOG_ERROR("HIS", "aggregate store: not even one sample fits the budget (%llu bytes/sample, budget %llu)", (unsigned long long)bps, (unsigned long long)mBudget);
				return false;
			}
			Freeze();
			mWindow = w > 0xFFFFFFFFull ? 0xFFFFFFFFu : uint32_t(w);
			mData = new uint8_t[size_t(mWindow) * mSlotBytes]();
			mFlags = new uint8_t[size_t(mWindow) * mFlagBytes]();
			mIds = new uint32_t[mWindow]();
		}
		if (mCount > 0 && sampleIndex <= LatestSample()) { return false; }
		if (mCount == mWindow) { DropOldest(1); }
		else { mInEpisode = false; }

		const uint32_t slot = SlotOf(mCount);
		mIds[slot] = sampleIndex;
		++mCount;
		std::memset(SlotData(slot), 0, mSlotBytes);
		std::memset(SlotFlags(slot), 0, mFlagBytes);
		return true;
	}

	bool AggregateStore::SetGroupSize(StringCRC group, uint16_t cells)
	{
		const int g = FindGroup(group);
		if (mCount == 0 || g < 0) { return false; }
		const uint32_t slot = SlotOf(mCount - 1);
		std::memcpy(SlotData(slot) + size_t(g) * kSizeBytes, &cells, kSizeBytes);
		SlotFlags(slot)[g] = 1;
		return true;
	}

	bool AggregateStore::SetFloatLayerRow(StringCRC layer, StringCRC group, const AggregateStats& stats)
	{
		const int li = FindLayer(layer);
		const int g = FindGroup(group);
		if (mCount == 0 || li < 0 || g < 0 || mLayers[li].mKind != AggregateKind::Float) { return false; }
		const uint32_t slot = SlotOf(mCount - 1);
		const float v[3] = { stats.mMin, stats.mMean, stats.mMax };
		std::memcpy(SlotData(slot) + mLayers[li].mOffset + size_t(g) * kFloatRowBytes, v, kFloatRowBytes);
		SlotFlags(slot)[size_t(li + 1) * mGroupCount + g] = 1;
		return true;
	}

	bool AggregateStore::AccumulateFloat(StringCRC layer, StringCRC group, const float* values, const uint8_t* mask, uint32_t cellCount)
	{
		if (values == nullptr && cellCount > 0) { return false; }
		float mn = 0.f, mx = 0.f;
		double sum = 0.0;
		uint32_t n = 0;
		for (uint32_t i = 0; i < cellCount; ++i)
		{
			if (mask && !mask[i]) { continue; }
			const float v = values[i];
			if (std::isnan(v)) { continue; }
			if (n == 0) { mn = mx = v; }
			else { if (v < mn) { mn = v; } if (v > mx) { mx = v; } }
			sum += double(v);
			++n;
		}
		AggregateStats s;
		if (n == 0) { s.mMin = s.mMean = s.mMax = std::nanf(""); }
		else { s.mMin = mn; s.mMax = mx; s.mMean = float(sum / double(n)); }
		return SetFloatLayerRow(layer, group, s);
	}

	bool AggregateStore::AccumulateCategory(StringCRC layer, StringCRC group, const float* values, const uint8_t* mask, uint32_t cellCount)
	{
		const int li = FindLayer(layer);
		const int g = FindGroup(group);
		if (mCount == 0 || li < 0 || g < 0 || mLayers[li].mKind != AggregateKind::Category) { return false; }
		if (values == nullptr && cellCount > 0) { return false; }
		const uint32_t e = mLayers[li].mEnumCount;
		uint16_t counts[kMaxEnumCount] = {};
		for (uint32_t i = 0; i < cellCount; ++i)
		{
			if (mask && !mask[i]) { continue; }
			const float v = values[i];
			if (std::isnan(v) || v < 0.f || v >= float(e) - 0.5f) { continue; }
			const uint32_t id = uint32_t(v + 0.5f);
			if (id < e && counts[id] < 0xFFFFu) { ++counts[id]; }
		}
		const uint32_t slot = SlotOf(mCount - 1);
		std::memcpy(SlotData(slot) + mLayers[li].mOffset + size_t(g) * e * 2, counts, size_t(e) * 2);
		SlotFlags(slot)[size_t(li + 1) * mGroupCount + g] = 1;
		return true;
	}

	bool AggregateStore::CopyRowFrom(uint32_t previousSample, StringCRC layer)
	{
		const int li = FindLayer(layer);
		uint32_t logical;
		if (mCount == 0 || li < 0 || !FindLogical(previousSample, logical)) { return false; }
		const uint32_t src = SlotOf(logical);
		const uint32_t dst = SlotOf(mCount - 1);
		if (src == dst) { return false; }
		const uint8_t* sf = SlotFlags(src) + size_t(li + 1) * mGroupCount;
		for (uint32_t g = 0; g < mGroupCount; ++g) { if (!sf[g]) { return false; } }
		const size_t bytes = LayerBytes(mLayers[li]);
		std::memcpy(SlotData(dst) + mLayers[li].mOffset, SlotData(src) + mLayers[li].mOffset, bytes);
		std::memcpy(SlotFlags(dst) + size_t(li + 1) * mGroupCount, sf, mGroupCount);
		return true;
	}

	bool AggregateStore::FindLogical(uint32_t sample, uint32_t& logical) const
	{
		uint32_t lo = 0, hi = mCount;
		while (lo < hi)
		{
			const uint32_t mid = lo + (hi - lo) / 2;
			const uint32_t v = mIds[SlotOf(mid)];
			if (v == sample) { logical = mid; return true; }
			if (v < sample) { lo = mid + 1; } else { hi = mid; }
		}
		return false;
	}

	bool AggregateStore::HasSample(uint32_t sample) const
	{
		uint32_t logical;
		return FindLogical(sample, logical);
	}

	bool AggregateStore::Aggregate(uint32_t sample, StringCRC layer, StringCRC group, AggregateStats& out) const
	{
		const int li = FindLayer(layer);
		const int g = FindGroup(group);
		uint32_t logical;
		if (li < 0 || g < 0 || mLayers[li].mKind != AggregateKind::Float || !FindLogical(sample, logical)) { return false; }
		const uint32_t slot = SlotOf(logical);
		if (!SlotFlags(slot)[size_t(li + 1) * mGroupCount + g]) { return false; }
		float v[3];
		std::memcpy(v, SlotData(slot) + mLayers[li].mOffset + size_t(g) * kFloatRowBytes, kFloatRowBytes);
		out.mMin = v[0]; out.mMean = v[1]; out.mMax = v[2];
		return true;
	}

	bool AggregateStore::CategoryCounts(uint32_t sample, StringCRC layer, StringCRC group, uint16_t* out, uint32_t n) const
	{
		const int li = FindLayer(layer);
		const int g = FindGroup(group);
		uint32_t logical;
		if (li < 0 || g < 0 || out == nullptr || mLayers[li].mKind != AggregateKind::Category || !FindLogical(sample, logical)) { return false; }
		const uint32_t e = mLayers[li].mEnumCount;
		if (n < e) { return false; }
		const uint32_t slot = SlotOf(logical);
		if (!SlotFlags(slot)[size_t(li + 1) * mGroupCount + g]) { return false; }
		std::memcpy(out, SlotData(slot) + mLayers[li].mOffset + size_t(g) * e * 2, size_t(e) * 2);
		return true;
	}

	bool AggregateStore::GroupSize(uint32_t sample, StringCRC group, uint16_t& out) const
	{
		const int g = FindGroup(group);
		uint32_t logical;
		if (g < 0 || !FindLogical(sample, logical)) { return false; }
		const uint32_t slot = SlotOf(logical);
		if (!SlotFlags(slot)[g]) { return false; }
		std::memcpy(&out, SlotData(slot) + size_t(g) * kSizeBytes, kSizeBytes);
		return true;
	}

	void AggregateStore::TruncateAfter(uint32_t sample)
	{
		bool dropped = false;
		while (mCount > 0 && mIds[SlotOf(mCount - 1)] > sample) { --mCount; dropped = true; }
		if (dropped) { mInEpisode = false; }
	}

	void AggregateStore::Clear()
	{
		mCount = 0;
		mHead = 0;
		mInEpisode = false;
		mEpisodeCount = 0;
		mGapCount = 0;
	}

	uint32_t AggregateStore::FirstRetained() const { return mCount ? mIds[SlotOf(0)] : 0; }
	uint32_t AggregateStore::LatestSample() const { return mCount ? mIds[SlotOf(mCount - 1)] : 0; }
	uint64_t AggregateStore::BytesUsed() const { return uint64_t(mWindow) * mSlotBytes; }

	double AggregateStore::UsedFraction() const
	{
		return mBudget == 0 ? 0.0 : double(uint64_t(mCount) * mSlotBytes) / double(mBudget);
	}

	AggregateGap AggregateStore::Gap(uint32_t i) const { return i < mGapCount ? mGaps[i] : AggregateGap{}; }
}
