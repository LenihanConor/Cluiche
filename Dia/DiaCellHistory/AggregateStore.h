#pragma once

// AggregateStore: Tier-1 per-group aggregates (float: min/mean/max; category: count per enum value; group sizes).
// Pure data structure (no DiaCellLayers dependency, no STL in this header). The recorder (not this class) decides
// group membership and feeds masks / values in.
//
// Layout is declared up front: AddLayer / AddGroup. The implicit group `world` is group 0 and always exists. Caps:
//   kMaxLayers = 64, kMaxGroups = 128 (including world), kMaxEnumCount = 256, kMaxGaps = 64.
// AddLayer / AddGroup are REJECTED (return false) once the first BeginSample has been called, even if all samples
// have since been truncated or evicted: the row layout is frozen for the lifetime of the store.
//
// Row cost per sample (the budget accounting): per group 2 bytes (group size, uint16) + per float layer 12 bytes
// (3 x float32) + per category layer 2 * enumCount bytes. window = budget / rowBytes. The per-slot sample id and
// "written" flags (L+1 bytes per group) are small bookkeeping outside the budget accounting.
// Float rows do not store a non-NaN cell count (spec: 12 bytes): a group with zero non-NaN cells is stored as NaN
// min/mean/max, which is how "empty" is reported.
// Sample indices are the caller's global indices: strictly increasing, may skip.
// Unwritten (sample, layer, group) rows read back as "not recorded" (false).

#include <DiaCore/CRC/StringCRC.h>
#include <cstddef>
#include <cstdint>

namespace Dia::CellHistory
{
	enum class AggregateKind : uint8_t { Float, Category };

	struct AggregateStats
	{
		float mMin = 0.f;
		float mMean = 0.f;
		float mMax = 0.f;
	};

	struct AggregateGap
	{
		uint32_t mFirstDropped = 0;
		uint32_t mLastDropped = 0;
	};

	class AggregateStore
	{
	public:
		static constexpr uint32_t kMaxLayers = 64;
		static constexpr uint32_t kMaxGroups = 128;
		static constexpr uint32_t kMaxEnumCount = 256;
		static constexpr uint32_t kMaxGaps = 64;

		explicit AggregateStore(uint64_t budgetBytes);
		~AggregateStore();
		AggregateStore(const AggregateStore&) = delete;
		AggregateStore& operator=(const AggregateStore&) = delete;

		static Dia::Core::StringCRC WorldGroup();

		// Layout. false on duplicate, full table, bad enumCount (category: 1..256; float ignores it) or after freeze.
		bool AddLayer(Dia::Core::StringCRC id, AggregateKind kind, uint32_t enumCount = 0);
		bool AddGroup(Dia::Core::StringCRC groupId);	// false on duplicate (incl. world), full table or after freeze

		// Opens a sample (evicting the oldest if full). false if sampleIndex <= LatestSample() (when non-empty),
		// no layers, or not even one row fits the budget (logs Error).
		bool BeginSample(uint32_t sampleIndex);

		// Writes into the open (latest) sample. All return false when no sample is open or ids are unknown/mismatched.
		bool SetGroupSize(Dia::Core::StringCRC group, uint16_t cells);
		bool SetFloatLayerRow(Dia::Core::StringCRC layer, Dia::Core::StringCRC group, const AggregateStats& stats);
		// mask: 1 byte per cell, nonzero = member; null = all cells. min/mean/max over non-NaN member cells.
		bool AccumulateFloat(Dia::Core::StringCRC layer, Dia::Core::StringCRC group, const float* values, const uint8_t* mask, uint32_t cellCount);
		// values are integer enum ids stored as floats; NaN / out-of-range ids are not counted.
		bool AccumulateCategory(Dia::Core::StringCRC layer, Dia::Core::StringCRC group, const float* values, const uint8_t* mask, uint32_t cellCount);
		// Copies all group rows of `layer` from a retained earlier sample into the open sample. false if the source
		// sample is evicted/missing, the layer was not fully recorded there, or no sample is open.
		bool CopyRowFrom(uint32_t previousSample, Dia::Core::StringCRC layer);

		// Reads. false for unrecorded / evicted samples, unknown layer/group, or wrong layer kind.
		bool Aggregate(uint32_t sample, Dia::Core::StringCRC layer, Dia::Core::StringCRC group, AggregateStats& out) const;
		bool CategoryCounts(uint32_t sample, Dia::Core::StringCRC layer, Dia::Core::StringCRC group, uint16_t* out, uint32_t n) const;
		bool GroupSize(uint32_t sample, Dia::Core::StringCRC group, uint16_t& out) const;
		bool HasSample(uint32_t sample) const;

		void TruncateAfter(uint32_t sample);	// drops all samples with index > sample
		// Drops every sample, the gaps and the eviction episodes; keeps the layout (layers, groups, window). Numbering may restart at 0.
		void Clear();

		bool Empty() const { return mCount == 0; }
		uint32_t FirstRetained() const;		// 0 when empty
		uint32_t LatestSample() const;		// 0 when empty
		uint32_t RetainedCount() const { return mCount; }
		uint32_t WindowLength() const { return mWindow; }	// 0 until the first BeginSample
		uint32_t LayerCount() const { return mLayerCount; }
		uint32_t GroupCount() const { return mGroupCount; }
		uint64_t BytesPerSample() const;	// row cost for the current layout
		uint64_t BytesUsed() const;			// window * bytesPerSample (allocated)
		double UsedFraction() const;		// retained rows * bytesPerSample / budget
		uint64_t Budget() const { return mBudget; }

		uint32_t GapCount() const { return mGapCount; }
		AggregateGap Gap(uint32_t i) const;	// oldest first; zeroed if out of range
		uint32_t EvictionEpisodeCount() const { return mEpisodeCount; }

	private:
		struct Layer
		{
			Dia::Core::StringCRC mId;
			AggregateKind mKind = AggregateKind::Float;
			uint32_t mEnumCount = 0;
			size_t mOffset = 0;		// byte offset of this layer's data within a slot
		};

		int FindLayer(Dia::Core::StringCRC id) const;
		int FindGroup(Dia::Core::StringCRC id) const;
		bool FindLogical(uint32_t sample, uint32_t& logical) const;
		uint32_t SlotOf(uint32_t logical) const { return (mHead + logical) % mWindow; }
		uint8_t* SlotData(uint32_t slot) const { return mData + size_t(slot) * mSlotBytes; }
		uint8_t* SlotFlags(uint32_t slot) const { return mFlags + size_t(slot) * mFlagBytes; }
		size_t LayerBytes(const Layer& l) const;
		void Freeze();
		void DropOldest(uint32_t n);
		void RecordDrop(uint32_t sample);

		uint64_t mBudget;
		Layer mLayers[kMaxLayers];
		uint32_t mLayerCount = 0;
		Dia::Core::StringCRC mGroups[kMaxGroups];
		uint32_t mGroupCount = 0;
		bool mFrozen = false;
		size_t mSlotBytes = 0;		// row bytes (budget accounting)
		size_t mFlagBytes = 0;		// (layerCount + 1) * groupCount
		uint32_t mWindow = 0;
		uint8_t* mData = nullptr;
		uint8_t* mFlags = nullptr;
		uint32_t* mIds = nullptr;
		uint32_t mHead = 0;
		uint32_t mCount = 0;
		bool mInEpisode = false;
		uint32_t mEpisodeCount = 0;
		AggregateGap mGaps[kMaxGaps];
		uint32_t mGapCount = 0;
	};
}
