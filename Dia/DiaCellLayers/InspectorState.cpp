#include <DiaCellLayers/InspectorState.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>

using Dia::Core::StringCRC;

namespace Dia::CellLayers
{
	InspectorState::InspectorState() : mEntries(new Entry[kMaxShown]) {}
	InspectorState::~InspectorState() { delete[] mEntries; }

	void InspectorState::ClearComputed()
	{
		for (uint32_t i = 0; i < mShownCount; ++i) { mEntries[i].computed = false; }
		mHasHistogram = false;
	}

	void InspectorState::SetActive(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, StringCRC baseLayerId)
	{
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

	bool InspectorState::AnyDirty(const CellLayerRegistry& registry) const
	{
		for (uint32_t i = 0; i < mShownCount; ++i) { if (NeedsCompute(registry, mEntries[i])) { return true; } }
		return false;
	}

	bool InspectorState::Tick(const CellLayerRegistry& registry, LayerReader& reader,
		const Dia::ValueSchema::ValueSchemaService* schema, const ICellSelectionView* selection)
	{
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
			if (!NeedsCompute(registry, e)) { continue; }
			const CellLayerDesc* d = registry.Find(e.id);
			e.computed = true;
			e.seenVersion = (d != nullptr && !d->version.empty()) ? d->version() : 0;
			SelectionStats::Compute(registry, reader, schema, *selection, e.id, e.stats);
			if (e.id == mBase)
			{
				mHasHistogram = (d != nullptr && d->kind == CellValueKind::Float)
					&& SelectionStats::ComputeHistogram(registry, reader, schema, *selection, e.id, mHistogram);
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
