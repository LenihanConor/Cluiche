#include <DiaCellLayers/InspectorState.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>

using Dia::Core::StringCRC;

namespace Dia::CellLayers
{
	// A: an owned bitset sized to the grid at pin time, exposed as an ICellSelectionView so SelectionStats can aggregate it
	// exactly like B. The version never changes while pinned (a re-pin makes a new set).
	class InspectorState::PinnedSet : public ICellSelectionView
	{
	public:
		PinnedSet(const Dia::Graphics::CellGridDesc& grid, const ICellSelectionView& source)
			: mGrid(&grid), mTopology(grid.topology), mWidth(grid.width), mHeight(grid.height)
		{
			mCellCount = static_cast<uint32_t>(mWidth) * mHeight;
			mWordCount = (mCellCount + 63u) / 64u;
			mWords = new uint64_t[mWordCount ? mWordCount : 1u]();
			source.ForEach(&PinnedSet::CopyCell, this);
		}
		~PinnedSet() override { delete[] mWords; }
		PinnedSet(const PinnedSet&) = delete;
		PinnedSet& operator=(const PinnedSet&) = delete;

		uint32_t	Version() const override { return 1; }
		uint32_t	Count() const override { return mCount; }
		bool		Contains(uint32_t cellIndex) const override
		{
			return cellIndex < mCellCount && ((mWords[cellIndex >> 6] >> (cellIndex & 63u)) & 1u) != 0;
		}
		void		ForEach(void (*fn)(uint32_t cellIndex, void* user), void* user) const override
		{
			for (uint32_t w = 0; w < mWordCount; ++w)
			{
				for (uint64_t bits = mWords[w]; bits != 0; bits &= bits - 1)
				{
					uint32_t bit = 0;
					while (((bits >> bit) & 1u) == 0) { ++bit; }
					fn(w * 64u + bit, user);
				}
			}
		}

		// Same identity rule as CellSelection::Reset: pointer, width, height and topology id.
		bool		MatchesGrid(const Dia::Graphics::CellGridDesc* grid) const
		{
			return grid == mGrid && grid != nullptr && grid->width == mWidth && grid->height == mHeight && grid->topology == mTopology;
		}

	private:
		static void CopyCell(uint32_t cellIndex, void* user)
		{
			PinnedSet* self = static_cast<PinnedSet*>(user);
			if (cellIndex >= self->mCellCount || self->Contains(cellIndex)) { return; }
			self->mWords[cellIndex >> 6] |= uint64_t(1) << (cellIndex & 63u);
			++self->mCount;
		}

		const Dia::Graphics::CellGridDesc*	mGrid;
		StringCRC	mTopology;
		uint32_t	mWidth;
		uint32_t	mHeight;
		uint32_t	mCellCount = 0;
		uint32_t	mWordCount = 0;
		uint64_t*	mWords = nullptr;
		uint32_t	mCount = 0;
	};

	void ComputeDelta(const LayerStats& b, const LayerStats& a, LayerDelta& out)
	{
		out = LayerDelta();
		out.countDelta = static_cast<int64_t>(b.count) - static_cast<int64_t>(a.count);
		out.hasRange = b.hasRange && a.hasRange;
		if (out.hasRange)
		{
			out.minDelta = static_cast<double>(b.min) - static_cast<double>(a.min);
			out.meanDelta = b.mean - a.mean;
			out.maxDelta = static_cast<double>(b.max) - static_cast<double>(a.max);
		}
	}

	InspectorState::InspectorState() : mEntries(new Entry[kMaxShown]) {}
	InspectorState::~InspectorState()
	{
		Unpin();
		delete[] mEntries;
	}

	void InspectorState::Unpin()
	{
		delete mPinned;
		mPinned = nullptr;
		delete[] mFrozen;
		mFrozen = nullptr;
		mFrozenCount = 0;
		for (uint32_t i = 0; i < kMaxShown; ++i) { mEntries[i].aComputed = false; }
	}

	void InspectorState::CheckPinIdentity()
	{
		if (mPinned != nullptr && !mPinned->MatchesGrid(mGrid)) { Unpin(); }
	}

	uint32_t InspectorState::PinnedCount() const { return mPinned != nullptr ? mPinned->Count() : 0u; }

	bool InspectorState::Pin(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, const ICellSelectionView* selection)
	{
		(void)registry;
		if (grid == nullptr || grid != mGrid || selection == nullptr || selection->Count() == 0) { return false; }
		PinnedSet* set = new PinnedSet(*grid, *selection);
		if (set->Count() == 0) { delete set; return false; }	// every selected cell was outside the grid
		Unpin();
		mPinned = set;
		mPending = true;	// A is live: the next Tick computes it with B
		return true;
	}

	bool InspectorState::PinFreeze(const CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
		const Dia::Graphics::CellGridDesc* grid, const ICellSelectionView* selection)
	{
		if (!Pin(registry, grid, selection)) { return false; }
		const uint32_t total = registry.LayerCountForGrid(grid);
		mFrozen = new FrozenEntry[total ? total : 1u];
		for (uint32_t i = 0; i < total; ++i)
		{
			const CellLayerDesc* d = registry.LayerOfGrid(grid, i);
			if (d == nullptr) { continue; }
			mFrozen[mFrozenCount].id = d->id;
			SelectionStats::Compute(registry, reader, schema, *mPinned, d->id, mFrozen[mFrozenCount].stats);
			++mFrozenCount;
		}
		return true;
	}

	const LayerStats* InspectorState::StatsForA(StringCRC layerId) const
	{
		if (mPinned == nullptr) { return nullptr; }
		if (mFrozen != nullptr)
		{
			for (uint32_t i = 0; i < mFrozenCount; ++i) { if (mFrozen[i].id == layerId) { return &mFrozen[i].stats; } }
			return nullptr;
		}
		for (uint32_t i = 0; i < mShownCount; ++i)
		{
			if (mEntries[i].id == layerId) { return mEntries[i].aComputed ? &mEntries[i].aStats : nullptr; }
		}
		return nullptr;
	}

	bool InspectorState::FrozenAvailable(StringCRC layerId) const
	{
		if (mPinned == nullptr || mFrozen == nullptr) { return false; }
		for (uint32_t i = 0; i < mFrozenCount; ++i) { if (mFrozen[i].id == layerId) { return true; } }
		return false;
	}

	void InspectorState::ClearComputed()
	{
		for (uint32_t i = 0; i < mShownCount; ++i) { mEntries[i].computed = false; }
		mHasHistogram = false;
	}

	void InspectorState::SetActive(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, StringCRC baseLayerId)
	{
		if (grid != mGrid) { Unpin(); }	// A belongs to one grid
		if (grid == mGrid && baseLayerId == mBase) { return; }
		const CellLayerDesc* base = baseLayerId != StringCRC::kZero ? registry.Find(baseLayerId) : nullptr;
		Dia::Core::Containers::String32 group;
		if (base != nullptr) { group = base->group; }
		if (grid != mGrid)
		{
			mShownCount = 0;
			mHasHistogram = false;
			mSelSeen = false;
		}
		if (grid != mGrid || !(group == mActiveGroup))
		{
			mExpandedCount = 0;
			mActiveGroup = group;
			if (base != nullptr) { mExpanded[mExpandedCount++] = group; }
		}
		mGrid = grid;
		mBase = baseLayerId;
		mShownStale = true;
		mPending = true;
	}

	void InspectorState::SetGroupExpanded(const char* group, bool expanded)
	{
		if (group == nullptr) { return; }
		for (uint32_t i = 0; i < mExpandedCount; ++i)
		{
			if (mExpanded[i] == group)
			{
				if (expanded) { return; }
				mExpanded[i] = mExpanded[--mExpandedCount];
				mShownStale = true;
				mPending = true;
				return;
			}
		}
		if (!expanded || mExpandedCount >= kMaxGroups) { return; }
		mExpanded[mExpandedCount++] = Dia::Core::Containers::String32(group);
		mShownStale = true;
		mPending = true;
	}

	bool InspectorState::IsGroupExpanded(const char* group) const
	{
		if (group == nullptr) { return false; }
		for (uint32_t i = 0; i < mExpandedCount; ++i) { if (mExpanded[i] == group) { return true; } }
		return false;
	}

	void InspectorState::RebuildShown(const CellLayerRegistry& registry)
	{
		mShownStale = false;
		StringCRC ids[kMaxShown];
		uint32_t n = 0;
		if (mGrid != nullptr)
		{
			if (mBase != StringCRC::kZero && registry.Find(mBase) != nullptr) { ids[n++] = mBase; }
			const uint32_t total = registry.LayerCountForGrid(mGrid);
			for (uint32_t i = 0; i < total && n < kMaxShown; ++i)
			{
				const CellLayerDesc* d = registry.LayerOfGrid(mGrid, i);
				if (d == nullptr || d->id == mBase) { continue; }
				if (IsGroupExpanded(d->group.AsCStr())) { ids[n++] = d->id; }
			}
		}
		Entry* kept = new Entry[kMaxShown];
		for (uint32_t i = 0; i < n; ++i)
		{
			kept[i].id = ids[i];
			for (uint32_t j = 0; j < mShownCount; ++j)
			{
				if (mEntries[j].id == ids[i]) { kept[i] = mEntries[j]; break; }
			}
		}
		for (uint32_t i = 0; i < n; ++i) { mEntries[i] = kept[i]; }
		delete[] kept;
		mShownCount = n;
	}

	bool InspectorState::NeedsCompute(const CellLayerRegistry& registry, const Entry& e) const
	{
		if (!e.computed) { return true; }
		const CellLayerDesc* d = registry.Find(e.id);
		return d != nullptr && !d->version.empty() && d->version() != e.seenVersion;
	}

	bool InspectorState::NeedsComputeA(const CellLayerRegistry& registry, const Entry& e) const
	{
		if (mPinned == nullptr || mFrozen != nullptr) { return false; }	// frozen A is never recomputed
		if (!e.aComputed) { return true; }
		const CellLayerDesc* d = registry.Find(e.id);
		return d != nullptr && !d->version.empty() && d->version() != e.aSeenVersion;
	}

	bool InspectorState::AnyDirty(const CellLayerRegistry& registry) const
	{
		for (uint32_t i = 0; i < mShownCount; ++i)
		{
			if (NeedsCompute(registry, mEntries[i]) || NeedsComputeA(registry, mEntries[i])) { return true; }
		}
		return false;
	}

	bool InspectorState::Tick(const CellLayerRegistry& registry, LayerReader& reader,
		const Dia::ValueSchema::ValueSchemaService* schema, const ICellSelectionView* selection)
	{
		CheckPinIdentity();
		if (mShownStale) { RebuildShown(registry); }
		const bool hadPending = mPending;

		if (selection == nullptr || selection->Count() == 0)
		{
			if (mHasSelection || mSelSeen) { ClearComputed(); }
			mHasSelection = false;
			mSelSeen = false;
			mPending = false;
			return false;
		}
		mHasSelection = true;
		if (!mSelSeen || selection->Version() != mSelVersion)
		{
			ClearComputed();	// selection changed: every shown layer is dirty
			mSelSeen = true;
			mSelVersion = selection->Version();
			mPending = true;
		}
		else if (!mPending && AnyDirty(registry))
		{
			mPending = true;
		}
		if (!hadPending) { return false; }	// detected this frame: the pass runs next Tick

		DIA_TRACE_ZONE("celllayers.inspector.pass", ::Dia::Observation::Trace::Category::kDiaGraphics);
		DIA_PROFILE_SCOPE("celllayers.inspector.pass", ::Dia::Observation::Profile::Category::kDiaGraphics);
		uint32_t computed = 0;
		for (uint32_t i = 0; i < mShownCount; ++i)
		{
			Entry& e = mEntries[i];
			const bool needB = NeedsCompute(registry, e);
			const bool needA = NeedsComputeA(registry, e);
			if (!needB && !needA) { continue; }
			const CellLayerDesc* d = registry.Find(e.id);
			const uint64_t version = (d != nullptr && !d->version.empty()) ? d->version() : 0;
			if (needB)
			{
				e.computed = true;
				e.seenVersion = version;
				SelectionStats::Compute(registry, reader, schema, *selection, e.id, e.stats);
				if (e.id == mBase)
				{
					mHasHistogram = (d != nullptr && d->kind == CellValueKind::Float)
						&& SelectionStats::ComputeHistogram(registry, reader, schema, *selection, e.id, mHistogram);
				}
			}
			if (needA)
			{
				e.aComputed = true;
				e.aSeenVersion = version;
				SelectionStats::Compute(registry, reader, schema, *mPinned, e.id, e.aStats);
			}
			++computed;
		}
		mPending = false;
		if (computed == 0) { return false; }
		++mPassCount;
		mLastPassLayerCount = computed;
		return true;
	}

	const LayerStats* InspectorState::StatsFor(StringCRC layerId) const
	{
		if (!mHasSelection) { return nullptr; }
		for (uint32_t i = 0; i < mShownCount; ++i)
		{
			if (mEntries[i].id == layerId) { return mEntries[i].computed ? &mEntries[i].stats : nullptr; }
		}
		return nullptr;
	}

	StringCRC InspectorState::ShownLayerAt(uint32_t index) const
	{
		return index < mShownCount ? mEntries[index].id : StringCRC::kZero;
	}
}
