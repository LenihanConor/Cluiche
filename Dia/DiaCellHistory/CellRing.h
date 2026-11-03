#pragma once

// CellRing: Tier-2 per-cell history. A set of per-layer ring buffers sharing ONE window of sample indices.
// Pure data structure (no DiaCellLayers dependency, no STL in this header).
//
// Storage: Float16 layers are quantised to 16 bits over [rangeMin, rangeMax]; Category8 layers to 8 bits.
//   Float16 encoding: q = round(clamp(v) normalised * 65534), code 0xFFFF is RESERVED for NaN (round-trips to NaN),
//   so the max value encodes as 65534. Dequantisation error <= range / (2 * 65534) < range / 65,535.
//   Out-of-range values clamp (+/-inf too). Category8: round, clamp to 0..255; NaN stores 255 (id 255 is reserved
//   for NaN/none and reads back as 255.0f).
// Window: windowLength = budgetBytes / bytesPerSample (sum over tracked layers of cellCount * 2 or * 1). When a
//   new sample would not fit the oldest sample is dropped from ALL rings together and a gap is recorded.
// Sample indices are the caller's global indices: strictly increasing, may skip. Call Track/Untrack/TruncateAfter
//   between samples (not between BeginSample and its Appends). A layer tracked mid-run has data from the next
//   BeginSample on.

#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Dia::CellHistory
{
	enum class CellRingKind : uint8_t { Float16, Category8 };

	struct CellRingGap
	{
		uint32_t mFirstDropped = 0;
		uint32_t mLastDropped = 0;
	};

	class CellRing
	{
	public:
		static constexpr uint32_t kMaxLayers = 64;
		static constexpr uint32_t kMaxGaps = 64;
		static constexpr uint16_t kFloatNaNCode = 0xFFFF;
		static constexpr uint8_t kCategoryNone = 255;

		CellRing(uint32_t cellCount, uint64_t budgetBytes);
		~CellRing();
		CellRing(const CellRing&) = delete;
		CellRing& operator=(const CellRing&) = delete;

		// false (no change) on duplicate, bad range (Float16 needs rangeMax > rangeMin, not NaN), full table, or a
		// window of 0 (logs Error). Logs Info with the new window on success.
		bool Track(Dia::Core::StringCRC layer, CellRingKind kind, float rangeMin, float rangeMax);
		bool Untrack(Dia::Core::StringCRC layer);	// false if unknown
		bool IsTracked(Dia::Core::StringCRC layer) const;
		uint32_t TrackedCount() const { return mLayerCount; }

		// Opens a new sample slot (evicting the oldest if full). false if sampleIndex <= LatestSample() (when
		// non-empty), no layers are tracked, or window is 0.
		bool BeginSample(uint32_t sampleIndex);
		// Writes the open sample's values for one layer. false if no open sample, unknown layer, or n != cellCount.
		bool Append(Dia::Core::StringCRC layer, const float* values, uint32_t n);

		bool Read(uint32_t sample, Dia::Core::StringCRC layer, float* out, uint32_t n) const;
		bool HasSample(uint32_t sample) const;

		// Drops all samples with index > sample.
		void TruncateAfter(uint32_t sample);

		bool Empty() const { return mCount == 0; }
		uint32_t FirstRetained() const;		// 0 when empty
		uint32_t LatestSample() const;		// 0 when empty
		uint32_t RetainedCount() const { return mCount; }
		uint32_t WindowLength() const { return mWindow; }
		uint64_t BytesPerSample() const { return mBytesPerSample; }
		uint64_t BytesUsed() const;			// allocated cell storage (window * bytesPerSample)
		double UsedFraction() const;		// retained count * bytesPerSample / budget
		uint32_t CellCount() const { return mCellCount; }

		uint32_t GapCount() const { return mGapCount; }
		CellRingGap Gap(uint32_t i) const;	// oldest first; zeroed if out of range
		uint32_t EvictionEpisodeCount() const { return mEpisodeCount; }

	private:
		struct Layer
		{
			Dia::Core::StringCRC mId;
			CellRingKind mKind = CellRingKind::Float16;
			float mMin = 0.f, mMax = 1.f;
			uint8_t* mData = nullptr;	// mWindow slots of cellCount * stride bytes
			uint32_t mFirstSample = 0;
			bool mPending = false;		// tracked, waiting for the next BeginSample to set mFirstSample
		};

		int FindLayer(Dia::Core::StringCRC id) const;
		uint32_t Stride(const Layer& l) const { return l.mKind == CellRingKind::Float16 ? 2u : 1u; }
		uint32_t SlotOf(uint32_t logical) const { return (mHead + logical) % mWindow; }
		bool FindLogical(uint32_t sample, uint32_t& logical) const;
		// Re-lays out storage for newWindow slots (retained samples beyond it are dropped, oldest first).
		void Relayout(uint32_t newWindow);
		void DropOldest(uint32_t n);
		void RecordDrop(uint32_t sample);
		void FreeAll();
		uint64_t ComputeBytesPerSample(int skipLayer) const;

		uint32_t mCellCount;
		uint64_t mBudget;
		Layer mLayers[kMaxLayers];
		uint32_t mLayerCount = 0;
		uint64_t mBytesPerSample = 0;
		uint32_t mWindow = 0;
		uint32_t* mIds = nullptr;	// sample index per slot
		uint32_t mHead = 0;
		uint32_t mCount = 0;
		bool mOpen = false;
		bool mInEpisode = false;
		uint32_t mEpisodeCount = 0;
		CellRingGap mGaps[kMaxGaps];
		uint32_t mGapCount = 0;
	};
}
