#include <DiaCellHistory/CellRing.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cmath>
#include <cstring>

namespace Dia::CellHistory
{
	using Dia::Core::StringCRC;

	namespace
	{
		constexpr double kFloatMaxCode = 65534.0;
	}

	uint16_t QuantiseCellFloat(float v, float mn, float mx)
	{
		if (std::isnan(v)) { return CellRing::kFloatNaNCode; }
		if (v < mn) { v = mn; }
		if (v > mx) { v = mx; }
		const double range = double(mx) - double(mn);
		double t = (double(v) - double(mn)) / range;
		if (t < 0.0) { t = 0.0; }
		if (t > 1.0) { t = 1.0; }
		return uint16_t(std::lround(t * kFloatMaxCode));
	}

	float DequantiseCellFloat(uint16_t q, float mn, float mx)
	{
		if (q == CellRing::kFloatNaNCode) { return std::nanf(""); }
		const double range = double(mx) - double(mn);
		return float(double(mn) + (double(q) / kFloatMaxCode) * range);
	}

	uint8_t QuantiseCellCategory(float v)
	{
		if (std::isnan(v)) { return CellRing::kCategoryNone; }
		if (v <= 0.f) { return 0; }
		if (v >= 255.f) { return 255; }
		return uint8_t(std::lround(v));
	}

	CellRing::CellRing(uint32_t cellCount, uint64_t budgetBytes) : mCellCount(cellCount), mBudget(budgetBytes) {}

	CellRing::~CellRing() { FreeAll(); }

	void CellRing::FreeAll()
	{
		for (uint32_t i = 0; i < mLayerCount; ++i) { delete[] mLayers[i].mData; mLayers[i].mData = nullptr; }
		delete[] mIds;
		mIds = nullptr;
	}

	int CellRing::FindLayer(StringCRC id) const
	{
		for (uint32_t i = 0; i < mLayerCount; ++i) { if (mLayers[i].mId == id) { return int(i); } }
		return -1;
	}

	bool CellRing::IsTracked(StringCRC layer) const { return FindLayer(layer) >= 0; }

	uint64_t CellRing::ComputeBytesPerSample(int skipLayer) const
	{
		uint64_t sum = 0;
		for (uint32_t i = 0; i < mLayerCount; ++i)
		{
			if (int(i) != skipLayer) { sum += uint64_t(mCellCount) * Stride(mLayers[i]); }
		}
		return sum;
	}

	void CellRing::RecordDrop(uint32_t sample)
	{
		if (!mInEpisode)
		{
			mInEpisode = true;
			++mEpisodeCount;
			DIA_LOG_WARNING("HIS", "cell history budget exceeded: dropping oldest samples (window %u, first dropped %u)", mWindow, sample);
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

	void CellRing::DropOldest(uint32_t n)
	{
		for (uint32_t i = 0; i < n && mCount > 0; ++i)
		{
			RecordDrop(mIds[mHead]);
			mHead = (mHead + 1) % mWindow;
			--mCount;
		}
	}

	void CellRing::Relayout(uint32_t newWindow)
	{
		if (mCount > newWindow) { DropOldest(mCount - newWindow); }

		uint32_t* newIds = newWindow ? new uint32_t[newWindow]() : nullptr;
		for (uint32_t i = 0; i < mCount; ++i) { newIds[i] = mIds[SlotOf(i)]; }
		for (uint32_t li = 0; li < mLayerCount; ++li)
		{
			Layer& l = mLayers[li];
			const size_t slotBytes = size_t(mCellCount) * Stride(l);
			uint8_t* nd = newWindow ? new uint8_t[size_t(newWindow) * slotBytes]() : nullptr;
			for (uint32_t i = 0; l.mData && i < mCount; ++i)
			{
				std::memcpy(nd + size_t(i) * slotBytes, l.mData + size_t(SlotOf(i)) * slotBytes, slotBytes);
			}
			delete[] l.mData;
			l.mData = nd;
		}
		delete[] mIds;
		mIds = newIds;
		mHead = 0;
		mWindow = newWindow;
	}

	bool CellRing::Track(StringCRC layer, CellRingKind kind, float rangeMin, float rangeMax)
	{
		if (FindLayer(layer) >= 0) { DIA_LOG_WARNING("HIS", "cell ring: layer '%s' already tracked", layer.AsChar()); return false; }
		if (mLayerCount >= kMaxLayers) { DIA_LOG_ERROR("HIS", "cell ring: cannot track '%s', layer table full", layer.AsChar()); return false; }
		if (kind == CellRingKind::Float16 && !(rangeMax > rangeMin) ) { DIA_LOG_ERROR("HIS", "cell ring: layer '%s' has an invalid range", layer.AsChar()); return false; }
		if (kind == CellRingKind::Float16 && (std::isnan(rangeMin) || std::isnan(rangeMax) || std::isinf(rangeMin) || std::isinf(rangeMax))) { DIA_LOG_ERROR("HIS", "cell ring: layer '%s' has a non-finite range", layer.AsChar()); return false; }

		const uint64_t add = uint64_t(mCellCount) * (kind == CellRingKind::Float16 ? 2u : 1u);
		const uint64_t newBps = mBytesPerSample + add;
		const uint64_t window64 = newBps ? mBudget / newBps : 0;
		if (window64 == 0 || window64 > 0xFFFFFFFFull)
		{
			DIA_LOG_ERROR("HIS", "cell ring: cannot track '%s', not even one sample fits the budget (%llu bytes/sample, budget %llu)", layer.AsChar(), (unsigned long long)newBps, (unsigned long long)mBudget);
			return false;
		}

		Layer& l = mLayers[mLayerCount];
		l.mId = layer; l.mKind = kind; l.mMin = rangeMin; l.mMax = rangeMax;
		l.mData = nullptr; l.mFirstSample = 0; l.mPending = true;
		++mLayerCount;
		mBytesPerSample = newBps;
		mInEpisode = false;
		Relayout(uint32_t(window64));
		DIA_LOG_INFO("HIS", "cell history tracking '%s': window now %u samples", layer.AsChar(), mWindow);
		return true;
	}

	bool CellRing::Untrack(StringCRC layer)
	{
		const int idx = FindLayer(layer);
		if (idx < 0) { return false; }
		delete[] mLayers[idx].mData;
		mLayers[idx].mData = nullptr;
		for (uint32_t i = uint32_t(idx) + 1; i < mLayerCount; ++i) { mLayers[i - 1] = mLayers[i]; }
		mLayers[mLayerCount - 1].mData = nullptr;	// moved-from slot must not alias
		--mLayerCount;
		mBytesPerSample = ComputeBytesPerSample(-1);
		mInEpisode = false;
		if (mLayerCount == 0)
		{
			delete[] mIds; mIds = nullptr;
			mHead = 0; mCount = 0; mWindow = 0; mOpen = false;
		}
		else
		{
			// Window can only grow; the retained data is preserved. (Larger than 32 bit windows are clamped.)
			uint64_t w = mBudget / mBytesPerSample;
			if (w > 0xFFFFFFFFull) { w = 0xFFFFFFFFull; }
			Relayout(uint32_t(w));
		}
		DIA_LOG_INFO("HIS", "cell history untracked '%s': window now %u samples", layer.AsChar(), mWindow);
		return true;
	}

	bool CellRing::BeginSample(uint32_t sampleIndex)
	{
		if (mLayerCount == 0 || mWindow == 0) { return false; }
		if (mCount > 0 && sampleIndex <= LatestSample()) { return false; }
		if (mCount == mWindow) { DropOldest(1); }
		else { mInEpisode = false; }

		const uint32_t slot = SlotOf(mCount);
		mIds[slot] = sampleIndex;
		++mCount;
		for (uint32_t i = 0; i < mLayerCount; ++i)
		{
			Layer& l = mLayers[i];
			if (l.mPending) { l.mPending = false; l.mFirstSample = sampleIndex; }
			const size_t slotBytes = size_t(mCellCount) * Stride(l);
			std::memset(l.mData + size_t(slot) * slotBytes, 0, slotBytes);
		}
		mOpen = true;
		return true;
	}

	bool CellRing::Append(StringCRC layer, const float* values, uint32_t n)
	{
		if (!mOpen || mCount == 0 || n != mCellCount || values == nullptr) { return false; }
		const int idx = FindLayer(layer);
		if (idx < 0) { return false; }
		Layer& l = mLayers[idx];
		if (l.mPending) { return false; }
		const uint32_t slot = SlotOf(mCount - 1);
		const size_t slotBytes = size_t(mCellCount) * Stride(l);
		uint8_t* dst = l.mData + size_t(slot) * slotBytes;
		if (l.mKind == CellRingKind::Float16)
		{
			for (uint32_t i = 0; i < n; ++i)
			{
				const uint16_t q = QuantiseCellFloat(values[i], l.mMin, l.mMax);
				std::memcpy(dst + size_t(i) * 2, &q, 2);
			}
		}
		else
		{
			for (uint32_t i = 0; i < n; ++i) { dst[i] = QuantiseCellCategory(values[i]); }
		}
		return true;
	}

	bool CellRing::FindLogical(uint32_t sample, uint32_t& logical) const
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

	bool CellRing::HasSample(uint32_t sample) const
	{
		uint32_t logical;
		return FindLogical(sample, logical);
	}

	bool CellRing::Read(uint32_t sample, StringCRC layer, float* out, uint32_t n) const
	{
		if (n != mCellCount || out == nullptr) { return false; }
		const int idx = FindLayer(layer);
		if (idx < 0) { return false; }
		const Layer& l = mLayers[idx];
		uint32_t logical;
		if (l.mPending || sample < l.mFirstSample || !FindLogical(sample, logical)) { return false; }
		const size_t slotBytes = size_t(mCellCount) * Stride(l);
		const uint8_t* src = l.mData + size_t(SlotOf(logical)) * slotBytes;
		if (l.mKind == CellRingKind::Float16)
		{
			for (uint32_t i = 0; i < n; ++i)
			{
				uint16_t q;
				std::memcpy(&q, src + size_t(i) * 2, 2);
				out[i] = DequantiseCellFloat(q, l.mMin, l.mMax);
			}
		}
		else
		{
			for (uint32_t i = 0; i < n; ++i) { out[i] = float(src[i]); }
		}
		return true;
	}

	void CellRing::TruncateAfter(uint32_t sample)
	{
		bool dropped = false;
		while (mCount > 0 && mIds[SlotOf(mCount - 1)] > sample) { --mCount; dropped = true; }
		if (dropped) { mInEpisode = false; }
	}

	uint32_t CellRing::FirstRetained() const { return mCount ? mIds[SlotOf(0)] : 0; }
	uint32_t CellRing::LatestSample() const { return mCount ? mIds[SlotOf(mCount - 1)] : 0; }
	uint64_t CellRing::BytesUsed() const { return uint64_t(mWindow) * mBytesPerSample; }
	double CellRing::UsedFraction() const { return mBudget == 0 ? 0.0 : double(uint64_t(mCount) * mBytesPerSample) / double(mBudget); }

	CellRingGap CellRing::Gap(uint32_t i) const { return i < mGapCount ? mGaps[i] : CellRingGap{}; }
}
